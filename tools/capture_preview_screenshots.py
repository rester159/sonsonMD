#!/usr/bin/env python3
"""Capture the current ROM without an emulator frontend; gameplay refills lives."""
from pathlib import Path
import subprocess,re,os,json,hashlib
r=Path(__file__).resolve().parents[1];digest=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest();o=r/'out/screenshots'/digest[:8];o.mkdir(parents=True,exist_ok=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)(?:\.lto_priv\.\d+)?\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'tools/verify_libretro.c').read_text();s=s.replace('static unsigned frame, sample, players;', 'static unsigned frame, sample, players,pixel_format;')
s=s.replace('if(cmd==9 || cmd==31)', 'if(cmd==10){pixel_format=*(unsigned*)data;return true;}\n    if(cmd==9 || cmd==31)')
callback=r'''static void video(const void*d,unsigned w,unsigned h,size_t pitch){
 static unsigned shot;const char*dir=getenv("CAPTURE_DIR");if(!dir||!d)return;
 unsigned target=0;const char*menu=getenv("CAPTURE_MENU");
 if(menu){if(frame!=800&&frame!=949&&frame!=1150)return;target=frame;}
 else {static const unsigned ticks[]={1000,8000,16000,24000,32000,40000,48000};
  if(!started||shot>=7||r32(input_ram,input_fc)-start_tick<ticks[shot])return;target=ticks[shot++];}
 char path[2048];snprintf(path,sizeof path,"%s/frame-%u.ppm",dir,target);FILE*f=fopen(path,"wb");if(!f)return;
 fprintf(f,"P6\n%u %u\n255\n",w,h);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
  unsigned v=pixel_format==1?((const uint32_t*)((const uint8_t*)d+y*pitch))[x]:((const uint16_t*)((const uint8_t*)d+y*pitch))[x];uint8_t rgb[3];
  if(pixel_format==1){rgb[0]=v>>16;rgb[1]=v>>8;rgb[2]=v;}
  else if(pixel_format==2){rgb[0]=((v>>11)&31)*255/31;rgb[1]=((v>>5)&63)*255/63;rgb[2]=(v&31)*255/31;}
  else{rgb[0]=((v>>10)&31)*255/31;rgb[1]=((v>>5)&31)*255/31;rgb[2]=(v&31)*255/31;}
  fwrite(rgb,1,3,f);
 }fclose(f);
}'''
s=s.replace('static void video(const void*d,unsigned w,unsigned h,size_t p) {(void)d;(void)w;(void)h;(void)p;}',callback)
(o/'game.c').write_text(s);subprocess.run(['cc','-O2',str(o/'game.c'),'-o',str(o/'game')],check=True)
a=s.index('static int16_t input(');b=s.index('#define FN',a)
menu=s[:a]+'''static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;if(port)return 0;
 if(atoi(getenv("CAPTURE_MENU"))==1)return id==3&&frame>=850&&frame<862;
 return (id==5&&frame>=900&&frame<912)||(id==3&&((frame>=940&&frame<952)||(frame>=1080&&frame<1092)))||(id==4&&frame>=1040&&frame<1052);
}
'''+s[b:s.index('int main(int argc,char**argv)')]
init=s[s.index('    void*core=dlopen'):s.index('    FN(void,retro_get_system_av_info')]
menu+='\nint main(int argc,char**argv){if(argc!=3)return 2;\n'+init+'\nfor(frame=0;frame<1300;frame++)retro_run();return 0;}\n'
(o/'menu.c').write_text(menu);subprocess.run(['cc','-O2',str(o/'menu.c'),'-o',str(o/'menu')],check=True)
for mode in (1,2):
 d=o/f'menu-{mode}';d.mkdir(exist_ok=True);env=os.environ.copy();env.update(CAPTURE_DIR=str(d),CAPTURE_MENU=str(mode))
 subprocess.run([str(o/'menu'),str(core),str(r/'out/rom.bin')],env=env,check=True)
for players in (1,2):
 d=o/f'{players}p';d.mkdir(exist_ok=True);env=os.environ.copy();env.update(CAPTURE_DIR=str(d),SONSON_SIM_INPUT='1',SONSON_FIXED_TICKS='1')
 cmd=[str(o/'game'),str(core),str(r/'out/rom.bin'),str(players),'48100',*[syms[n]for n in ('g_frameCounter','g_presentCounter','g_levelW','g_game','g_numPlayers','g_attract')],str(d/'trace.csv')]
 with (d/'run.log').open('w')as log:subprocess.run(cmd,env=env,check=True,stdout=log,stderr=subprocess.STDOUT)
 assert len(list(d.glob('*.ppm')))==7
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==digest
(o/'manifest.json').write_text(json.dumps({'rom_sha256':digest,'core_sha256':hashlib.sha256(core.read_bytes()).hexdigest(),'scope':'Unmodified video output. Menus use controller input only; gameplay uses the reproducible full-course workload with lives refilled. Preview captures, not release qualification.'},indent=2)+'\n');print(o)

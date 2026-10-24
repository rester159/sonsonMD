#!/usr/bin/env python3
"""Fresh-process Genesis Plus GX SRAM checks; explicit fixtures, no frontend."""
from pathlib import Path
import re,subprocess,json,hashlib,zlib
r=Path(__file__).resolve().parents[1];o=r/'out/sram-windowless';o.mkdir(exist_ok=True)
(o/'smoke.srm').unlink(missing_ok=True)
(o/'report.json').unlink(missing_ok=True)
s=(r/'tools/verify_libretro.c').read_text();base=s[:s.index('int main(int argc,char**argv)')];a=base.index('static int16_t input(');b=base.index('#define FN',a)
base=base[:a]+'''static unsigned test_stage;
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;if(port)return 0;
 if(test_stage>=2){
  if(id==5&&frame>=900&&frame<912)return 1;
  if(id==3&&frame>=940&&frame<952)return 1;
  if(test_stage==2)return (id==4&&frame>=1040&&frame<1052)||(id==3&&((frame>=1080&&frame<1092)||(frame>=1200&&frame<1212)));
  return test_stage>=4&&id==3&&frame>=1040&&frame<1052;
 }
 return id==3 && ((frame>=850&&frame<862)||(frame>=1000&&frame<1012));
}
'''+base[b:]
init=s[s.index('    void*core=dlopen'):s.index('    FN(void,retro_get_system_av_info')]
code=base+'\nint main(int argc,char**argv){if(argc!=5)return 2;test_stage=atoi(argv[4]);\n'+init+'''
 FN(size_t,retro_get_memory_size,(unsigned));FN(void,retro_unload_game,(void));FN(void,retro_deinit,(void));
 uint8_t*ram=retro_get_memory_data(2),*sr=retro_get_memory_data(0);size_t n=retro_get_memory_size(0);
 if(!ram||!sr||!n)return 3;
 FILE *sf=fopen(argv[3],"rb");if(sf){if(fread(sr,1,n,sf)!=n)return 4;fclose(sf);}
 for(frame=0;frame<(test_stage==5?2500:1400);frame++){
  if(frame==950 && atoi(argv[4])==0){w8(ram,CONFIG,0xdd);w8(ram,CONFIG+1,0xa9);}
  if(frame==1150 && test_stage==2){w8(ram,CONFIG,0xff);w8(ram,CONFIG+1,0xe9);w8(ram,CONFIG+7,9);}
  if(frame==1100 && test_stage==5){
   unsigned score=234567;for(unsigned k=0;k<4;k++)w8(ram,GAME+28+k,score>>(24-k*8));
   w8(ram,GAME+32,1);w8(ram,GAME+38,1);w8(ram,HC,0);
  }
  retro_run();
  if(frame==500||frame==949||frame==1000||frame==1080||frame==1280||frame==1399||frame==2499)printf("STATE %u status=%u config=%02x,%02x credits=%u best=%u\\n",frame,r8(ram,STATUS),r8(ram,CONFIG),r8(ram,CONFIG+1),r8(ram,CONFIG+7),r32(ram,BEST));
 }
 sf=fopen(argv[3],"wb");if(!sf)return 5;fwrite(sr,1,n,sf);fclose(sf);printf("SRAM %zu\\n",n);
 retro_unload_game();retro_deinit();return 0;
}
'''
syms={name:int(addr,16) for addr,name in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)(?:\.lto_priv\.\d+)?\s*$',(r/'out/symbol.txt').read_text(),re.M)}
for token,name in [('CONFIG','g_config'),('STATUS','g_saveStatus'),('BEST','run_best_score'),('GAME','g_game'),('HC','g_homeCredits')]:code=re.sub(r'\b'+token+r'\b',str(syms[name]),code)
(o/'smoke.c').write_text(code);subprocess.run(['cc','-O2',str(o/'smoke.c'),'-o',str(o/'smoke')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'

rom_hash=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()
reports=[]
def run(stage,path,label):
 result=subprocess.run([str(o/'smoke'),str(core),str(r/'out/rom.bin'),str(path),str(stage)],check=True,capture_output=True,text=True)
 (o/(label+'.log')).write_text(result.stdout);print(label,result.stdout,flush=True)
 states={int(f):{'status':int(status),'dsw1':int(a,16),'dsw2':int(b,16),'credits':int(c),'best':int(best)} for f,status,a,b,c,best in re.findall(r'STATE (\d+) status=(\d+) config=([0-9a-f]+),([0-9a-f]+) credits=(\d+) best=(\d+)',result.stdout)}
 assert 1399 in states
 reports.append({'label':label,'states':states});return states
path=o/'smoke.srm'
s=run(0,path,'arcade-save');assert s[500]['status']==0 and s[1080]['status']==2
s=run(1,path,'arcade-reload');assert s[500]['status']==1 and (s[949]['dsw1'],s[949]['dsw2'])==(0xdd,0xa9)
s=run(2,path,'home-save');assert s[1280]['status']==2
s=run(3,path,'home-reload');assert (s[949]['dsw1'],s[949]['dsw2'],s[949]['credits'])==(0xff,0xe9,9)
s=run(1,path,'arcade-preserved');assert (s[949]['dsw1'],s[949]['dsw2'])==(0xdd,0xa9)
# Independent on-disk fixture, verified through the production game's load path.
raw=bytearray(path.read_bytes());logical=bytearray(raw[1::2]);assert logical[:4]==b'SSNS'
latest=max((0,512),key=lambda off:int.from_bytes(logical[off+8:off+12],'big'))
slot=bytearray(logical[latest:latest+512]);assert slot[511]==0xa5
slot[17:19]=(0x94e9).to_bytes(2,'big');slot[19:23]=(123456).to_bytes(4,'big')
slot[507:511]=(zlib.crc32(slot[:507])&0xffffffff).to_bytes(4,'big')
logical[latest:latest+512]=slot;raw[1::2]=logical;scorepath=o/'score.srm';scorepath.write_bytes(raw)
s=run(4,scorepath,'score-load');assert s[1280]['best']==123456,s
s=run(5,scorepath,'score-write');assert s[2499]['status']==2,s
s=run(4,scorepath,'score-reload');assert s[1280]['best']==234567,s
bad=bytearray(path.read_bytes());bad[(latest+511)*2+1]=0
badpath=o/'one-slot-damaged.srm';badpath.write_bytes(bad)
s=run(3,badpath,'slot-fallback');assert s[500]['status']==1 and (s[949]['dsw1'],s[949]['dsw2'],s[949]['credits'])==(0xdf,0xcb,3),s
bad[511*2+1]=bad[1023*2+1]=0;badpath=o/'both-slots-damaged.srm';badpath.write_bytes(bad)
s=run(3,badpath,'default-recovery');assert s[500]['status']==0 and (s[949]['dsw1'],s[949]['dsw2'],s[949]['credits'])==(0xdf,0xcb,3),s
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==rom_hash
report={'passed':True,'rom_sha256':rom_hash,'core_sha256':hashlib.sha256(core.read_bytes()).hexdigest(),'runs':reports,'scope':'Windowless production ROM, fresh core process per stage. Controller-driven Arcade/Home menu commits with explicit configuration RAM fixtures; independent valid score SRAM fixture, forced final-life Home score write and new-process reload; damaged-slot fallback/default recovery. Does not prove flashcart persistence, ranking-name UI, survival-time writes or physical hardware.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n')

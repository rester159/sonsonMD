#!/usr/bin/env python3
"""Windowless target attract smoke test: controller-only boot, read-only RAM."""
from pathlib import Path
import subprocess,re,hashlib,json,csv,os,argparse
p=argparse.ArgumentParser();p.add_argument("--scenario",choices=("idle","credit-idle","prelude-start","demo-p2","rank-two"),default="idle");args=p.parse_args()
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/title-connected'/args.scenario;o.mkdir(parents=True,exist_ok=True)
core=Path(os.environ.get('GENPLUS_CORE',str(Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')))
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)(?:\.lto_priv\.\d+)?\s*$',(r/'out/symbol.txt').read_text(),re.M)}
base=(r/'tools/verify_libretro.c').read_text().split('int main(int argc,char**argv)')[0]
# Save bounded visual evidence without a frontend or graphics device.
base=base.replace('static unsigned frame, sample, players;', 'static unsigned frame, sample, players, pixel_format;')
base=base.replace('if(cmd==9 || cmd==31)', 'if(cmd==10){pixel_format=*(unsigned*)data;return true;}\n    if(cmd==9 || cmd==31)')
callback=r'''static void video(const void*d,unsigned w,unsigned h,size_t pitch) {
 if(!d || frame%400!=0 || frame<800)return;
 char path[2048];snprintf(path,sizeof(path),"CAPTURE_DIR/frame-%u.ppm",frame);
 FILE*f=fopen(path,"wb");if(!f)return;fprintf(f,"P6\n%u %u\n255\n",w,h);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
  unsigned v=pixel_format==1?((const uint32_t*)((const uint8_t*)d+y*pitch))[x]:((const uint16_t*)((const uint8_t*)d+y*pitch))[x];
  uint8_t rgb[3];
  if(pixel_format==1){rgb[0]=v>>16;rgb[1]=v>>8;rgb[2]=v;}
  else if(pixel_format==2){rgb[0]=((v>>11)&31)*255/31;rgb[1]=((v>>5)&63)*255/63;rgb[2]=(v&31)*255/31;}
  else{rgb[0]=((v>>10)&31)*255/31;rgb[1]=((v>>5)&31)*255/31;rgb[2]=(v&31)*255/31;}
  fwrite(rgb,1,3,f);
 }fclose(f);
}'''.replace('CAPTURE_DIR',str(o))
base=base.replace('static void video(const void*d,unsigned w,unsigned h,size_t p) {(void)d;(void)w;(void)h;(void)p;}',callback)
a=base.index('static int16_t input(');b=base.index('#define FN',a)
base=base[:a]+'''static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id) {
 (void)device;(void)index;
 if(frame<740)return port==0 && id==3 && frame%24<12;
 SCENARIO_INPUT
 return 0;
}
'''+base[b:]
rules={"idle":"", "credit-idle":"if(port==0)return id==8 && frame>=1000 && frame<1012;", "prelude-start":"if(port==0)return (id==8 && frame>=1000 && frame<1012)||(id==3 && frame>=1100 && frame<1120);", "demo-p2":"if(port==1)return (id==8 && frame>=2000 && frame<2012)||(id==3 && frame>=2100 && frame<2120);", "rank-two":"if(port<2)return (id==8 && frame>=3300 && frame<3312)||(id==3 && frame>=3400 && frame<3420);"}
base=base.replace('SCENARIO_INPUT',rules[args.scenario])
code=base+''' 
int main(int argc,char**argv){
 if(argc!=4)return 2;
 void *core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(Environment));FN(void,retro_set_video_refresh,(void(*)(const void*,unsigned,unsigned,size_t)));
 FN(void,retro_set_audio_sample,(void(*)(int16_t,int16_t)));FN(void,retro_set_audio_sample_batch,(size_t(*)(const int16_t*,size_t)));
 FN(void,retro_set_input_poll,(void(*)(void)));FN(void,retro_set_input_state,(int16_t(*)(unsigned,unsigned,unsigned,unsigned)));
 FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct Game*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));
 FN(unsigned,m68k_get_reg,(int));
 FN(void,retro_unload_game,(void));FN(void,retro_deinit,(void));
 retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 FILE *f=fopen(argv[2],"rb");if(!f)return 2;fseek(f,0,SEEK_END);long len=ftell(f);rewind(f);void *rom=malloc(len);if(fread(rom,1,len,f)!=(size_t)len)return 2;fclose(f);
 struct Game game={argv[2],rom,len,NULL};if(!retro_load_game(&game))return 2;
 uint8_t *ram=retro_get_memory_data(2);if(!ram)return 2;
 f=fopen(argv[3],"w");if(!f)return 2;
 fputs("refresh,pc,attract,tick,clock,demo_end,players,cursor0,cursor1,input0,input1,previous0,previous1,delay0,delay1,state,phase,credits,startmask\\n",f);
 for(frame=0;frame<14000;frame++){
  retro_run();
  fprintf(f,"%u,%u,%u,%u,%u,%u,%u",frame,m68k_get_reg(16),r8(ram,ATTR),r32(ram,TICK),r8(ram,CLOCK),r8(ram,END),r8(ram,GAME+84));
  fprintf(f,",%u,%u",r16(ram,STATE),r16(ram,STATE+2));
  for(unsigned k=4;k<10;k++)fprintf(f,",%u",r8(ram,STATE+k));
  fprintf(f,",%u,%u,%u,%u",r8(ram,TSTATE),r8(ram,TPHASE),r16(ram,CREDITS),r8(ram,STARTMASK));
  fputc('\\n',f);
 }
 fclose(f);retro_unload_game();retro_deinit();free(rom);dlclose(core);return 0;
}
'''
for token,name in [('ATTR','g_attract'),('TICK','g_frameCounter'),('CLOCK','wave_clock'),('GAME','g_game'),('STATE','attract_input'),('END','demo_end'),('TSTATE','arcade_title_state'),('TPHASE','arcade_title_phase'),('CREDITS','g_arcadeCredits'),('STARTMASK','g_startPlayers')]:code=re.sub(r'\b'+token+r'\b',str(syms[name]),code)
(o/'harness.c').write_text(code)
subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [r/'out/rom.bin',r/'out/symbol.txt',core]}
subprocess.run([str(o/'harness'),str(core),str(r/'out/rom.bin'),str(o/'trace.csv')],check=True,stdout=(o/'log.txt').open('w'),stderr=subprocess.STDOUT)
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items())
rows=[{k:int(v) for k,v in x.items()} for x in csv.DictReader((o/'trace.csv').open())]
transitions=[]
for row in rows:
 if row['refresh']<800:continue
 state=(row['state'],row['phase'])
 if not transitions or state!=tuple(transitions[-1]['state']):
  transitions.append({'refresh':row['refresh'],'state':list(state)})
report={'hashes':hashes,'transitions':transitions,'scenario':args.scenario,'scope':'Controller-only production lifecycle; windowless core, 14000 refreshes. Checks state visits and paid starts, not exact original queue timing or pixel equivalence.'}
if args.scenario in ('idle','credit-idle'):
 assert sum(t['state']==[4,2] for t in transitions)>=2, 'Individual rankings not reached twice'
 assert sum(t['state']==[4,4] for t in transitions)>=2, 'Combined rankings not reached twice'
 assert sum(t['state']==[1,4] for t in transitions)>=3, 'Prelude cycles missing'
 if args.scenario=='credit-idle':
  assert any(t['state']==[5,2] for t in transitions), 'Credited screen missing'
  assert all(x['credits']==1 for x in rows if x['refresh']>1100), 'Idle credit lost'
else:
 start={'prelude-start':1100,'demo-p2':2100,'rank-two':3400}[args.scenario]
 expected={'prelude-start':1,'demo-p2':2,'rank-two':3}[args.scenario]
 paid=[x for x in rows if start+50<x['refresh']<start+150 and not x['attract']]
 assert len(paid)>80 and all(x['startmask']==expected and x['credits']==0 for x in paid), paid[:3]
 assert paid[-1]['tick']>paid[0]['tick'], 'Paid game not advancing'
 print('PAID START PASS',args.scenario,expected)
report['passed']=True
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

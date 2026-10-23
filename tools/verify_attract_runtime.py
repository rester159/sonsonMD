#!/usr/bin/env python3
"""Windowless target attract smoke test: controller-only boot, read-only RAM."""
from pathlib import Path
import subprocess,re,hashlib,json,csv,os
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/attract-runtime';o.mkdir(parents=True,exist_ok=True)
core=Path(os.environ.get('GENPLUS_CORE',str(Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')))
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
base=(r/'tools/verify_libretro.c').read_text().split('int main(int argc,char**argv)')[0]
a=base.index('static int16_t input(');b=base.index('#define FN',a)
base=base[:a]+'''static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id) {
 (void)device;(void)index;
 return port==0 && id==3 && frame<740 && frame%24<12;
}
'''+base[b:]
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
 fputs("refresh,pc,attract,tick,clock,demo_end,players,cursor0,cursor1,input0,input1,previous0,previous1,delay0,delay1\\n",f);
 for(frame=0;frame<10000;frame++){
  retro_run();
  fprintf(f,"%u,%u,%u,%u,%u,%u,%u",frame,m68k_get_reg(16),r8(ram,ATTR),r32(ram,TICK),r8(ram,CLOCK),r8(ram,END),r8(ram,GAME+84));
  fprintf(f,",%u,%u",r16(ram,STATE),r16(ram,STATE+2));
  for(unsigned k=4;k<10;k++)fprintf(f,",%u",r8(ram,STATE+k));
  fputc('\\n',f);
 }
 fclose(f);retro_unload_game();retro_deinit();free(rom);dlclose(core);return 0;
}
'''
for token,name in [('ATTR','g_attract'),('TICK','g_frameCounter'),('CLOCK','wave_clock'),('GAME','g_game'),('STATE','attract_input'),('END','demo_end')]:code=re.sub(r'\b'+token+r'\b',str(syms[name]),code)
(o/'harness.c').write_text(code)
subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [r/'out/rom.bin',r/'out/symbol.txt',core]}
subprocess.run([str(o/'harness'),str(core),str(r/'out/rom.bin'),str(o/'trace.csv')],check=True,stdout=(o/'log.txt').open('w'),stderr=subprocess.STDOUT)
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items())
rows=[{k:int(v) for k,v in x.items()} for x in csv.DictReader((o/'trace.csv').open())]
# Compare target sequencer transitions directly with original ROM input data,
# Only display-wait PCs are coherent: a refresh can interrupt game_update
# between the tick increment, clock increment and script dispatch. Compare every
# intervening source step between those coherent snapshots.
rom=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
data=rom[0x9a6e-0x4000:0x9bd2-0x4000]
symbols=sorted((int(v,16),n) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+[tT]\s+(\S+)\s*$',(r/'out/symbol.txt').read_text(),re.M))
wait_ranges=[(addr,symbols[i+1][0]) for i,(addr,name) in enumerate(symbols[:-1]) if name.startswith('VDP_waitVBlank') or name=='VDP_waitVSync']
assert len(wait_ranges)==2
checks=0;exits=0;entries=0;skipped=0;ending=0;prev=None;active=False
for row in rows:
 if row['attract'] and not active:entries+=1;prev=None
 if active and not row['attract']:exits+=1;prev=None
 active=bool(row['attract'])
 if not active:continue
 if row['demo_end']:
  ending+=1;prev=None;continue
 if not any(lo<=row['pc']<hi for lo,hi in wait_ranges):skipped+=1;continue
 if prev is None:prev=row;continue
 delta=row['tick']-prev['tick']
 if delta==0:continue
 assert 0<delta<=2*(row['refresh']-prev['refresh'])+2 and row['players']==2,(row,prev)
 assert row['clock']==(prev['clock']+delta)&255,(row,prev)
 expected=dict(prev)
 for step in range(delta):
  clock=(prev['clock']+step+1)&255
  for p in range(2):
   expected[f'previous{p}']=expected[f'input{p}']
   if expected[f'delay{p}']:expected[f'delay{p}']-=1
   else:
    cursor=expected[f'cursor{p}'];assert cursor+1<len(data)
    expected[f'input{p}'],expected[f'delay{p}']=data[cursor:cursor+2]
    expected[f'cursor{p}']+=2
   if clock&7==0:expected[f'input{p}']|=1
 for key in ('cursor0','cursor1','input0','input1','previous0','previous1','delay0','delay1'):
  assert row[key]==expected[key],(row['refresh'],key,row[key],expected[key])
 checks+=delta;prev=row
assert checks>1900 and exits>=1 and entries>=2,(checks,exits,entries)
report={'hashes':hashes,'source_updates_checked':checks,'ending_snapshots_excluded':ending,'snapshots_outside_wait_skipped':skipped,'coherent_wait_ranges':wait_ranges,'demo_entries':entries,'demo_exits':exits,'harness_sha256':hashlib.sha256((o/'harness.c').read_bytes()).hexdigest(),'trace_sha256':hashlib.sha256((o/'trace.csv').read_bytes()).hexdigest(),'scope':'Controller-only cold boot then idle; read-only RAM snapshots; actual target sequencer versus source ROM bytes. No life refills, no RAM injection. Not full source lifecycle, graphics/audio or performance parity.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

#!/usr/bin/env python3
"""Windowless post-game controls/capture; score and final death are fixtures."""
from pathlib import Path
import csv,hashlib,json,re,subprocess,zlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/postgame-runtime';o.mkdir(parents=True,exist_ok=True)
prior=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'out/parity-audit/attract-runtime/harness.c').read_text()
s=s.replace('static bool environment(', 'static unsigned pixel_format,scenario;\nstatic bool environment(')
s=s.replace('    if(cmd==9', '    if(cmd==10){pixel_format=*(unsigned*)data;return true;}\n    if(cmd==9')
a=s.index('static void video(');b=s.index('static void audio(',a)
s=s[:a]+'''static void video(const void*d,unsigned w,unsigned h,size_t pitch){
 if(!d || scenario || !(frame==2100||frame==2300||frame==2500))return;
 char path[1024];snprintf(path,sizeof(path),CAPTURE "/frame-%u.ppm",frame);
 FILE *f=fopen(path,"wb");if(!f)exit(4);fprintf(f,"P6\\n%u %u\\n255\\n",w,h);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
  unsigned v=pixel_format==1?((uint32_t*)((uint8_t*)d+y*pitch))[x]:((uint16_t*)((uint8_t*)d+y*pitch))[x];
  unsigned char rgb[3];
  if(pixel_format==1){rgb[0]=v>>16;rgb[1]=v>>8;rgb[2]=v;}
  else if(pixel_format==2){rgb[0]=((v>>11)&31)*255/31;rgb[1]=((v>>5)&63)*255/63;rgb[2]=(v&31)*255/31;}
  else{rgb[0]=((v>>10)&31)*255/31;rgb[1]=((v>>5)&31)*255/31;rgb[2]=(v&31)*255/31;}
  fwrite(rgb,1,3,f);
 }fclose(f);
}
'''.replace('CAPTURE',json.dumps(str(o)))+s[b:]
a=s.index('static int16_t input(');b=s.index('#define FN',a)
s=s[:a]+'''static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;
 if(port && scenario!=2)return 0;
 if(port && frame<2000)return frame>=1100&&frame<1112&&(id==3||id==8);
 if(frame<740)return id==3 && frame%24<12;
 if(frame>=1000&&frame<1012)return id==3||id==8;
 if(frame>=1060&&frame<1072)return id==8;
 if(scenario==4)return 0;
 if(scenario==1)return frame>=2110&&frame<2122&&id==3;
 if((frame>=2050&&frame<2058)||(frame>=2080&&frame<2088))return id==7;
 if((frame>=2110&&frame<2118)||(frame>=2140&&frame<2148)||(frame>=2170&&frame<2178)||(frame>=2200&&frame<2208))return id==0;
 return 0;
}
'''+s[b:]
s=s.replace('if(argc!=4)return 2;','if(argc!=5 && argc!=6)return 2;scenario=atoi(argv[4]);').replace('frame<10000','frame<(scenario==4?6500:2800)')
s=s.replace(' uint8_t *ram=retro_get_memory_data(2);', '''
 if(argc==6){FILE *saved=fopen(argv[5],"rb");uint8_t *battery=retro_get_memory_data(0);
 if(!saved||!battery||fread(battery,1,2048,saved)!=2048)return 5;fclose(saved);}
 uint8_t *ram=retro_get_memory_data(2);''')
s=s.replace('  retro_run();',f'''  if(frame==2000){{
    w8(ram,{syms['g_game']+34},3);w8(ram,{syms['g_game']+76},3);
    w8(ram,{syms['g_game']+32},0);w8(ram,{syms['g_game']+74},0);
    for(unsigned i=0;i<4;i++)w8(ram,{syms['g_game']+28}+i,50000u>>(24-8*i));
    if(scenario==2)for(unsigned i=0;i<4;i++)w8(ram,{syms['g_game']+70}+i,60000u>>(24-8*i));
    w8(ram,{syms['g_gameOverBeat']},0);w8(ram,{syms['g_gameOverBeat']+1},1);
  }}
  retro_run();''')
s=s.replace('delay0,delay1\\n','delay0,delay1,phase,credits,lives,save_status,pacing_owner,pacing_phase,pacing_pending\\n')
s=s.replace("  fputc('\\n',f);",f'''  fprintf(f,",%u,%u,%u,%u,%u,%u,%u",r8(ram,{syms['arcade_postgame_phase']}),r16(ram,{syms['g_arcadeCredits']}),r8(ram,{syms['g_game']+32}),r8(ram,{syms['g_saveStatus']}),r32(ram,{syms['gameplay_pacing']}),r32(ram,{syms['session_pacing']}),r32(ram,{syms['session_pacing']+4}));
  fputc('\\n',f);''')
# Dump physical interleaved SRAM bytes, covering both 512-byte logical slots.
s=s.replace(' fclose(f);retro_unload_game();',f''' fclose(f);
 {{char path[1024];snprintf(path,sizeof(path),"%s.sram",argv[3]);FILE *save=fopen(path,"wb");
 uint8_t *battery=retro_get_memory_data(0);if(battery&&save)fwrite(battery,1,2048,save);if(save)fclose(save);}}
 retro_unload_game();''')
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=next(p for p in prior['hashes'] if p.endswith('.dylib'))
reports=[]
for scenario in (0,1,2,3,4):
 trace=o/f'trace-{scenario}.csv'
 args=[str(o/'harness'),core,str(r/'out/rom.bin'),str(trace),str(scenario)]
 if scenario==3:args.append(str(o/'trace-2.csv.sram'))
 with (o/f'log-{scenario}.txt').open('w') as log:subprocess.run(args,check=True,stdout=log,stderr=subprocess.STDOUT)
 rows=[{k:int(v) for k,v in row.items()} for row in csv.DictReader(trace.open())]
 assert len(rows)==(6500 if scenario==4 else 2800)
 assert all(row['pacing_owner']==syms['session_pacing'] and row['pacing_phase']<5*53693175 for row in rows[1100:]), 'Arcade scene lost shared pacing owner'
 assert rows[2100]['phase']==1 and rows[2100]['credits']==1
 if scenario==4:
  changes=[(i,row['phase']) for i,row in enumerate(rows) if i and row['phase']!=rows[i-1]['phase']]
  assert [phase for _,phase in changes]==[1,2,3,0],changes
  start,individual,total,end=[i for i,_ in changes]
  assert 3675<=individual-start<=3690,changes
  assert 190<=total-individual<=205 and 190<=end-total<=205,changes
  assert rows[-1]['save_status']==2 and rows[-1]['credits']==1
 elif scenario==1:
  assert rows[2300]['phase']==0 and rows[2300]['credits']==0 and rows[2300]['lives']>0
 else:
  assert rows[2300]['phase']==2 and rows[2500]['phase']==3 and rows[2700]['phase']==0
  assert rows[2700]['save_status']==2 and rows[2700]['credits']==1
 # Genesis Plus GX exposes SRAM on odd physical bytes. Validate the serialized
 # schema independently of the target SavedData layout and save status flag.
 battery=Path(str(trace)+'.sram').read_bytes()[1::2]
 valid=[]
 for offset in (0,512):
  slot=battery[offset:offset+512]
  if len(slot)==512 and slot[:8]==bytes.fromhex('53534e53000201a9') and slot[511]==0xa5:
   assert zlib.crc32(slot[:507])==int.from_bytes(slot[507:511],'big'), 'SRAM CRC mismatch'
   valid.append(slot)
 assert valid, 'No committed v2 SRAM slot'
 slot=max(valid,key=lambda b:int.from_bytes(b[8:12],'big'))
 if scenario==3:
  assert rows[900]['save_status']==1, 'Fresh process did not load saved data'
  assert slot[337:361]==bytes.fromhex('000600001111111100050000111111110005000011111111'),slot[337:361].hex()
  assert slot[377:389]==bytes.fromhex('001100001111111111111111'),slot[377:389].hex()
  assert int.from_bytes(slot[8:12],'big')==2
 elif scenario==2:
  assert slot[337:353]==bytes.fromhex('00060000111111110005000011111111'),slot[337:353].hex()
  assert slot[377:389]==bytes.fromhex('001100001111111111111111'),slot[377:389].hex()
 else:
  expected='00050000'+('10101010' if scenario==1 else '3f3f3f3f' if scenario==4 else '11111111')
  assert slot[337:345]==bytes.fromhex(expected),slot[337:345].hex()
 reports.append({'scenario':scenario,'passed':True,'shared_arcade_pacing_owner':True,'phase_at_2300':rows[2300]['phase'],'save_status':rows[-1]['save_status'],
                 'idle_timeout_transitions':changes if scenario==4 else None,'fresh_process_restore':scenario==3,'sram_crc_valid':True,'individual_first_two':slot[337:353].hex(),'total_first':slot[377:389].hex()})
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
report={'results':reports,'hashes':prior['hashes'],'scope':'Injected qualifying scores/final-death boundary in 1P and 2P, controller letter entry then ranking screens, committed SRAM names/scores/CRC, separate paid Start interruption, fresh-process restore followed by another ranked run, and unattended name-entry expiry through both timed boards. Not natural reachability, original-CPU lifecycle or hardware proof.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

#!/usr/bin/env python3
"""Target coin cue PCM with explicit DIP injection; no frontend window."""
from pathlib import Path
import re,subprocess,hashlib,json,csv
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/coin-audio';o.mkdir(parents=True,exist_ok=True)
prior=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text());assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'out/parity-audit/attract-runtime/harness.c').read_text();a=s.index('static void audio(');b=s.index('#define FN',a)
s=s[:a]+'''static unsigned scenario,enabled;static unsigned long long samples[2],nonzero[2];static unsigned peak[2];
static void audio(int16_t l,int16_t r){
 unsigned edge=scenario>=2?2000:1000;int bin=frame>=edge&&frame<edge+250?1:frame>=edge-150&&frame<edge-10?0:-1;
 if(bin<0)return;int v[2]={l,r};for(unsigned k=0;k<2;k++){unsigned m=v[k]<0?-v[k]:v[k];samples[bin]++;nonzero[bin]+=m!=0;if(m>peak[bin])peak[bin]=m;}
}
static size_t batch(const int16_t*d,size_t n){for(size_t i=0;i<n;i++)audio(d[i*2],d[i*2+1]);return n;}
static void poll(void){}
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;if(port)return 0;
 if(frame<740)return id==3&&frame%24<12;
 unsigned edge=scenario>=2?2000:1000;
 return scenario!=0&&id==8&&frame>=edge&&frame<edge+12;
}
'''+s[b:]
s=s.replace('if(argc!=4)return 2;','if(argc!=6)return 2;scenario=atoi(argv[4]);enabled=atoi(argv[5]);').replace('frame<10000','frame<2300')
s=s.replace('  retro_run();',f'''  if(frame==800){{
   unsigned raw=r8(ram,{syms['g_config']});raw=enabled?(raw&~32u):(raw|32u);
   if(scenario==3)raw=(raw&~31u)|24u;
   w8(ram,{syms['g_config']},raw);w8(ram,{syms['g_config']+5},enabled);
  }}
  if(frame==1990 && scenario>=2 && !r8(ram,{syms['g_attract']}))return 10;
  retro_run();
  if(frame==2150 && scenario>=2){{
   if(r8(ram,{syms['g_attract']})!=(scenario==3))return 11;
   if(r16(ram,{syms['g_arcadeCredits']})!=(scenario==2))return 12;
  }}''')
s=s.replace('fclose(f);retro_unload_game();','printf("PCM %llu %llu %u %llu %llu %u\\n",samples[0],nonzero[0],peak[0],samples[1],nonzero[1],peak[1]);fclose(f);retro_unload_game();')
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib';results=[]
for scenario,enabled in ((0,0),(1,0),(1,1),(2,0),(3,0)):
 with (o/f'log-{scenario}-{enabled}.txt').open('w') as log:subprocess.run([str(o/'harness'),str(core),str(r/'out/rom.bin'),str(o/f'trace-{scenario}-{enabled}.csv'),str(scenario),str(enabled)],check=True,stdout=log,stderr=subprocess.STDOUT)
 m=re.search(r'PCM (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)',(o/f'log-{scenario}-{enabled}.txt').read_text());assert m
 before,nz0,peak0,after,nz1,peak1=map(int,m.groups());assert before>100000 and after>100000
 assert nz0==0 and peak0==0,(scenario,enabled,'pre-cue audio',nz0,peak0)
 if scenario:assert nz1>1000 and peak1>100,(scenario,enabled,nz1,peak1)
 else:assert nz1==0 and peak1==0
 results.append({'scenario':scenario,'demo_sound':enabled,'samples_before':before,'nonzero_before':nz0,'samples_after':after,'nonzero_after':nz1,'peak_after':peak1})
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
report={'results':results,'hashes':prior['hashes'],'harness_sha256':hashlib.sha256((o/'harness.c').read_bytes()).hexdigest(),'scope':'Controller boot and C pulses, injected raw/decoded demo DIP and partial-coin ratio only. PCM silence before cue; output after title/credited-demo/partial-demo coins, including demo sound OFF; partial coin remains in demo. No exact waveform/timbre or source transition timing claim.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

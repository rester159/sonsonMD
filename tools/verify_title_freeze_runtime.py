#!/usr/bin/env python3
"""Windowless Freeze on title: coin processing continues; mode dispatch waits."""
from pathlib import Path
import subprocess,re,json,hashlib,csv
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/title-freeze';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
for address,hexbytes in ((0x412d,'b6300643973b39'),(0x911e,'bd91b4bd4127bd6362bd91d5bd4134bd4167'),(0x9130,'963b85802608d602588e9141ad953b')):
 expected=bytes.fromhex(hexbytes);assert rom[address-0x4000:address-0x4000+len(expected)]==expected
prior=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text());assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'out/parity-audit/attract-runtime/harness.c').read_text();a=s.index('static int16_t input(');b=s.index('#define FN',a)
s=s[:a]+'''static unsigned scenario;
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;if(port)return 0;
 if(frame<740)return id==3&&frame%24<12;
 if(scenario==0){
  if(frame>=950&&frame<962)return id==8;
  if((frame>=1000&&frame<1012)||(frame>=2400&&frame<2412))return id==3;
 }
 return 0;
}
'''+s[b:]
s=s.replace('if(argc!=4)return 2;','if(argc!=5)return 2;scenario=atoi(argv[4]);').replace('frame<10000','frame<3600')
s=s.replace('  retro_run();',f'''  if(frame==900)w8(ram,{syms['g_config']+1},r8(ram,{syms['g_config']+1})&127);
  if(frame==2300)w8(ram,{syms['g_config']+1},r8(ram,{syms['g_config']+1})|128);
  retro_run();''')
s=s.replace('delay0,delay1\\n','delay0,delay1,credits\\n').replace("  fputc('\\n',f);",f"  fprintf(f,\",%u\",r16(ram,{syms['g_arcadeCredits']}));\n  fputc('\\n',f);")
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib';checks=[]
for scenario in (0,1):
 with (o/f'log-{scenario}.txt').open('w') as log:subprocess.run([str(o/'harness'),str(core),str(r/'out/rom.bin'),str(o/f'trace-{scenario}.csv'),str(scenario)],check=True,stdout=log,stderr=subprocess.STDOUT)
 rows=[{k:int(v) for k,v in row.items()} for row in csv.DictReader((o/f'trace-{scenario}.csv').open())]
 assert len(rows)==3600
 frozen=rows[1100:2290]
 clock_updates=sum((b['clock']-a['clock'])&255 for a,b in zip(frozen,frozen[1:]))
 expected_updates=(len(frozen)-1)*55.4/59.922743404312
 checks.append({'scenario':scenario,'clock_updates':clock_updates,'clock_runs_during_freeze':abs(clock_updates-expected_updates)<=2,'title_stays_idle':len({x['tick'] for x in frozen})==1 and {x['attract'] for x in frozen}=={0},'coin_preserved':all(x['credits']==(scenario==0) for x in frozen),'resumes':(rows[2500]['tick']>frozen[-1]['tick'] and rows[2500]['credits']==0 and rows[2500]['attract']==0) if scenario==0 else (rows[2301]['attract']==0 and rows[3500]['attract']==1),'trace_sha256':hashlib.sha256((o/f'trace-{scenario}.csv').read_bytes()).hexdigest()})
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
report={'checks':checks,'hashes':prior['hashes'],'scope':'Inject Freeze DIP on arcade title for1400refreshes. Scenario0 inserts coin and pulses Start while frozen, then fresh Start after release; scenario1 checks idle demo timer. Checks source-rate clock advancement during frozen title dispatch. Full lifecycle phase, held-Start-across-release, visual and audio fidelity remain unverified.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert all(all(x[k] for k in ('title_stays_idle','coin_preserved','resumes','clock_runs_during_freeze')) for x in checks)

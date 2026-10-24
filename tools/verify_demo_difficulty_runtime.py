#!/usr/bin/env python3
"""Windowless demo/paid start difficulty matrix; DIP settings are injected."""
from pathlib import Path
import argparse,csv,hashlib,json,re,subprocess
p=argparse.ArgumentParser();p.add_argument('--expect-old',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/demo-difficulty'/('before' if a.expect_old else 'after');o.mkdir(parents=True,exist_ok=True)
(o/'report.json').unlink(missing_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
assert rom[0x8f80-0x4000:0x8f89-0x4000]==bytes.fromhex('d6540d2226015fd75a')
prior=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'out/parity-audit/attract-runtime/harness.c').read_text()
a0=s.index('static int16_t input(');b=s.index('#define FN',a0)
s=s[:a0]+'''static unsigned difficulty,paid;
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;if(port)return 0;
 if(frame<740)return id==3&&frame%24<12;
 return paid && frame>=1000 && frame<1012 && (id==3||id==8);
}
'''+s[b:]
s=s.replace('if(argc!=4)return 2;','if(argc!=6)return 2;difficulty=atoi(argv[4]);paid=atoi(argv[5]);').replace('frame<10000','frame<2200')
s=s.replace('  retro_run();',f'''  if(frame==900){{w8(ram,{syms['g_config']+3},difficulty);
   w8(ram,{syms['g_config']+1},(r8(ram,{syms['g_config']+1})&159)|((3-difficulty)<<5));}}
  retro_run();''')
s=s.replace('delay0,delay1\\n','delay0,delay1,stage,combat\\n').replace("  fputc('\\n',f);",f'''  fprintf(f,",%u,%u",r8(ram,{syms['g_game']+86}),r16(ram,{syms['g_arcadeCombatDifficulty']}));
  fputc('\\n',f);''')
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=next(p for p in prior['hashes'] if p.endswith('.dylib'));results=[]
for paid in (0,1):
 for difficulty in range(4):
  trace=o/f'trace-{paid}-{difficulty}.csv'
  with (o/f'log-{paid}-{difficulty}.txt').open('w') as log:subprocess.run([str(o/'harness'),core,str(r/'out/rom.bin'),str(trace),str(difficulty),str(paid)],check=True,stdout=log,stderr=subprocess.STDOUT)
  rows=[{k:int(v) for k,v in row.items()}for row in csv.DictReader(trace.open())];assert len(rows)==2200
  early=[row for row in rows[1000:] if row['attract']==(1-paid) and 1<=row['tick']<=4]
  assert early,(paid,difficulty)
  want=4*difficulty if paid else 0
  passed=all(row['stage']==want and row['combat']==want*256 for row in early)
  assert passed==(not a.expect_old or paid==1 or difficulty==0),(paid,difficulty,early)
  results.append({'paid':paid,'difficulty':difficulty,'expected_stage':want,'observed_stages':sorted({row['stage'] for row in early}),'snapshots':len(early),'passed':passed})
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
report={'results':results,'hashes':prior['hashes'],'scope':'Injected raw/decoded DIP settings, controller boot and paid Start; read-only first-four-update stage/combat snapshots. Source8F80 live-game gate pinned. Not complete demo trajectories or full lifecycle parity.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

#!/usr/bin/env python3
"""Read-only scores during controller-started demos and a paid shooting run."""
from pathlib import Path
import argparse,csv,hashlib,json,re,subprocess
p=argparse.ArgumentParser();p.add_argument('--expect-old',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/demo-score'/('before' if a.expect_old else 'after');o.mkdir(parents=True,exist_ok=True)
(o/'report.json').unlink(missing_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
assert rom[0x4cbe-0x4000:0x4cc6-0x4000]==bytes.fromhex('3440962210270092')
assert rom[0x4d58-0x4000:0x4d5a-0x4000]==bytes.fromhex('35c0')
prior=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'out/parity-audit/attract-runtime/harness.c').read_text()
a0=s.index('static int16_t input(');b=s.index('#define FN',a0)
s=s[:a0]+'''static unsigned paid;
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;if(port)return 0;
 if(frame<740)return id==3&&frame%24<12;
 if(!paid)return 0;
 if(frame>=1000&&frame<1012)return id==3||id==8;
 return frame>1100 && (id==7 || (id==0 && frame%16<3) || (id==4 && frame%200<24) || (id==5 && frame%200>=24 && frame%200<48));
}
'''+s[b:]
s=s.replace('if(argc!=4)return 2;','if(argc!=5)return 2;paid=atoi(argv[4]);')
s=s.replace('delay0,delay1\\n','delay0,delay1,score0,score1\\n').replace("  fputc('\\n',f);",f'''  fprintf(f,",%u,%u",r32(ram,{syms['g_game']+28}),r32(ram,{syms['g_game']+70}));
  fputc('\\n',f);''')
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=next(p for p in prior['hashes'] if p.endswith('.dylib'));results=[]
for paid in (0,1):
 trace=o/f'trace-{paid}.csv'
 with (o/f'log-{paid}.txt').open('w') as log:subprocess.run([str(o/'harness'),core,str(r/'out/rom.bin'),str(trace),str(paid)],check=True,stdout=log,stderr=subprocess.STDOUT)
 rows=[{k:int(v)for k,v in row.items()}for row in csv.DictReader(trace.open())];assert len(rows)==10000
 # A paid observation ends at the first later demo; idle title scores must
 # never masquerade as paid-run awards.
 end=next((i for i,row in enumerate(rows) if i>=1100 and row['attract']),len(rows)) if paid else len(rows)
 observed=[row for row in rows[1100:end] if row['tick']>0 and row['attract']==1-paid];assert len(observed)>100
 maximum=[max(row[f'score{k}']for row in observed)for k in (0,1)]
 if paid:assert maximum[0]>0,maximum
 elif a.expect_old:assert max(maximum)>0,maximum
 else:assert maximum==[0,0],maximum
 results.append({'paid':paid,'snapshots':len(observed),'maximum_scores':maximum,'score_gate_passed':bool(paid or maximum==[0,0])})
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
report={'results':results,'hashes':prior['hashes'],'scope':'Controller-only boot, idle demos and separate paid shooting run; read-only score snapshots. Original4CBE live-game gate byte-pinned. No original matched gameplay trajectory, full reward coverage or hardware proof.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

#!/usr/bin/env python3
"""Read-only natural demo ending: frozen input/world and phased countdown."""
from pathlib import Path
import csv,hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/demo-end/runtime';o.mkdir(parents=True,exist_ok=True)
(o/'report.json').unlink(missing_ok=True)
prior=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text());assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
syms={n:int(v,16)for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'out/parity-audit/attract-runtime/harness.c').read_text().replace('delay0,delay1\\n','delay0,delay1,end_timer,camera\\n')
s=s.replace("  fputc('\\n',f);",f'''  fprintf(f,",%u,%u",r8(ram,{syms['demo_end']+1}),r16(ram,{syms['g_game']+88}));
  fputc('\\n',f);''')
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=next(p for p in prior['hashes']if p.endswith('.dylib'))
with (o/'core.log').open('w')as log:subprocess.run([str(o/'harness'),core,str(r/'out/rom.bin'),str(o/'trace.csv')],check=True,stdout=log,stderr=subprocess.STDOUT)
rows=[{k:int(v)for k,v in row.items()}for row in csv.DictReader((o/'trace.csv').open())];assert len(rows)==10000
wait=lambda row:any(lo<=row['pc']<hi for lo,hi in prior['coherent_wait_ranges'])
fields=['camera','cursor0','cursor1','input0','input1','previous0','previous1','delay0','delay1']
previous=None;checks=0;completed=0;phases=[]
for row in rows:
 if not row['attract'] or not row['demo_end']:
  previous=None;continue
 if not wait(row):continue
 if row['demo_end']==1:previous=None;continue
 if previous is None:
  phases.append(row['refresh']);previous=row;continue
 delta=row['tick']-previous['tick']
 assert 0<=delta<10,(row,previous)
 assert row['clock']==(previous['clock']+delta)&255
 assert all(row[k]==previous[k]for k in fields),(row,previous)
 boundaries=sum(((previous['clock']+n)&63)==0 for n in range(1,delta+1))
 assert row['end_timer']==previous['end_timer']-boundaries,(row,previous)
 if previous['demo_end']==2 and row['demo_end']==3:
  assert row['end_timer']==0 and (row['clock']&63)<=1
  completed+=1
 checks+=delta;previous=row
assert checks>300 and completed>=1,(checks,completed)
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
report={'passed':True,'wait_updates_checked':checks,'ending_segments':len(phases),'completed_countdowns':completed,'hashes':prior['hashes'],'scope':'Controller-only natural demos, coherent wait snapshots; input/script/camera remain fixed while shared clock and four-boundary countdown advance. Outer demo timeout can preempt an ending. No matched original actor-death timing, visual or sound-driver proof.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

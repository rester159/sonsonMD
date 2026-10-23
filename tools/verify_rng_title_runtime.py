#!/usr/bin/env python3
"""Controller-only cold boot: check initial Arcade title's source RNG seed."""
from pathlib import Path
import csv,hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/rng-handoff/title';o.mkdir(parents=True,exist_ok=True)
(o/'report.json').unlink(missing_ok=True)
prior=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
nm=(r/'out/symbol.txt').read_text();syms={n:int(v,16)for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',nm,re.M)}
s=(r/'out/parity-audit/attract-runtime/harness.c').read_text().replace('frame<10000','frame<1000').replace('delay0,delay1\\n','delay0,delay1,seed,sample\\n')
s=s.replace("  fputc('\\n',f);",f'''  fprintf(f,",%u,%u",r16(ram,{syms['arcade_rng']}),r8(ram,{syms['arcade_rng']+2}));
  fputc('\\n',f);''')
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=next(p for p in prior['hashes'] if p.endswith('.dylib'))
with (o/'core.log').open('w') as log:subprocess.run([str(o/'harness'),core,str(r/'out/rom.bin'),str(o/'trace.csv')],check=True,stdout=log,stderr=subprocess.STDOUT)
rows=list(csv.DictReader((o/'trace.csv').open()))
assert len(rows)==1000, f'Incomplete boot trace: {len(rows)} snapshots'
assert [int(row['refresh']) for row in rows]==list(range(1000)), 'Missing or duplicate frames'
title_rows=rows[900:1000]
assert len(title_rows)==100
for row in title_rows:assert int(row['seed'])==0x87cd and int(row['sample'])==0 and int(row['attract'])==0,row
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
report={'passed':True,'title_snapshots':len(title_rows),'hashes':prior['hashes'],'scope':'Read-only initial Arcade title RNG snapshots after controller-driven boot. No repeated-cycle or paid-game RNG trace comparison.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

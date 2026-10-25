#!/usr/bin/env python3
"""Archive a completed windowless run and require resumed scrolling after ending.
This verifies reachability only; lives are refilled by the generating harness.
"""
import argparse,csv,hashlib,json,shutil
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('name');args=p.parse_args()
if not args.name or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-'for c in args.name):p.error('name must use lowercase letters, digits and hyphens')
r=Path(__file__).resolve().parents[1];out=r/'out';dest=out/'parity-audit'/args.name
results=json.loads((out/'genplus-results.json').read_text())
assert {v['players']for v in results}=={1,2},'Both completed modes required'
rom_hash=hashlib.sha256((out/'rom.bin').read_bytes()).hexdigest();reports=[]
for result in results:
 assert result['rom_sha256']==rom_hash,'Build changed after runtime test'
 mode=result['players'];rows=list(csv.DictReader((out/f'genplus-{mode}p.csv').open()))
 assert len(rows)==result['samples']
 events=[]
 for i in range(1,len(rows)):
  if int(rows[i-1]['camera'])!=22016 or int(rows[i]['camera'])!=0:continue
  start=i-1
  while start and int(rows[start-1]['camera'])==22016:start-=1
  assert i+400<len(rows),'Need 400 post-restart refreshes'
  after=rows[i+400]
  assert int(after['camera'])>0,'Scroll failed to resume'
  assert int(after['players'])==mode,'Player count changed'
  events.append({'reset_refresh':int(rows[i]['refresh']),'hold_refreshes':i-start,
   'hold_ticks':int(rows[i]['tick'])-int(rows[start]['tick']),
   'camera_after_400_refreshes':int(after['camera'])})
 assert events,f'No end-of-course restart in {mode}P'
 reports.append({'players':mode,'events':events})
dest.mkdir(parents=True,exist_ok=False)
for name in ['genplus-results.json','rom.bin','symbol.txt']+[f'genplus-{n}p.{ext}'for n in (1,2)for ext in ('csv','txt')]:shutil.copy2(out/name,dest/name)
report={'rom_sha256':rom_hash,'scope':'Windowless controller-driven run with lives refilled. Ending hold, course reset and continued scrolling; not source state/visual parity or performance certification.','modes':reports}
(dest/'course-resets.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

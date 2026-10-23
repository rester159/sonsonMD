#!/usr/bin/env python3
"""Check source CPU observations of large-enemy initialization and damage."""
import csv, hashlib, json
from pathlib import Path
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
trace=o/'large-damage-oracle.csv'
rows=[{k:int(v) for k,v in row.items()} for row in csv.DictReader(trace.open())]
assert 'LARGE_DAMAGE_DONE' in (o/'large-damage-source/mame.log').read_text()
assert len(rows)==4610
for d in rows:
 if d['kind']==0:
  assert d['hp']==7 and d['active']==1
  continue
 hit=0<=d['dx']+28<=40 and 0<=(d['dy']+28)%256<40
 dead=hit and d['input_hp']==1
 assert d['hp']==((d['input_hp']-1)&255 if hit else d['input_hp']),d
 assert d['active']==(2 if dead else 1),d
 assert (d['major'],d['minor'])==((2,0)if dead else(1,0)),d
 assert (d['shot_active'],d['shot_major'],d['shot_minor'])==((2,2,0)if hit else(1,1,0)),d
 assert d['flash']==int(hit) and d['credit']==(d['owner']if dead else 0) and d['carry']==int(hit),d
sha=lambda b:hashlib.sha256(b).hexdigest()
program=sha(b''.join((r/'arcade'/n).read_bytes()for n in('ss.01e','ss.02e','ss.03e')))
assert program=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
report={'program_sha256':program,'trace_sha256':sha(trace.read_bytes()),'cases':len(rows),'initial_hp':7,'scope':'Original D58B initialization and EE8E hit leaf; both owners, every byte-valued HP, nine overlap/boundary inputs. Seven hits required from initialized HP. Full movement, scoring and natural encounter remain separate.'}
(o/'large-damage-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

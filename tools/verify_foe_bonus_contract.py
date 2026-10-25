#!/usr/bin/env python3
"""Check original shared FF behavior against an explicit byte-width contract."""
from pathlib import Path
import json,hashlib
r=Path(__file__).resolve().parents[1];out=r/'out/parity-audit'
rom=b''.join((r/'arcade'/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e'])
source=hashlib.sha256(rom).hexdigest();assert source=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def byte(a):assert 0x4000<=a<0x10000;return rom[a-0x4000]
def word(a):return byte(a)*256+byte(a+1)
def animate(b):
 if b[13]:b[13]=(b[13]-1)&255;return
 a=int.from_bytes(b[14:16],'big');b[16]=byte(a);b[17]=byte(a+1);ctl=byte(a+2)
 b[13]=ctl&127;b[14:16]=(word(a+3)if ctl&128 else a+3).to_bytes(2,'big')
rows=[json.loads(line) for line in (out/'foe-bonus-oracle.jsonl').read_text().splitlines()]
assert len(rows)==1408
for row in rows:
 b=bytearray.fromhex(row['before']);n=row['index'];phase=row['phase']
 if phase==0:
  b[2]=1;b[0]=(b[0]&253)|64;b[3]=n;n=(n+1)if n<10 else 0
  b[14:16]=word(0xb291+2*b[3]).to_bytes(2,'big');b[13]=0;animate(b)
 elif phase==2:
  b[2]=3;animate(b)
  if b[4]:b[17]=0xc3
 if row['x']>=248:b[1]=6;b[2]=2
 assert b.hex()==row['after'],row
 assert n==row['next_index'],row
result={'cases':len(rows),'record_bytes_checked':len(rows)*32,'global_counter_checks':len(rows),'program_sha256':source,'trace_sha256':hashlib.sha256((out/'foe-bonus-oracle.jsonl').read_bytes()).hexdigest(),'scope':'Original CPU isolated G/H FF states0..3, active flags1/2/65/128, bonus indices0..10, positions0/8/247/248. Purge disabled, animation timer seeded0. Native gameplay integration remains pending.'}
(out/'foe-bonus-contract.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

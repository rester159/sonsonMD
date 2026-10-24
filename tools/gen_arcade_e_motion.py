#!/usr/bin/env python3
"""Extract C5EE movement vectors for source scroll selectors0/1."""
from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parents[1];b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def byte(a):return b[a-0x4000]
def word(a):return byte(a)*256+byte(a+1)
rows=[]
for selector in range(2):
 base=word(0xc5ee+selector*2);row=[]
 for direction in range(17):
  a=base+direction*5;x=(byte(a)<<16)|word(a+1);x=x-(1<<24)if x&(1<<23)else x
  y=word(a+3);y=y-65536 if y&32768 else y
  assert -(1<<23)<=x<(1<<23);row.append((x,y))
 rows.append(row)
s='#include "arcade_e_motion.h"\nconst ArcadeEVector arcade_e_vectors[2][17]={\n'+''.join('    {'+','.join('{%d,%d}'%v for v in row)+'},\n'for row in rows)+'};\n'
(r/'src/arcade_e_motion_data.c').write_text(s)
o=r/'out/parity-audit/e-motion';o.mkdir(parents=True,exist_ok=True);(o/'manifest.json').write_text(json.dumps({'vectors':rows},indent=2)+'\n')
print('Extracted34 E movement vectors.')

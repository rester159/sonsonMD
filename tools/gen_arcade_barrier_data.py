#!/usr/bin/env python3
"""Decode the four original barrier animation sequences and background patterns."""
from pathlib import Path
import hashlib
r=Path(__file__).resolve().parents[1]
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
s='#include "arcade_barrier_step.h"\nconst u8 arcade_barrier_frames[4][9][4]={\n'
for base in (0xb7e3,0xb80b,0xb833,0xb85b):
 s+='{'+','.join('{'+','.join(str(x)for x in b[base-0x4000+i*4:base-0x4000+i*4+4])+'}'for i in range(9))+'},\n'
 assert b[base-0x4000+36:base-0x4000+40]==bytes((255,255,base>>8,base&255))
s+='};\nconst u8 arcade_barrier_patterns[36][8]={\n'
for i in range(8,44):s+='{'+','.join(str(x)for x in b[0xb403-0x4000+i*8:0xb403-0x4000+i*8+8])+'},\n'
s+='};\n';(r/'src/arcade_barrier_data.c').write_text(s)

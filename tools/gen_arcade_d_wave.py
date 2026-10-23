#!/usr/bin/env python3
"""Extract source F403's eight D formations without hand-transcribing rows."""
from pathlib import Path
import hashlib
r=Path(__file__).resolve().parents[1]
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
t=b[0xf403-0x4000:0xf443-0x4000];assert len(t)==64
s='/* Generated from source F403..F442. */\n#include "arcade_d_wave.h"\nconst u8 arcade_d_placements[8][4][2]={\n'
for row in range(8):s+='    {'+','.join('{%d,%d}'%tuple(t[row*8+i:row*8+i+2])for i in range(0,8,2))+'},\n'
s+='};\n';(r/'src/arcade_d_wave_data.c').write_text(s)
print('Extracted8 D formations/32births from pinned arcade ROM.')

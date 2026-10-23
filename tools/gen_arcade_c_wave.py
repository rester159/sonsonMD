#!/usr/bin/env python3
"""Extract source F2F8's eight C formations without hand-transcribing rows."""
from pathlib import Path
import hashlib
r=Path(__file__).resolve().parents[1]
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
t=b[0xf2f8-0x4000:0xf358-0x4000];assert len(t)==96
s='/* Generated from source F2F8..F357. */\n#include "arcade_c_wave.h"\nconst u8 arcade_c_placements[8][6][2]={\n'
for row in range(8):s+='    {'+','.join('{%d,%d}'%tuple(t[row*12+i:row*12+i+2])for i in range(0,12,2))+'},\n'
s+='};\n';(r/'src/arcade_c_wave_data.c').write_text(s)
print('Extracted8 C formations/48births from pinned arcade ROM.')

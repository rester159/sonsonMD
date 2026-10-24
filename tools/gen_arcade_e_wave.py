#!/usr/bin/env python3
"""Pinned source F4BC..F4DB: eight difficulty groups, two births each."""
from pathlib import Path
import hashlib
r=Path(__file__).resolve().parents[1];b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
t=b[0xf4bc-0x4000:0xf4dc-0x4000];assert len(t)==32
s='#include "arcade_e_wave.h"\nconst u8 arcade_e_placements[8][2][2]={\n'
for row in range(8):s+='    {'+','.join('{%d,%d}'%tuple(t[row*4+i:row*4+i+2])for i in (0,2))+'},\n'
s+='};\n';(r/'src/arcade_e_wave_data.c').write_text(s);print('Extracted8 E formations/16births.')

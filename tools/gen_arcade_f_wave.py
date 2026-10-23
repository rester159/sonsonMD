#!/usr/bin/env python3
"""F placement banks from original F57A pointer table."""
from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parents[1];b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
roots=[int.from_bytes(b[a-0x4000:a-0x4000+2],'big')for a in range(0xf57a,0xf582,2)]
assert roots==[0xf582,0xf5c2,0xf602,0xf642]
s='#include "arcade_f_wave.h"\nconst u8 arcade_f_placements[4][8][4][2]={\n'
for base in roots:
 s+='{\n'
 for j in range(8):s+='{'+','.join('{%d,%d}'%tuple(b[base+j*8+k*2-0x4000:base+j*8+k*2-0x4000+2])for k in range(4))+'},\n'
 s+='},\n'
s+='};\n';(r/'src/arcade_f_wave_data.c').write_text(s)
print('32 four-slot F formations extracted')

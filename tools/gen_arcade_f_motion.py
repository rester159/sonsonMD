#!/usr/bin/env python3
from pathlib import Path
import hashlib
r=Path(__file__).resolve().parents[1];b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def raw(a,n):return b[a-0x4000:a-0x4000+n]
s='#include "arcade_f_motion.h"\nconst ArcadeFVector arcade_f_entry_vectors[9]={\n'
for i in range(9):
 p=0xe09a+5*i;s+='{%d,%d},\n'%(int.from_bytes(raw(p,3),'big',signed=True),int.from_bytes(raw(p+3,2),'big',signed=True))
s+='};\nconst s32 arcade_f_cruise_vectors[9]={'+','.join(str(int.from_bytes(raw(0xe0c7+3*i,3),'big',signed=True))for i in range(9))+'};\n'
s+='const u8 arcade_f_sine[128]={'+','.join(map(str,raw(0xe0e2,128)))+'};\n'
(r/'src/arcade_f_motion_data.c').write_text(s);print(s)

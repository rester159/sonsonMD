#!/usr/bin/env python3
"""Independent raw-ROM pixel decode and compiled generated lookup checks."""
from pathlib import Path
import ctypes as C,csv,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit';d=json.loads((o/'bonus-art-map.json').read_text())
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_bonus_art.c'),'-o',str(o/'bonus-art.dylib')],check=True)
lib=C.CDLL(str(o/'bonus-art.dylib'))
pixels=(C.c_uint32*(d['distinct_cells']*32)).in_dll(lib,'arcade_bonus_pixels')
planes=[(r/'arcade'/a).read_bytes()+(r/'arcade'/b).read_bytes()for a,b in [('ss_9.m5','ss_10.m6'),('ss_11.m3','ss_12.m4'),('ss_13.m1','ss_14.m2')]]
lut=(r/'arcade/ssb3.h7').read_bytes();mapping={(p['code'],p['bank']):p['frame']for p in d['poses']}
checks=0
for (code,bank),frame in mapping.items():
 for y in range(16):
  for x in range(16):
   pos=code*32+y+(16 if x<8 else 0);bit=x%8 # MAME layout xoffset 7..0, MSB-first bit reader.
   pen=sum(((plane[pos]>>bit)&1)<<i for i,plane in enumerate(planes))
   color=(lut[bank*8+pen]&15)if pen else 0
   if pen and not color:color=15
   tile=(x//8)*2+y//8;row=pixels[frame*32+tile*8+y%8]
   actual=(row>>(28-(x%8)*4))&15
   assert actual==color,(code,bank,x,y,actual,color);checks+=1
# Arena ownership is independent of sprite pixels.
import re
scene=int(re.search(r'#define SS_SCENE_TILE_COUNT\s+(\d+)',(r/'inc/ss_gfx.h').read_text()).group(1))
anim=int(re.search(r'#define SS_ANIM_TILE_BASE\s+(\d+)',(r/'inc/ss_anim.h').read_text()).group(1))
assert scene<=d['tile_begin']<d['tile_end']<=anim
report={'pixel_checks':checks,'distinct_cells':d['distinct_cells'],'tile_begin':d['tile_begin'],'tile_end':d['tile_end'],'source_sha256':hashlib.sha256((r/'src/arcade_bonus_art.c').read_bytes()).hexdigest(),'scope':'Compiled art against raw source graphics/palette indices and non-overlapping gameplay arena. Target VRAM/palette display validation remains pending.'}
(o/'bonus-art-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

#!/usr/bin/env python3
"""Exact D poses, sharing animation cells and allocating protected scene gaps."""
from pathlib import Path
import re,json,hashlib
import gen_assets as G,gen_anim
r=Path(__file__).resolve().parents[1];roms=G.Roms(str(r/'arcade'))
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
# F000..F003 holds plane scrolling; F400 begins SAT. Reserve F020..F31F.
slots=[1921,1925,1929,1933,1937,1941]
protected={0}
for file,symbol in [('arcade_food_art.c','arcade_food_tiles'),('arcade_food_art.c','arcade_food_spawn_tiles'),('arcade_barrier_art.c','arcade_barrier_resident')]:
 s=(r/'src'/file).read_text().split(symbol,1)[1].split('=',1)[1].split(';',1)[0]
 protected|={int(n)for n in re.findall(r'\d+',s)if int(n)<136}
allocated={t+i for t in slots for i in range(4)}
assert not allocated&protected and not allocated&set(range(61,133))
w,*_=gen_anim.bake(roms,290);resident={tuple(w[i:i+32]):290+i//8 for i in range(0,len(w),32)}
# Reuse already-resident C death art, with exact pixel identity.
c_text=(r/'src/arcade_c_art.c').read_text()
c_slots=[int(n)for n in re.findall(r'\d+',c_text.split('arcade_c_art_slots',1)[1].split('={',1)[1].split('}',1)[0])]
c_pixels=[int(n,16)for n in re.findall(r'0x([0-9a-fA-F]{8})',c_text.split('pixels',1)[1].split('};',1)[0])]
for i,tile in enumerate(c_slots):resident.setdefault(tuple(c_pixels[i*32:i*32+32]),tile)
def byte(a):return b[a-0x4000]
def word(a):return byte(a)*256+byte(a+1)
roots=[word(0xc129+i*2)for i in range(16)]+[word(p)for p in (0xc1a2,0xc1a4,0xc1c8,0xc1ca)]
addresses=set()
for p in roots:
 while p not in addresses:
  assert 0xc149<=p<0xc1ee;addresses.add(p)
  p=word(p+3)if byte(p+2)&128 else p+3
poses=sorted({(byte(p)+((byte(p+1)&32)<<3),byte(p+1)&31)for p in addresses})
added=[];mapping=[]
for code,bank in poses:
 pixels=tuple(G.bake_sprite_tiles(roms,code,bank));tile=resident.get(pixels)
 if tile is None:
  tile=slots[len(added)];resident[pixels]=tile;added.append(pixels)
 mapping.append((code,bank,tile))
assert len(added)==len(slots)
(r/'inc/arcade_d_art.h').write_text('''#ifndef ARCADE_D_ART_H
#define ARCADE_D_ART_H
#include "arcade_types.h"
#define ARCADE_D_ART_CELLS 6
#define ARCADE_D_TILE_BASE 1921
#define ARCADE_D_TILE_COUNT 24
/* F020..F31F uses unused plane-scroll table space. Requires HSCROLL_PLANE,
 * HScroll baseF000 and SAT baseF400. Explicit cold-load ownership only. */
extern const u16 arcade_d_art_slots[ARCADE_D_ART_CELLS];
extern const u32 arcade_d_pixels[ARCADE_D_ART_CELLS][32];
u16 arcade_d_tile(u8 code,u8 attr);
#endif
''')
s='#include "arcade_d_art.h"\nconst u16 arcade_d_art_slots[ARCADE_D_ART_CELLS]={'+','.join(map(str,slots))+'};\nconst u32 arcade_d_pixels[ARCADE_D_ART_CELLS][32]={\n'
s+=''.join('{'+','.join('0x%08x'%v for v in pixels)+'},\n'for pixels in added)+'};\n'
s+='u16 arcade_d_tile(u8 code,u8 attr)\n{\n    switch(((u16)(attr&31)<<9)|code|((u16)(attr&32)<<3))\n    {\n'
for code,bank,tile in mapping:s+='        case %d:return %d;\n'%((bank<<9)|code,tile)
s+='    }\n    return 65535;\n}\n';(r/'src/arcade_d_art.c').write_text(s)
o=r/'out/parity-audit/d-art';o.mkdir(parents=True,exist_ok=True)
report={'poses':mapping,'slots':slots,'new_tiles':len(added)*4,'protected_scene_tiles':sorted(protected),'scope':'Exact existing Genesis pixel bake; no palette fidelity claim. Not yet cold-loaded or live-rendered.'}
(o/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

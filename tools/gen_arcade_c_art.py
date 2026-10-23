#!/usr/bin/env python3
"""Exact C poses, sharing animation cells and allocating protected scene gaps."""
from pathlib import Path
import re,json,hashlib
import gen_assets as G,gen_anim
r=Path(__file__).resolve().parents[1];roms=G.Roms(str(r/'arcade'))
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
# Fixed gaps leave food/entrance/barrier aliases and main61..132 intact.
slots=[1,5,9,14,20,24,28,32,36]
protected={0}
for file,symbol in [('arcade_food_art.c','arcade_food_tiles'),('arcade_food_art.c','arcade_food_spawn_tiles'),('arcade_barrier_art.c','arcade_barrier_resident')]:
 s=(r/'src'/file).read_text().split(symbol,1)[1].split('=',1)[1].split(';',1)[0]
 protected|={int(n)for n in re.findall(r'\d+',s)if int(n)<136}
allocated={t+i for t in slots for i in range(4)}
assert not allocated&protected and not allocated&set(range(61,133))
w,*_=gen_anim.bake(roms,290);resident={tuple(w[i:i+32]):290+i//8 for i in range(0,len(w),32)}
# Walk ROM animation roots and all normal script roots independently of manifest.
def byte(a):return b[a-0x4000]
def word(a):return byte(a)*256+byte(a+1)
roots=[0xbd01,word(0xbd63),word(0xbd65),word(0xbd89),word(0xbd8b)]
p=word(0xbdaf)
while byte(p)<128:roots.append(word(p+2));p+=4
assert p==0xbe81;roots.append(word(p+2))
addresses=set()
for p in roots:
 while p not in addresses:
  addresses.add(p)
  if p==0x86be:break # terminal transient pose; no subsequent animation read
  assert 0xbd01<=p<0xbdaf
  p=word(p+3)if byte(p+2)&128 else p+3
poses=sorted({(byte(p)+((byte(p+1)&32)<<3),byte(p+1)&31)for p in addresses})
added=[];mapping=[]
for code,bank in poses:
 pixels=tuple(G.bake_sprite_tiles(roms,code,bank));tile=resident.get(pixels)
 if tile is None:
  tile=slots[len(added)];resident[pixels]=tile;added.append(pixels)
 mapping.append((code,bank,tile))
assert len(added)==len(slots)
(r/'inc/arcade_c_art.h').write_text('''#ifndef ARCADE_C_ART_H
#define ARCADE_C_ART_H
#include "arcade_types.h"
#define ARCADE_C_ART_CELLS 9
/* Four tiles per cell. Load only after the captured scene; cold-load only. */
extern const u16 arcade_c_art_slots[ARCADE_C_ART_CELLS];
extern const u32 arcade_c_pixels[ARCADE_C_ART_CELLS][32];
u16 arcade_c_tile(u8 code,u8 attr);
#endif
''')
s='#include "arcade_c_art.h"\nconst u16 arcade_c_art_slots[ARCADE_C_ART_CELLS]={'+','.join(map(str,slots))+'};\nconst u32 arcade_c_pixels[ARCADE_C_ART_CELLS][32]={\n'
s+=''.join('{'+','.join('0x%08x'%v for v in pixels)+'},\n'for pixels in added)+'};\n'
s+='u16 arcade_c_tile(u8 code,u8 attr)\n{\n    switch(((u16)(attr&31)<<9)|code|((u16)(attr&32)<<3))\n    {\n'
for code,bank,tile in mapping:s+='        case %d:return %d;\n'%((bank<<9)|code,tile)
s+='    }\n    return 65535;\n}\n';(r/'src/arcade_c_art.c').write_text(s)
o=r/'out/parity-audit/c-art';o.mkdir(parents=True,exist_ok=True)
report={'poses':mapping,'slots':slots,'new_tiles':len(added)*4,'protected_scene_tiles':sorted(protected),'scope':'Exact existing Genesis pixel bake; no palette fidelity claim. Not yet cold-loaded or live-rendered.'}
(o/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

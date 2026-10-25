#!/usr/bin/env python3
"""Resident enemyMain poses; reuse exact animation cells and retired scene space."""
from pathlib import Path
import re,json,hashlib
import gen_assets as G,gen_anim
r=Path(__file__).resolve().parents[1];roms=G.Roms(str(r/'arcade'));rom=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
base,count=61,72;protected={0}
for file,symbol in [('arcade_food_art.c','arcade_food_tiles'),('arcade_food_art.c','arcade_food_spawn_tiles'),('arcade_barrier_art.c','arcade_barrier_resident')]:
 s=(r/'src'/file).read_text().split(symbol,1)[1].split('=',1)[1].split(';',1)[0]
 protected|={int(n)for n in re.findall(r'\d+',s)if int(n)<136}
assert not protected&set(range(base,base+count))
words,_,_,_=gen_anim.bake(roms,290);resident={tuple(words[i:i+32]):290+i//8 for i in range(0,len(words),32)}
# Reachable graph, independent of an old extraction manifest.
roots=[0xbb65+3*i for i in range(4)]+[0xbb9d,0xbbb1,0xbb7f];addresses=set()
for a in roots:
 while a not in addresses:
  assert 0xbb65<=a<0xbbc5;addresses.add(a);ctl=rom[a-0x4000+2]
  a=int.from_bytes(rom[a-0x4000+3:a-0x4000+5],'big')if ctl&128 else a+3
poses=sorted({(rom[a-0x4000]+((rom[a-0x4000+1]&32)<<3),rom[a-0x4000+1]&31)for a in addresses})
added=[];mapping=[]
for code,bank in poses:
 pixels=tuple(G.bake_sprite_tiles(roms,code,bank));tile=resident.get(pixels)
 if tile is None:
  tile=base+len(added)*4;resident[pixels]=tile;added.append(pixels)
 mapping.append((code,bank,tile))
assert len(added)*4==count
(r/'inc/arcade_main_art.h').write_text('''#ifndef ARCADE_MAIN_ART_H
#define ARCADE_MAIN_ART_H
#include "arcade_types.h"
/* Overwrites retired scene patterns only; borrowed food/barrier tiles protected. */
#define ARCADE_MAIN_TILE_BASE 61
#define ARCADE_MAIN_TILE_COUNT 72
extern const u32 arcade_main_pixels[ARCADE_MAIN_TILE_COUNT*8];
u16 arcade_main_tile(u8 code,u8 attr);
#endif
''')
s='#include "arcade_main_art.h"\nconst u32 arcade_main_pixels[ARCADE_MAIN_TILE_COUNT*8]={\n'+''.join(','.join('0x%08x'%v for v in pixels)+',\n'for pixels in added)+'};\nu16 arcade_main_tile(u8 code,u8 attr)\n{\n    switch(((u16)(attr&31)<<9)|code|((u16)(attr&32)<<3))\n    {\n'
for code,bank,tile in mapping:s+='        case %d:return %d;\n'%((bank<<9)|code,tile)
s+='    }\n    return 65535;\n}\n';(r/'src/arcade_main_art.c').write_text(s)
o=r/'out/parity-audit/main-art';o.mkdir(parents=True,exist_ok=True);report={'poses':mapping,'new_tiles':count,'base':base,'protected_scene_tiles':sorted(protected),'program_sha256':hashlib.sha256(rom).hexdigest(),'scope':'Exact baked Genesis pixels, preserving existing palette adaptation; no arcade-color fidelity claim.'};(o/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');print('23 poses;18 new contiguous cells;72 tiles at61..132')

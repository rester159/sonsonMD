#!/usr/bin/env python3
"""Bake barrier characters and flashes with exact resident reuse hints."""
from pathlib import Path
import hashlib,json,re
import gen_assets as G
r=Path(__file__).resolve().parents[1];g=G.Roms(str(r/'arcade'))
rom=b''.join((r/'arcade'/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e'])
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
resident={};inputs={}
main_header=(r/'inc/arcade_main_art.h').read_text()
main_base=int(re.search(r'#define ARCADE_MAIN_TILE_BASE\s+(\d+)',main_header)[1])
main_count=int(re.search(r'#define ARCADE_MAIN_TILE_COUNT\s+(\d+)',main_header)[1])
inputs['inc/arcade_main_art.h']=hashlib.sha256(main_header.encode()).hexdigest()
c_art=(r/'src/arcade_c_art.c').read_text()
c_slots=[int(n)for n in re.findall(r'\d+',c_art.split('arcade_c_art_slots',1)[1].split('={',1)[1].split('}',1)[0])]
c_reserved={tile+i for tile in c_slots for i in range(4)}
inputs['src/arcade_c_art.c']=hashlib.sha256(c_art.encode()).hexdigest()
for file,symbol,base in [('src/ss_gfx.c','ss_scene_tiles',0),('src/arcade_bonus_art.c','arcade_bonus_pixels',136),('src/ss_anim.c','ss_anim_tiles',290),('src/ss_level.c','world_tiles',730)]:
    path=r/file
    if not path.exists():path=r/'res/ss_gfx.c'
    text=path.read_text();inputs[str(path.relative_to(r))]=hashlib.sha256(path.read_bytes()).hexdigest()
    data=text.split(symbol,1)[1].split('};',1)[0]
    words=[int(x,16)for x in re.findall(r'0x([0-9a-fA-F]{8})',data)]
    assert len(words)%8==0
    for i in range(0,len(words),8):
        # These captured-scene patterns are overwritten by resident main art.
        if symbol=='ss_scene_tiles' and (main_base<=base+i//8<main_base+main_count or base+i//8 in c_reserved):continue
        resident.setdefault(tuple(words[i:i+8]),base+i//8)

# Food's immutable tiles are also resident during gameplay.
inputs['src/arcade_food_art.c']=hashlib.sha256((r/'src/arcade_food_art.c').read_bytes()).hexdigest()
data=(r/'src/arcade_food_art.c').read_text().split('arcade_food_pixels',1)[1].split('};',1)[0]
words=[int(x,16)for x in re.findall(r'0x([0-9a-fA-F]{8})',data)]
slots=list(range(280,288))+list(range(1372,1440))
for i,slot in enumerate(slots):resident.setdefault(tuple(words[i*8:i*8+8]),slot)
cells=[];maps=[];hints=[];pose_cost=[]
for index in range(8,44):
 pattern=rom[0xb403+index*8-0x4000:0xb403+index*8-0x4000+8]
 versions=[]
 for variant in range(5):
  pose=[]
  for cell in range(4):
   attr=pattern[cell+4] if variant==0 else [0x4d,0x49,0x45,0x41][variant-1]
   pixels=tuple(G.bake_char_tile(g,pattern[cell]+((attr&3)<<8),attr>>2))
   if pixels not in cells:cells.append(pixels);hints.append(resident.get(pixels,65535))
   pose.append(cells.index(pixels))
  versions.append(pose);pose_cost.append(len({i for i in pose if hints[i]==65535}))
 maps.append(versions)
header='#ifndef ARCADE_BARRIER_ART_H\n#define ARCADE_BARRIER_ART_H\n#include "arcade_types.h"\n#define ARCADE_BARRIER_ART_TILES '+str(len(cells))+'\nextern const u32 arcade_barrier_pixels[ARCADE_BARRIER_ART_TILES*8];\nextern const u16 arcade_barrier_tiles[36][5][4];\n/* Immutable resident tile or65535: needs dynamic allocation before publication. */\nextern const u16 arcade_barrier_resident[ARCADE_BARRIER_ART_TILES];\n#endif\n'
(r/'inc/arcade_barrier_art.h').write_text(header)
s='#include "arcade_barrier_art.h"\nconst u32 arcade_barrier_pixels[ARCADE_BARRIER_ART_TILES*8]={\n'
for tile in cells:s+=','.join('0x%08x'%v for v in tile)+',\n'
s+='};\nconst u16 arcade_barrier_tiles[36][5][4]={\n'
for versions in maps:s+='{'+','.join('{'+','.join(map(str,pose))+'}'for pose in versions)+'},\n'
s+='};\nconst u16 arcade_barrier_resident[ARCADE_BARRIER_ART_TILES]={'+','.join(map(str,hints))+'};\n'
(r/'src/arcade_barrier_art.c').write_text(s)
out=r/'out/parity-audit/barrier-art';out.mkdir(exist_ok=True)
report=dict(tiles=len(cells),resident_tiles=sum(t!=65535 for t in hints),new_tiles=sum(t==65535 for t in hints),max_new_tiles_per_pose=max(pose_cost),poses=180,bytes=len(cells)*32,resident_inputs=inputs,scope='Complete36 patterns with normal plus four flash attribute choices. Logical ROM maps and resident hints; dynamic VRAM binding pending.')
(out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
# Reserve four contiguous tiles per live source slot, coloring lifetime conflicts.
objects=[]
text=(r/'src/arcade_world_objects.c').read_text().split('={',1)[1].split('};',1)[0]
for row in re.findall(r'\{([0-9,]+)\}',text):objects.append(list(map(int,row.split(','))))
assert len(objects)==348
adj=[set()for _ in range(20)];pool=objects[:16]+[[0]*8 for _ in range(4)];sets=[]
for block in range(4,88):
    active=[i for i,o in enumerate(pool)if o[-1]==2 and o[2]];sets.append(active)
    for i in active:adj[i].update(set(active)-{i})
    if block<87:pool[block%5*4:block%5*4+4]=objects[block*4:block*4+4]
colors={}
while len(colors)<20:
    i=max((i for i in range(20)if i not in colors),key=lambda i:(len({colors[j]for j in adj[i]if j in colors}),len(adj[i]),-i))
    used={colors[j]for j in adj[i]if j in colors}
    colors[i]=next(c for c in range(20)if c not in used)
assert max(colors.values())==11
assert all(len({colors[i]for i in slots})==len(slots)for slots in sets)
s='#include "arcade_barrier_upload.h"\nconst u8 arcade_barrier_slot[20]={'+','.join(str(colors[i])for i in range(20))+'};\nconst u32 arcade_barrier_pose_pixels[180][32]={\n'
for versions in maps:
    for pose in versions:s+='{'+','.join('0x%08x'%word for tile in pose for word in cells[tile])+'},\n'
s+='};\n';(r/'src/arcade_barrier_pose_data.c').write_text(s)
(out/'allocation.json').write_text(json.dumps(dict(slots=[colors[i]for i in range(20)],max_pool_barriers=max(map(len,sets)),pool_states=len(sets),vram_range=[0xf800,0xfe00],pose_bytes=180*128,source_sha256=hashlib.sha256((r/'src/arcade_world_objects.c').read_bytes()).hexdigest(),scope='Conflict-free across source rotating-pool snapshots; per-slot old map retirement and live reset/publication ordering require integration validation.'),indent=2)+'\n')
# A64-column ring overwrites both old columns before a shared slot's new art.
groups={}
for i,obj in enumerate(objects):
    if obj[-1]==2 and obj[2]:groups.setdefault(colors[i%20],[]).append((i,obj[0]))
pairs=[]
for color,items in groups.items():
    for left,right in zip(items,items[1:]):pairs.append((color,left[0],right[0],right[1]-left[1]))
assert all(pair[3]>=528 for pair in pairs), 'Shared barrier art outlives old map columns'
(out/'lifetime.json').write_text(json.dumps(dict(pairs=pairs,minimum_world_separation=min(p[3]for p in pairs),scope='Monotonic course: new art at newX-256, old two-column map overwritten by oldX+16+256. Campaign reset/abnormal jumps require separate reset.'),indent=2)+'\n')

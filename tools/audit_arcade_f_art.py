#!/usr/bin/env python3
"""Inventory F poses against current resident actor graphics, without allocating VRAM."""
from pathlib import Path
import hashlib,json,re
import gen_assets as G
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/f-art';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def byte(a):return b[a-0x4000]
def word(a):return byte(a)*256+byte(a+1)
roots=[0xe032,0xe040]+[word(p)for p in (0xe04e,0xe050,0xe074,0xe076)]
frames=set()
for p in roots:
 while p not in frames:
  assert 0xe032<=p<0xe09a;frames.add(p);p=word(p+3)if byte(p+2)&128 else p+3
poses=sorted({(byte(p)+((byte(p+1)&32)<<3),byte(p+1)&31)for p in frames})
resident={}
s=(r/'src/ss_anim.c').read_text().split('ss_anim_tiles',1)[1].split('};',1)[0]
w=[int(x,16)for x in re.findall(r'0x([0-9a-fA-F]{8})',s)]
for i in range(0,len(w),32):resident.setdefault(tuple(w[i:i+32]),290+i//8)
for family in ('c','d'):
 s=(r/f'src/arcade_{family}_art.c').read_text()
 slots=[int(n)for n in re.findall(r'\d+',s.split(f'arcade_{family}_art_slots',1)[1].split('={',1)[1].split('}',1)[0])]
 pixels=[int(n,16)for n in re.findall(r'0x([0-9a-fA-F]{8})',s.split('pixels',1)[1].split('};',1)[0])]
 for i,tile in enumerate(slots):resident.setdefault(tuple(pixels[i*32:i*32+32]),tile)
s=(r/'src/arcade_main_art.c').read_text().split('pixels',1)[1].split('};',1)[0]
w=[int(x,16)for x in re.findall(r'0x([0-9a-fA-F]{8})',s)]
assert len(w)==72*8
for i in range(0,len(w),32):resident.setdefault(tuple(w[i:i+32]),61+i//8)
roms=G.Roms(str(r/'arcade'));missing={};mapping=[]
for code,bank in poses:
 pixels=tuple(G.bake_sprite_tiles(roms,code,bank));tile=resident.get(pixels)
 if tile is None:missing.setdefault(pixels,len(missing))
 mapping.append({'code':code,'bank':bank,'resident_tile':tile,'new_cell':missing[pixels]if tile is None else None})
def flip_cell(words,h,v):
 # Genesis 2x2 sprites store tiles column first, then rows within each tile.
 rows=[[0]*16 for _ in range(16)]
 for y in range(16):
  for x in range(16):
   row=words[((x//8)*2+y//8)*8+y%8]
   rows[y][x]=(row>>(28-4*(x%8)))&15
 out=[]
 for tx in range(2):
  for ty in range(2):
   for yy in range(8):
    row=0
    for xx in range(8):
     x=tx*8+xx;y=ty*8+yy
     row=(row<<4)|rows[15-y if v else y][15-x if h else x]
    out.append(row)
 return tuple(out)
flip_resident={}
for pixels,tile in resident.items():
 for h,v in ((0,0),(1,0),(0,1),(1,1)):
  transformed=flip_cell(pixels,h,v)
  assert flip_cell(transformed,h,v)==pixels
  flip_resident.setdefault(transformed,(tile,h,v))
flip_new={};flip_mapping=[]
for pixels,index in missing.items():
 reuse=flip_resident.get(pixels)
 if reuse is not None:flip_mapping.append({'new_cell':index,'resident':reuse});continue
 reuse=flip_new.get(pixels)
 if reuse is not None:flip_mapping.append({'new_cell':index,'shared_new':reuse});continue
 flip_mapping.append({'new_cell':index,'unique':True})
 for h,v in ((0,0),(1,0),(0,1),(1,1)):
  flip_new.setdefault(flip_cell(pixels,h,v),(index,h,v))
report={'animation_frames':len(frames),'poses':mapping,'new_cells':len(missing),'new_tiles':len(missing)*4,'new_bytes':len(missing)*128,'flip_sharing':flip_mapping,'unique_cells_after_flips':sum(bool(p.get('unique'))for p in flip_mapping),'scope':'Exact indexed-pixel comparisons against resident animation/C/D/main cells. No allocation, palette fidelity or runtime rendering claim.'}
(o/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

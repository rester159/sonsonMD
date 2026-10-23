#!/usr/bin/env python3
"""Compiled resident poses versus the established arcade-to-Genesis pixel bake."""
from pathlib import Path
import ctypes as C,re,json,subprocess
import gen_assets as G
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/f-art';m=json.loads((o/'audit.json').read_text())
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_f_art.c'),str(r/'src/arcade_bonus_art.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'));lib.arcade_f_tile.argtypes=[C.c_uint8,C.c_uint8];lib.arcade_f_tile.restype=C.c_uint16
resident={}
for file,symbol,base in [('ss_gfx.c','ss_scene_tiles',0),('ss_anim.c','ss_anim_tiles',290)]:
 s=(r/'src'/file).read_text().split(symbol,1)[1].split('};',1)[0];w=[int(x,16)for x in re.findall(r'0x([0-9a-fA-F]{8})',s)]
 for i in range(0,len(w),8):resident[base+i//8]=w[i:i+8]
# C's resident death cell is also used by F.
c=(r/'src/arcade_c_art.c').read_text()
slots=[int(n)for n in re.findall(r'\d+',c.split('arcade_c_art_slots',1)[1].split('={',1)[1].split('}',1)[0])]
cp=[int(n,16)for n in re.findall(r'0x([0-9a-fA-F]{8})',c.split('pixels',1)[1].split('};',1)[0])]
for cell,tile in enumerate(slots):
 for i in range(4):resident[tile+i]=cp[cell*32+i*8:cell*32+i*8+8]
roms=G.Roms(str(r/'arcade'));checks=0
for p in m['poses']:
 code,bank,tile=p['code'],p['bank'],p['resident_tile']
 expected=list(G.bake_sprite_tiles(roms,code,bank))
 for flip in (0,64,128,192):
  got=lib.arcade_f_tile(code&255,bank|((code>>3)&32)|flip);assert got==tile
  actual=sum((resident[got+i]for i in range(4)),[]);assert actual==expected
  checks+=256
assert lib.arcade_f_tile(255,31)==65535
# Bonus score poses may select a shared animation cell instead of bonus VRAM.
class Bonus(C.Structure):
 _fields_=[('next',C.c_uint16),('tile',C.c_uint16),('owner2_tile',C.c_uint16),('code',C.c_uint8),('attr',C.c_uint8),('timer',C.c_uint8)]
bonus=list((C.c_uint32*(132*8)).in_dll(lib,'arcade_bonus_pixels'));aliases=0
for f in (Bonus*22).in_dll(lib,'arcade_bonus_frames'):
 for attr,tile in [(f.attr,f.tile)]+([(0xc3,f.owner2_tile)]if f.owner2_tile!=65535 else []):
  shared=lib.arcade_f_tile(f.code,attr)
  if shared==65535:continue
  assert sum((resident[shared+i]for i in range(4)),[])==bonus[(tile-136)*8:(tile-136)*8+32]
  aliases+=1
assert aliases==0 # F normal score49 has no overlap with shared bonus score50.

report={'bonus_aliases':aliases,'pixel_checks':checks,'new_resident_bytes':0,'scope':'Compiled mappings/pixels match established indexed Genesis bake across11 poses/all flip attributes. All art uses existing resident cells; no target readback or live F-enemy rendering proof.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

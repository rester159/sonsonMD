#!/usr/bin/env python3
"""Compiled resident poses versus the established arcade-to-Genesis pixel bake."""
from pathlib import Path
import ctypes as C,re,json,subprocess
import gen_assets as G
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/main-art';m=json.loads((o/'manifest.json').read_text())
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_main_art.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'));lib.arcade_main_tile.argtypes=[C.c_uint8,C.c_uint8];lib.arcade_main_tile.restype=C.c_uint16
pixels=list((C.c_uint32*(72*8)).in_dll(lib,'arcade_main_pixels'));resident={}
for file,symbol,base in [('ss_gfx.c','ss_scene_tiles',0),('ss_anim.c','ss_anim_tiles',290)]:
 s=(r/'src'/file).read_text().split(symbol,1)[1].split('};',1)[0];w=[int(x,16)for x in re.findall(r'0x([0-9a-fA-F]{8})',s)]
 for i in range(0,len(w),8):resident[base+i//8]=w[i:i+8]
before={k:list(v)for k,v in resident.items()}
for i in range(72):resident[61+i]=pixels[i*8:i*8+8]
for tile in m['protected_scene_tiles']:assert resident[tile]==before[tile]
roms=G.Roms(str(r/'arcade'));checks=0
for code,bank,tile in m['poses']:
 expected=list(G.bake_sprite_tiles(roms,code,bank))
 for flip in (0,64,128,192):
  got=lib.arcade_main_tile(code&255,bank|((code>>3)&32)|flip);assert got==tile
  actual=sum((resident[got+i]for i in range(4)),[]);assert actual==expected
  checks+=256
assert lib.arcade_main_tile(255,31)==65535
s=(r/'src/game.c').read_text();assert s.index('VDP_loadTileData(ss_scene_tiles')<s.index('VDP_loadTileData(arcade_main_pixels')<s.index('VDP_loadTileData(arcade_bonus_pixels')
report={'pixel_checks':checks,'new_resident_bytes':2304,'protected_scene_patterns':len(m['protected_scene_tiles']),'scope':'Compiled mappings/pixels match established indexed Genesis bake across23 poses/all flip attributes. Protected food/barrier aliases unchanged. Cold-load order checked; no target readback or live main-enemy rendering proof.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

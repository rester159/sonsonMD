#!/usr/bin/env python3
"""Check name grid, label pixels, name writes and cursor SAT placement."""
import ctypes as C
import hashlib,json,re,subprocess
from pathlib import Path
import gen_assets as G
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/name-art';o.mkdir(parents=True,exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),
 *[str(r/'src'/n) for n in ('arcade_name_art.c','arcade_name_map.c','arcade_cell_sat.c')],'-o',str(o/'art.dylib')],check=True)
lib=C.CDLL(str(o/'art.dylib'))
class Name(C.Structure):_fields_=[('raw',C.c_uint8*12),('name',C.c_uint8*4)]
class View(C.Structure):_fields_=[('x',C.c_int16),('y',C.c_int16),('width',C.c_uint16),('height',C.c_uint16)]
lib.arcade_name_map_begin.argtypes=[C.POINTER(C.c_uint16),C.c_uint8]
lib.arcade_name_map_update.argtypes=[C.POINTER(C.c_uint16),C.c_uint8,C.POINTER(Name),C.c_uint8]
lib.arcade_name_cursor_sat.argtypes=[C.POINTER(Name),C.c_uint8,C.c_uint16,C.POINTER(View),C.POINTER(C.c_uint16),C.c_uint16,C.c_uint16]
lib.arcade_name_cursor_sat.restype=C.c_uint16
h=(r/'inc/arcade_name_art.h').read_text()
def val(n):return int(re.search('#define ARCADE_NAME_'+n+r' (\d+)',h)[1])
data=(C.c_uint32*(val('TILES')*8)).in_dll(lib,'arcade_name_pixels')
tiles=[tuple(data[i:i+8]) for i in range(0,len(data),8)]
gfx=G.Roms(str(r/'arcade'))
def char(c,a):return G.bake_char_tile(gfx,c,a>>2)
grid=bytes.fromhex('2f1011101210131014101510161017101819101a101b101c101d101e101f10201021221023102410251026102710281029102a001001100210031004100b100d102c103f051006100710081009100e102b105f104f')
assert len(grid)==85
checks=0
for players in range(4):
 m=(C.c_uint16*1024)(*[65535]*1024);lib.arcade_name_map_begin(m,players)
 expected=[None]*1024;expected[160:992]=[char(16,0)]*832
 for i,code in enumerate(grid):expected[0xc8+(i//17)*64+i%17]=char(code,0x2c)
 labels=[(0x247,0x0c,'ENTER@YOUR@INITIALS')]
 if players&1:labels.append((0x2a8,0x10,'SONSON'))
 if players&2:labels.append((0x2b2,0x10,'TONTON'))
 for start,attr,text in labels:
  for i,c in enumerate(text):expected[start+i]=char(ord(c)-48,attr)
 for cell,pixels in enumerate(expected):
  if pixels is None:assert m[cell]==65535
  else:assert tiles[m[cell]]==pixels,(players,cell)
  checks+=1
for player in range(2):
 for glyph in range(64):
  for slot in range(4):
   s=Name();s.name[:]=[glyph]*4;m=(C.c_uint16*1024)(*[65535]*1024)
   lib.arcade_name_map_update(m,player,C.byref(s),1<<slot)
   destination=(0x2f2 if player else 0x2e7)+slot*2
   assert tiles[m[destination]]==char(glyph,0x2c)
   assert sum(v!=65535 for v in m)==1;checks+=1
 for selector in range(85):
  if selector%17 in (1,15):continue
  for crop in (0,1,24):
   s=Name();s.raw[6]=selector;v=View(0,crop,256,224);sat=(C.c_uint16*8)(*[65535]*8)
   n=lib.arcade_name_cursor_sat(C.byref(s),player,136,C.byref(v),sat,0,2)
   assert n==1
   assert list(sat[:4])==[128+44+16*(selector//17)-crop,0x501,0x2000+136+val('CURSOR')+player*4,128+60+8*(selector%17)]
   assert list(sat[4:])==[65535]*4
   s.raw[2]=1;before=bytes(sat)
   assert lib.arcade_name_cursor_sat(C.byref(s),player,136,C.byref(v),sat,0,2)==0 and bytes(sat)==before
   checks+=1
 expected=G.bake_sprite_tiles(gfx,0x56+player,3 if not player else 0)
 start=(val('CURSOR')+player*4)*8
 assert tuple(data[start:start+32])==tuple(expected)
report={'checks':checks,'tiles':val('TILES'),'fits_bonus_arena':val('TILES')<=132,'passed':True,
 'source_sha256':{n:hashlib.sha256((r/n).read_bytes()).hexdigest() for n in ('src/arcade_name_art.c','src/arcade_name_map.c')},
 'scope':'Host compiled grid/labels/name-cell pixels, preserved HUD/bottom row, both cursor glyphs and valid selector SAT positions under three viewports. Existing graphics decoder reused; VDP and live session integration pending.'}
(o/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

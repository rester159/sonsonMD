#!/usr/bin/env python3
"""Validate production ranking map against source numeric/name layout."""
import ctypes as C
import hashlib
import json
from pathlib import Path
import re
import subprocess
import gen_assets as G

root=Path(__file__).resolve().parents[1]
out=root/'out/parity-audit/title-art'
out.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(root/'tools/verify_title_art.py')],check=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib',
 '-I'+str(root/'inc'),str(root/'src/arcade_title_map.c'),str(root/'src/arcade_title_art.c'),
 '-o',str(out/'ranking.dylib')],check=True)
lib=C.CDLL(str(out/'ranking.dylib'))
class Entry(C.Structure):
    _fields_=[('score',C.c_uint8*4),('name',C.c_uint8*8)]
lib.arcade_title_map_ranking.argtypes=[C.POINTER(C.c_uint16),C.c_uint8,C.POINTER(Entry)]
lib.arcade_title_map_label.argtypes=[C.POINTER(C.c_uint16),C.c_uint8]
count=int(re.search(r'#define ARCADE_TITLE_ART_TILES (\d+)',(root/'inc/arcade_title_art.h').read_text())[1])
data=(C.c_uint32*(count*8)).in_dll(lib,'arcade_title_pixels')
tiles=[tuple(data[i:i+8]) for i in range(0,len(data),8)]
gfx=G.Roms(str(root/'arcade'))
attrs=(0x10,0x28,4,0x1c,0xc)
oracle=[[G.bake_char_tile(gfx,c,a>>2) for c in range(256)] for a in attrs]
checks=cases=0
# Vary all name codes and score nibble values/positions, including all-zero and
# non-decimal values retained by the arcade's raw packed-nibble renderer.
for total in (0,1):
    for seed in range(256):
        entries=(Entry*5)()
        map_=(C.c_uint16*1024)(*[65535]*1024)
        lib.arcade_title_map_label(map_,13 if total else 8)
        expected=[None if v==65535 else tiles[v] for v in map_]
        for row,entry in enumerate(entries):
            value=seed << ((seed%8)*4)
            value &= 0xffffffff
            for i,b in enumerate(value.to_bytes(4,'big')):entry.score[i]=b
            for i in range(8):entry.name[i]=(seed+i+row)&255
            # Independent packed-nibble expansion through hexadecimal text.
            digits=f'{value:08x}'.lstrip('0') or '0'
            chars=[16]*(8-len(digits))+[int(c,16) for c in digits]
            score=0x231+row*64 if total else 0x22f+row*64
            name=0x227+row*64 if total else 0x22a+row*64
            for i,c in enumerate(chars):expected[score+i]=oracle[row][c]
            for i in range(8 if total else 4):
                expected[name+i+(i//4)]=oracle[row][entry.name[i]]
        lib.arcade_title_map_ranking(map_,total,entries)
        for cell,pixels in enumerate(expected):
            if pixels is None:assert map_[cell]==65535,(total,seed,cell)
            else:assert map_[cell]<count and tiles[map_[cell]]==pixels,(total,seed,cell)
            checks+=1
        cases+=1
report={'cases':cases,'map_cells_checked':checks,'passed':True,
 'source_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in (root/'src/arcade_title_map.c',root/'src/arcade_title_art.c')},
 'scope':'Host-compiled individual/total ranking map: all byte-valued name glyphs per row, packed score nibbles, leading blanks, original gaps/positions and untouched cells. Static labels separately checked. Existing graphic decoder reused; SRAM ranking ownership and live VDP integration pending.'}
(out/'ranking-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

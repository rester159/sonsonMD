#!/usr/bin/env python3
"""Verify source lifecycle poses against selector C and linked resident tiles.
Selector is host-compiled pending gameplay integration; no linked selector claim.
"""
from pathlib import Path
import ctypes,hashlib,json,re,struct,subprocess,tempfile
import gen_assets as G
r=Path(__file__).resolve().parents[1]
program=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(program).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
# Independently walk the six original graphs, including their loop pointers.
poses=set()
for root in (0xaffc,0xb00a,0xafac,0xafd0,0xaf9a,0xaf9f):
    seen=set();a=root
    while a not in seen:
        seen.add(a);code,attr,ctl=program[a-0x4000:a-0x4000+3]
        poses.add((code,attr))
        a=int.from_bytes(program[a-0x4000+3:a-0x4000+5],'big') if ctl&128 else a+3
        assert len(seen)<=4
assert len(poses)==16
rom=(r/'out/rom.bin').read_bytes()
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
h=(r/'inc/ss_anim.h').read_text()
base=int(re.search(r'#define SS_ANIM_TILE_BASE\s+(\d+)',h)[1])
count=int(re.search(r'#define SS_ANIM_TILE_COUNT\s+(\d+)',h)[1])
g=G.Roms(str(r/'arcade'));checks=0
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp)
    (tmp/'arcade_life.h').write_text('#include <stdint.h>\ntypedef uint8_t u8;typedef uint16_t u16;\n')
    lib=tmp/'life.dylib'
    subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-I'+str(tmp),str(r/'src/arcade_life_art.c'),'-o',str(lib)],check=True)
    fn=ctypes.CDLL(str(lib)).arcade_life_tile
    fn.argtypes=[ctypes.c_uint8,ctypes.c_uint8];fn.restype=ctypes.c_uint16
    for code,attr in sorted(poses):
        tile=fn(code,attr);assert base<=tile and tile+4<=base+count
        expected=b''.join(struct.pack('>I',v) for v in G.bake_sprite_tiles(g,code+((attr&32)<<3),attr&31))
        start=syms['ss_anim_tiles']+(tile-base)*32
        assert rom[start:start+128]==expected,(code,attr,tile)
        assert attr&0xc0==0xc0,'Expected unflipped player poses'
        checks+=128
    assert fn(0xff,0)==0 and fn(0x40,0xc1)==0
world=(r/'inc/ss_level.h').read_text()
wb=int(re.search(r'#define SS_LEVEL_TILE_BASE\s+(\d+)',world)[1])
wt=int(re.search(r'#define SS_LEVEL_TILE_TOP\s+(\d+)',world)[1])
assert base+count<=wb<wt<=1376
result={'rom_sha256':hashlib.sha256(rom).hexdigest(),'poses':len(poses),'pixel_bytes':checks,'mismatches':0,'sprite_tiles':[base,base+count],'world_tiles':[wb,wt],'scope':'Host-compiled semantic selector and linked resident pixels; lifecycle renderer integration pending'}
(r/'out/parity-audit/life-assets-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))

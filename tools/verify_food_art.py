#!/usr/bin/env python3
"""Compiled food tile maps and pixels against raw arcade graphics and tables."""
from pathlib import Path
import ctypes as C,hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/food-art'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_food_art.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'));maps=(C.c_uint16*160).in_dll(lib,'arcade_food_tiles');pixels=(C.c_uint32*(76*8)).in_dll(lib,'arcade_food_pixels')
tiles={}
for file,symbol,base in [('src/ss_gfx.c','ss_scene_tiles',0),('src/arcade_bonus_art.c','arcade_bonus_pixels',136),('src/ss_anim.c','ss_anim_tiles',290),('src/ss_level.c','world_tiles',730)]:
    path=r/file
    if not path.exists():path=r/'res/ss_gfx.c'
    data=path.read_text().split(symbol,1)[1].split('};',1)[0]
    words=[int(x,16)for x in re.findall(r'0x([0-9a-fA-F]{8})',data)]
    for i in range(0,len(words),8):tiles[base+i//8]=words[i:i+8]
slots=list(range(280,288))+list(range(1372,1440))
assert all(slot not in tiles for slot in slots)
assert not set(slots)&set(range(268,280)) and not set(slots)&set(range(1308,1372))
for i,slot in enumerate(slots):tiles[slot]=list(pixels[i*8:i*8+8])
rom=b''.join((r/'arcade'/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e'])
p0=(r/'arcade/ss_7.b6').read_bytes();p1=(r/'arcade/ss_8.b5').read_bytes();lut=(r/'arcade/ssb2.c4').read_bytes()
checks=0
for family,base in enumerate([0xb403,0xb5c3]):
    for index,food in enumerate(range(-12,8)):
        source=rom[base+food*8-0x4000:base+food*8-0x4000+8]
        for cell in range(4):
            code=source[cell]+((source[cell+4]&3)<<8);bank=source[cell+4]>>2
            tile=maps[family*80+index*4+cell];assert tile in tiles
            for y in range(8):
                for x in range(8):
                    pen=((p0[code*8+y]>>(7-x))&1)|(((p1[code*8+y]>>(7-x))&1)<<1)
                    expected=lut[bank*4+pen]&15
                    actual=(tiles[tile][y]>>(28-x*4))&15
                    assert actual==expected,(family,food,cell,x,y,tile,actual,expected)
                    checks+=1
spawn_maps=(C.c_uint16*8).in_dll(lib,'arcade_food_spawn_tiles')
for i in range(8):
    phase=i//2;cell=i%2
    code=rom[0xb18a-0x4000+phase*2+cell]+512
    attr=rom[0xb18a-0x4000+8+phase*2+cell]
    assert attr==0x42
    tile=spawn_maps[i];assert tile in tiles
    for y in range(8):
        for x in range(8):
            pen=((p0[code*8+y]>>(7-x))&1)|(((p1[code*8+y]>>(7-x))&1)<<1)
            assert ((tiles[tile][y]>>(28-x*4))&15)==(lut[(attr>>2)*4+pen]&15)
            checks+=1
report=dict(pixel_checks=checks,poses=40,new_tiles=76,new_bytes=2432,
            source_sha256=hashlib.sha256((r/'src/arcade_food_art.c').read_bytes()).hexdigest(),
            scope='Compiled maps/pixels and exact borrowed resident tiles versus source ROM character planes/LUT. Ranges280..287 and1372..1439 require font base>=1440. Target layout/readback and live rendering pending.')
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

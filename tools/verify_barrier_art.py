#!/usr/bin/env python3
"""Compiled barrier tile maps and pixels against raw arcade graphics and tables."""
from pathlib import Path
import ctypes as C,hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/barrier-art'
manifest=json.loads((r/'out/parity-audit/barrier-art/manifest.json').read_text());count=manifest['tiles']
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_barrier_art.c'),str(r/'src/arcade_barrier_pose_data.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'));maps=(C.c_uint16*720).in_dll(lib,'arcade_barrier_tiles');pixels=(C.c_uint32*(count*8)).in_dll(lib,'arcade_barrier_pixels');hints=(C.c_uint16*count).in_dll(lib,'arcade_barrier_resident')
pose_pixels=(C.c_uint32*(180*32)).in_dll(lib,'arcade_barrier_pose_pixels')
for pose in range(180):
    for cell in range(4):
        tile=maps[pose*4+cell]
        assert list(pose_pixels[pose*32+cell*8:pose*32+cell*8+8])==list(pixels[tile*8:tile*8+8])
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
food=(r/'src/arcade_food_art.c').read_text().split('arcade_food_pixels',1)[1].split('};',1)[0]
foodwords=[int(x,16)for x in re.findall(r'0x([0-9a-fA-F]{8})',food)]
for i,slot in enumerate(slots):tiles[slot]=foodwords[i*8:i*8+8]
for i,hint in enumerate(hints):
    if hint!=65535:assert list(pixels[i*8:i*8+8])==tiles[hint],(i,hint)
rom=b''.join((r/'arcade'/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e'])
p0=(r/'arcade/ss_7.b6').read_bytes();p1=(r/'arcade/ss_8.b5').read_bytes();lut=(r/'arcade/ssb2.c4').read_bytes()
checks=0
for index in range(36):
    source=rom[0xb403+(index+8)*8-0x4000:0xb403+(index+8)*8-0x4000+8]
    for variant in range(5):
        for cell in range(4):
            attr=source[cell+4] if variant==0 else (0x4d,0x49,0x45,0x41)[variant-1]
            code=source[cell]+((attr&3)<<8);bank=attr>>2
            tile=maps[index*20+variant*4+cell];assert tile<count
            for y in range(8):
                for x in range(8):
                    pen=((p0[code*8+y]>>(7-x))&1)|(((p1[code*8+y]>>(7-x))&1)<<1)
                    expected=lut[bank*4+pen]&15
                    actual=(pixels[tile*8+y]>>(28-x*4))&15
                    assert actual==expected,(index,variant,cell,x,y)
                    checks+=1
report=dict(pixel_checks=checks,poses=180,tiles=count,resident_hints_checked=sum(h!=65535 for h in hints),source_sha256=hashlib.sha256((r/'src/arcade_barrier_art.c').read_bytes()).hexdigest(),scope='Compiled logical tile maps and pixels versus independent raw arcade character planes/palette LUT, including every flash variant. Resident hints checked against live immutable banks; VRAM allocation and target upload pending.')
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

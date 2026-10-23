#!/usr/bin/env python3
"""Check compiled pickup pixels against raw planes and original CPU poses."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import subprocess

r = Path(__file__).resolve().parents[1]
out = r/'out/parity-audit/powerup-art'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib',
                '-I'+str(r/'inc'),str(r/'src/arcade_powerup_art.c'),
                '-o',str(out/'art.dylib')], check=True)
lib = C.CDLL(str(out/'art.dylib'))
lib.arcade_powerup_art_cell.argtypes = [C.c_uint8, C.c_uint8]
lib.arcade_powerup_art_cell.restype = C.c_uint16
pixels = (C.c_uint32*(19*32)).in_dll(lib, 'arcade_powerup_pixels')
planes = [(r/'arcade'/a).read_bytes()+(r/'arcade'/b).read_bytes()
          for a,b in [('ss_9.m5','ss_10.m6'),('ss_11.m3','ss_12.m4'),('ss_13.m1','ss_14.m2')]]
lut = (r/'arcade/ssb3.h7').read_bytes()
# Use observed original CPU poses, not the generated manifest, for reachability.
trace = r/'out/parity-audit/powerup-lifecycle.jsonl'
poses = set()
for line in (trace.read_text()+(r/'out/parity-audit/orb-lifecycle.jsonl').read_text()).splitlines():
    row = json.loads(line)
    actor = bytes.fromhex(row['after'])
    if actor[0]:
        poses.add((actor[16], actor[17] & 63))
assert len(poses) == 19
used = set()
pixel_checks = 0
for code, attr in poses:
    cell = lib.arcade_powerup_art_cell(code, attr)
    assert cell < 19, (code, attr, cell)
    used.add(cell)
    fullcode = code + ((attr & 32)<<3)
    for y in range(16):
        for x in range(16):
            offset = fullcode*32+y+(16 if x < 8 else 0)
            pen = sum(((plane[offset]>>(x%8)) & 1)<<i for i,plane in enumerate(planes))
            expected = (lut[(attr & 31)*8+pen] & 15) if pen else 0
            if pen and not expected:
                expected = 15
            tile = (x//8)*2+y//8
            actual = (pixels[cell*32+tile*8+y%8]>>(28-(x%8)*4)) & 15
            assert actual == expected, (code,attr,x,y,actual,expected)
            pixel_checks += 1
assert used == set(range(19))
for code in range(256):
    for attr in range(256):
        result = lib.arcade_powerup_art_cell(code,attr)
        assert (result < 19) == ((code,attr & 63) in poses), (code,attr,result)
        if result < 19:
            assert result == lib.arcade_powerup_art_cell(code,attr & 63)
report = dict(pixel_checks=pixel_checks, selector_checks=65536, cells=19,
              trace_sha256=hashlib.sha256(trace.read_bytes()).hexdigest(),
              orb_trace_sha256=hashlib.sha256((r/'out/parity-audit/orb-lifecycle.jsonl').read_bytes()).hexdigest(),
              generated_sha256=hashlib.sha256((r/'src/arcade_powerup_art.c').read_bytes()).hexdigest(),
              scope='Compiled ROM art against raw arcade bitplanes/palette LUT; selectors against observed original CPU poses. Live target VRAM/CRAM/SAT validation pending.')
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

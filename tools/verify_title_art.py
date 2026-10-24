#!/usr/bin/env python3
"""Check compiled title label assets against independently transcribed labels."""
import ctypes as C
import hashlib
import json
from pathlib import Path
import re
import subprocess
import gen_assets as G

root = Path(__file__).resolve().parents[1]
out = root/'out/parity-audit/title-art'
out.mkdir(parents=True, exist_ok=True)
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-dynamiclib',
                '-I'+str(root/'inc'), str(root/'src/arcade_title_art.c'),
                '-o', str(out/'test.dylib')], check=True)
lib = C.CDLL(str(out/'test.dylib'))
header = (root/'inc/arcade_title_art.h').read_text()
def count(name):
    return int(re.search(r'#define ARCADE_TITLE_ART_'+name+r' (\d+)', header)[1])
class Cell(C.Structure):
    _fields_ = [('cell', C.c_uint16), ('tile', C.c_uint16)]
class Group(C.Structure):
    _fields_ = [('command', C.c_uint16), ('first', C.c_uint16), ('count', C.c_uint16)]
pixels = (C.c_uint32*(count('TILES')*8)).in_dll(lib, 'arcade_title_pixels')
cells = (Cell*count('CELLS')).in_dll(lib, 'arcade_title_cells')
groups = (Group*count('GROUPS')).in_dll(lib, 'arcade_title_groups')
# @ is the source space glyph; ':' is the source copyright glyph.
labels = {
    6: [(0x13c1, 0x38, '@CREDIT'+'@'*25)],
    7: [(0x13c1, 0x08, '@FREE@PLAY'+'@'*21)],
    8: [(0x11e9, 0x2c, 'SCORE@@RANKING')],
    10: [(0x138a, 0x2c, ':@CAPCOM@1984')],
    11: [(0x11a7, 0x10, 'PLEASE@DEPOSIT@COIN')],
    12: [(0x11a7, 0x0c, 'PRESS@START@BUTTON')],
    13: [(0x11e6, 0x2c, 'TOTAL@SCORE@RANKING')],
}
for row, (rank, attr) in enumerate(zip(('1ST','2ND','3RD','4TH','5TH'), (0x10,0x28,4,0x1c,0xc))):
    labels[8] += [(0x1226+row*64, attr, rank), (0x1237+row*64, attr, 'PTS')]
    labels[13] += [(0x1223+row*64, attr, rank), (0x1239+row*64, attr, 'PTS')]
gfx = G.Roms(str(root/'arcade'))
checked = 0
for group in groups:
    expected = [(addr+i, ord(ch)-0x30, attr)
                for addr, attr, text in labels.pop(group.command)
                for i, ch in enumerate(text)]
    assert group.count == len(expected), (group.command, group.count, len(expected))
    assert group.first == checked
    for index, (addr, code, attr) in enumerate(expected):
        cell = cells[group.first+index]
        assert cell.cell == addr-0x1000
        assert cell.tile < count('TILES')
        # Existing tested graphics conversion supplies the pixel oracle; this
        # checks the new label selection/placement/attributes and C serialization.
        assert tuple(pixels[cell.tile*8:cell.tile*8+8]) == G.bake_char_tile(gfx, code, attr>>2)
        checked += 1
assert not labels and checked == count('CELLS')
report = {'compiled_cells_checked': checked, 'groups': len(groups),
          'asset_sha256': hashlib.sha256((root/'src/arcade_title_art.c').read_bytes()).hexdigest(),
          'scope': 'Host-compiled static label cells, original positions and palette-bank pixels. Existing graphics decoder reused. Live VDP, dynamic scores and lifecycle integration pending.'}
(out/'verification.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))

"""spawns.py -- list stage spawn records (bank 4 $BD08 -> bank 8 lists) per
stage, grouped by (bank, spawn routine), with stages, spawner ids, screens.
usage: python3 spawns.py [BANK]   (BANK hex filters the record bank)"""
import os, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../../..'))
rom = open(os.path.join(ROOT, 'arcade', 'Son_Son_II_J_TEng.pce'), 'rb').read()


def b(bank, a):
    return rom[bank * 0x2000 + (a & 0x1FFF)]


want = int(sys.argv[1], 16) if len(sys.argv) > 1 else None
use = {}
for s in range(7):
    lst = b(4, 0xBD08 + 2 * s) | b(4, 0xBD09 + 2 * s) << 8
    for scr in range(64):
        rec = b(8, lst + 2 * scr) | b(8, lst + 2 * scr + 1) << 8
        while b(8, rec) != 0xFF:
            x, y, sp, bk, lo, hi = [b(8, rec + i) for i in range(6)]
            use.setdefault((bk, hi << 8 | lo), []).append((s, scr, sp, x, y))
            rec += 6
for (bk, h), v in sorted(use.items()):
    if want is not None and bk != want:
        continue
    print('bank %02X $%04X n=%3d' % (bk, h, len(v)))
    for s in sorted({e[0] for e in v}):
        scr = sorted({(e[1] & 7, e[1] >> 3) for e in v if e[0] == s})
        print('    stage %d (idx %d) spawners %s screens(x,y) %s' % (
            s + 1, s, sorted({e[2] for e in v if e[0] == s}), scr))

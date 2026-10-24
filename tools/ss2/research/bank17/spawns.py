"""spawns.py [BANK...] -- list the stage spawn records (bank 4 $BD08 ->
bank 8 lists) grouped by (code bank, handler), with stages and screens."""
import os, sys
ROOT = os.path.dirname(os.path.abspath(__file__)).rsplit('/tools/', 1)[0]
rom = open(ROOT + '/arcade/Son_Son_II_J_TEng.pce', 'rb').read()


def bb(bank, a):
    return rom[bank * 0x2000 + (a & 0x1FFF)]


use = {}
for s in range(7):
    lst = bb(4, 0xBD08 + 2 * s) | bb(4, 0xBD09 + 2 * s) << 8
    for scr in range(64):
        rec = bb(8, lst + 2 * scr) | bb(8, lst + 2 * scr + 1) << 8
        while bb(8, rec) != 0xFF:
            r = [bb(8, rec + i) for i in range(6)]
            key = (r[3], r[4] | r[5] << 8)
            use.setdefault(key, []).append((s + 1, scr & 7, scr >> 3, r[0], r[1], r[2]))
            rec += 6
want = [int(a, 16) for a in sys.argv[1:] if a != '-v']
for (bank, h), l in sorted(use.items()):
    if want and bank not in want:
        continue
    print('bank %02X %04X: %d recs, stages %s' % (bank, h, len(l), sorted({x[0] for x in l})))
    if '-v' in sys.argv:
        for x in l:
            print('    st%d scr(%d,%d) xy %02X,%02X spawner %02X' % x)

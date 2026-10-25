"""peek.py K WX WY FRAMES [LOCK7F] -- inject like capture_at.py and print
task table, pc and a few object fields per frame (debugging aid)."""
import os, sys
sys.setrecursionlimit(10000)
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))
import snapshot

k, wx, wy, n = (int(a) for a in sys.argv[1:5])
lock = len(sys.argv) > 5
m = snapshot.stage_start(k)
done = [0]
pcs = {}


def tr(c, pc, op):
    if not done[0] and c.mpr[7] == 0 and pc == 0xEC42:
        r = m.ram
        cx = (wx - 0x70) & 0x7FF
        cy = (wy - 0x80) & 0x7FF
        for base in (0x45, 0x4B):
            r[base] = cx >> 8; r[base + 1] = cx & 0xFF; r[base + 2] = 0
            r[base + 3] = cy >> 8; r[base + 4] = cy & 0xFF; r[base + 5] = 0
        r[0x298] = 0; r[0x299] = 0x70; r[0x29B] = 0; r[0x29C] = 0x80
        for x in range(0x20):
            r[0x45A + x] = 0
        if lock:
            r[0x7F] = 1
        done[0] = 1
    if done[0]:
        key = (c.mpr[6], pc)
        pcs[key] = pcs.get(key, 0) + 1


m.cpu.trace = tr
for f in range(n):
    pcs.clear()
    m.run_frame()
    r = m.ram
    top = sorted([t for t in pcs.items() if not 0xE1A5 <= t[0][1] < 0xE1C1], key=lambda t: t[0][1])[:6]; ec = pcs.get((0x1F, 0xEC42), 0) + pcs.get((9, 0xEC42), 0)
    print(f, 'tasks', ' '.join('%02X' % r[i] for i in range(16)), '7F %02X 6B %02X' % (r[0x7F], r[0x6B]),
          'EC42 %d' % ec, 'slots', ''.join('%X' % (r[0x45A + x] >> 4) for x in range(0x20)),
          ' '.join('%02X:%04X*%d' % (b, p, c) for (b, p), c in top))

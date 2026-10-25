"""watch.py SNAPSHOT FRAMES PAD ADDR[,ADDR...]  -- report code writing work-RAM addresses (logical $2000-$3FFF)."""
import pickle, sys
from collections import Counter
sys.setrecursionlimit(10000)
snap, frames, pad, addrs = sys.argv[1], int(sys.argv[2]), int(sys.argv[3], 16), [int(a, 16) for a in sys.argv[4].split(',')]
m = pickle.load(open(snap, 'rb'))
phys = {0x1F0000 + (a - 0x2000): a for a in addrs}
c = Counter(); vals = {}
ow = m.write
def w(a, v):
    if a in phys:
        cpu = m.cpu; k = (phys[a], cpu.mpr[cpu.pc >> 13], cpu.pc)
        c[k] += 1; vals.setdefault(k, []).append(v)
    ow(a, v)
m.write = w
m.pad = pad
for f in range(frames): m.run_frame()
for (a, b, pc), n in sorted(c.items()):
    print('$%04X  %02X:%04X  x%-4d %s' % (a, b, pc, n, ' '.join('%02X' % v for v in vals[(a, b, pc)][:16])))

"""trace.py K FRAMES BOT OUT [--nopairs]

Whole-frame research trace from snapshot.stage_start(K) (K = 0..6): writes
OUT.pairs (capture_pairs.py format, bracket $EC42..$EC42, for
tools/ss2/hosttest) and OUT.cov (pickle {(bank, pc): count} restricted to
bank 5 at $C000-$DFFF and bank 3 at $8000-$9FFF) plus the set of object
handlers seen in slots ($28BA/$28DA, $28FA/$291A with $275A bank).

BOT: 'script:<capture_pairs script>' or 'explore:<seed>' (census-like bot:
walks one way for a while, turns back, attacks and jumps on a cycle; the
seed varies the phases), or 'right', 'left'.
"""
import os, pickle, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..'))
sys.setrecursionlimit(10000)
import snapshot
from run import parse_script

k, frames, bot, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3], sys.argv[4]
nopairs = '--nopairs' in sys.argv
m = snapshot.stage_start(k)

if bot.startswith('script:'):
    ev = parse_script(bot[7:].split())
    def pad(f):
        return sum(b for a, z, b in ev if a <= f <= z)
else:
    kind, _, seed = bot.partition(':')
    seed = int(seed or 0)
    def pad(f):
        f2 = f + seed * 97
        if kind == 'right':
            p = 0x02
        elif kind == 'left':
            p = 0x08
        else:
            phase = f2 % (600 + seed * 40)
            p = 0x08 if 420 <= phase < 540 else 0x02
            if seed & 1:
                p ^= 0x0A
        if f2 % (12 + seed % 5) < 3: p |= 0x20
        if f2 % 90 >= 30 and f2 % 90 < 40 + (seed % 3) * 6: p |= 0x10
        if f2 % 400 >= 200 and f2 % 400 < 230: p |= 0x01
        if seed >= 4 and f2 % 300 >= 250 and f2 % 300 < 270: p = (p & ~0x01) | 0x04
        return p

cov = {}
pairs = []
pending = [None]


def tr(c, pc, op):
    b = c.mpr[pc >> 13]
    if (b == 5 and 0xC000 <= pc < 0xE000) or (b == 3 and 0x8000 <= pc < 0xA000):
        cov[(b, pc)] = cov.get((b, pc), 0) + 1
    if not nopairs and pc == 0xEC42 and c.mpr[7] == 0:
        if pending[0] is not None:
            pairs.append((m.frame, pending[0], bytes(m.ram)))
        pending[0] = bytes(m.ram)


handlers = {}
m.cpu.trace = tr
for f in range(frames):
    m.pad = pad(f)
    m.run_frame()
    ram = m.ram
    for x in range(32):
        if ram[0x45A + x]:
            bk = ram[0x75A + x]
            for hi, lo, kind in ((0x8BA, 0x8DA, 'h'), (0x8FA, 0x91A, 'hit')):
                h = ram[hi + x] << 8 | ram[lo + x]
                key = (bk if h >= 0xC000 else 0, h, kind)
                handlers[key] = handlers.get(key, 0) + 1
m.cpu.trace = None
pickle.dump({'cov': cov, 'handlers': handlers}, open(out + '.cov', 'wb'))
if not nopairs:
    with open(out + '.pairs', 'wb') as fh:
        fh.write(struct.pack('<4sI', b'SS2P', len(pairs)))
        for fr, a, b in pairs:
            fh.write(struct.pack('<H', fr & 0xFFFF))
            fh.write(a)
            fh.write(b)
print(len(pairs), 'pairs', len(cov), 'pcs')

"""irqmask_stage.py K FRAMES SEED OUT -- like tools/ss2/irq_mask.py but from
snapshot.stage_start(K) with the trace.py explorer bot: records work-RAM
bytes written from interrupt context (other stages play other music and
effects, touching sound-driver RAM the stage-1 mask never saw). The union
with out/ss2/irq_mask.bin is the mask for multi-stage replays."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..'))
sys.setrecursionlimit(10000)
import snapshot

k, frames, seed, out = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
m = snapshot.stage_start(k)


def pad(f):
    f2 = f + seed * 97
    phase = f2 % (600 + seed * 40)
    p = 0x08 if 420 <= phase < 540 else 0x02
    if seed & 1:
        p ^= 0x0A
    if f2 % (12 + seed % 5) < 3: p |= 0x20
    if f2 % 90 >= 30 and f2 % 90 < 40 + (seed % 3) * 6: p |= 0x10
    if f2 % 400 >= 200 and f2 % 400 < 230: p |= 0x01
    if seed >= 4 and f2 % 300 >= 250 and f2 % 300 < 270: p = (p & ~0x01) | 0x04
    return p


depth = [0]
addrs = set()
orig_irq = m.cpu.irq
def irq(vector):
    depth[0] += 1
    orig_irq(vector)
m.cpu.irq = irq
def tr(c, pc, op):
    if depth[0] and op == 0x40 and c.mpr[pc >> 13] == 0:
        depth[0] -= 1
m.cpu.trace = tr
ow = m.write
def w(a, v):
    if depth[0] and 0x1F0000 <= a < 0x1F2000:
        addrs.add(a - 0x1F0000)
    ow(a, v)
m.write = w
for f in range(frames):
    m.pad = pad(f)
    m.run_frame()
open(out, 'wb').write(bytes(1 if i in addrs else 0 for i in range(0x2000)))
print(len(addrs))

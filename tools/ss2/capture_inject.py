"""capture_inject.py OUT K FRAMES 'SCRIPT' PC [ADDR=VAL ...]

VBlank-bracketed pairs from snapshot.stage_start(K): at the first frame
start ($EC42) the RAM pokes (hex, $2000-based) are applied and task 0 jumps
to PC (e.g. F617 stage clear, F884 a miss). Replay with SS2_CONTINUOUS=1
hosttest/run.sh OUT ss2_frame_vbl_inject and SS2_TASK0=PC.
"""
import struct, sys
sys.setrecursionlimit(10000)
import snapshot
from run import parse_script

out, k, frames = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
ev = parse_script(sys.argv[4].split()) if sys.argv[4] else []
pc0 = int(sys.argv[5], 16)
pokes = [tuple(int(v, 16) for v in p.split('=')) for p in sys.argv[6:]]
m = snapshot.stage_start(k)
pairs, pend, done = [], [None], [0]


def tr(c, pc, op):
    if not done[0] and pc == 0xEC42 and c.mpr[7] == 0:
        for a, v in pokes:
            m.ram[(a - 0x2000) & 0x1FFF] = v
        c.pc = pc0
        done[0] = 1


ovbl = m.vblank


def vbl():
    snap = bytes(m.ram)
    if pend[0] is not None:
        pairs.append((m.frame, pend[0], snap))
    pend[0] = snap
    ovbl()


m.vblank = vbl
m.cpu.trace = tr
import os
SAVE = int(os.environ['SAVE']) if os.environ.get('SAVE') else None
SHOT = {int(v) for v in os.environ.get('SHOTS', '').split(',') if v}
for f in range(frames):
    m.pad = sum(b for a, z, b in ev if a <= f <= z)
    m.run_frame()
    if f in SHOT or f == SAVE:
        import render
        render.render(m).save('%s-%d.png' % (out, f))
    if f == SAVE:                            # the research machine, for converters
        import pickle
        m.cpu.trace, m.vblank = None, ovbl
        pickle.dump(m, open(os.environ['SAVEPKL'], 'wb'))
        break
with open(out, 'wb') as fh:
    fh.write(struct.pack('<4sI', b'SS2P', len(pairs)))
    for fr, a, b in pairs:
        fh.write(struct.pack('<H', fr & 0xFFFF))
        fh.write(a)
        fh.write(b)
print(len(pairs), 'pairs')

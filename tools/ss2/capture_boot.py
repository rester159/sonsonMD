"""capture_boot.py OUT FRAMES 'SCRIPT'

VBlank-bracketed work-RAM pairs (capture_vbl.py format) from power-on:
title, attract demo, intro, stage start. Replay with
SS2_CONTINUOUS=1 hosttest/run.sh OUT ss2_frame_vbl_boot (the native game
boots before the first pair). Keys as run.py (N = RUN, 1/2 = I/II).
"""
import struct, sys
sys.setrecursionlimit(10000)
import snapshot
from pce import PCE
from run import parse_script

out, frames = sys.argv[1], int(sys.argv[2])
ev = parse_script(sys.argv[3].split()) if len(sys.argv) > 3 and sys.argv[3] else []
m = PCE(open(snapshot.ROM, 'rb').read())
pairs, pend = [], [None]
ovbl = m.vblank


def vbl():
    snap = bytes(m.ram)
    if pend[0] is not None:
        pairs.append((m.frame, pend[0], snap))
    pend[0] = snap
    ovbl()


m.vblank = vbl
for f in range(frames):
    m.pad = sum(b for a, z, b in ev if a <= f <= z)
    m.run_frame()
with open(out, 'wb') as fh:
    fh.write(struct.pack('<4sI', b'SS2P', len(pairs)))
    for fr, a, b in pairs:
        fh.write(struct.pack('<H', fr & 0xFFFF))
        fh.write(a)
        fh.write(b)
print(len(pairs), 'pairs')

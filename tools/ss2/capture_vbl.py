"""capture_vbl.py OUT K FRAMES 'SCRIPT' [WX WY]

VBlank-bracketed work-RAM pairs: state at the start of one vertical blank
(before its IRQ) and at the start of the next, from snapshot.stage_start(K).
Unlike the $EC42 brackets these stay one frame long while the main task
runs a multi-frame sequence (doors, fades, messages); replay them with
SS2_CONTINUOUS=1 ... ss2_frame_vbl.

With WX WY the camera is moved at the first frame start so the player
(screen $70,$80) stands at world (WX, WY) and object slots are cleared
(the injection of research/bank09/capture_at.py). Prints the frames on
which the door entry $7DE3 and the $EDB0 restart ran.
"""
import struct, sys
sys.setrecursionlimit(10000)
import snapshot
from run import parse_script

out, k, frames = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
ev = parse_script(sys.argv[4].split()) if sys.argv[4] else []
inj = (int(sys.argv[5]), int(sys.argv[6])) if len(sys.argv) > 6 else None
m = snapshot.stage_start(k)
pairs, pend, hits = [], [None], []
done = [inj is None]


def inject():
    r = m.ram
    wx, wy = inj
    cx, cy = (wx - 0x70) & 0x7FF, (wy - 0x80) & 0x7FF
    for base in (0x45, 0x4B):
        r[base] = cx >> 8; r[base + 1] = cx & 0xFF; r[base + 2] = 0
        r[base + 3] = cy >> 8; r[base + 4] = cy & 0xFF; r[base + 5] = 0
    r[0x298] = 0; r[0x299] = 0x70; r[0x29A] = 0
    r[0x29B] = 0; r[0x29C] = 0x80; r[0x29D] = 0
    for x in range(0x20):
        r[0x45A + x] = 0
    for s in range(0x100):
        if r[0xB5A + s] != 0xFF:
            r[0xB5A + s] = 0


def tr(c, pc, op):
    if not done[0] and pc == 0xEC42 and c.mpr[7] == 0:
        inject()
        done[0] = 1
    if c.mpr[pc >> 13] == 2 and pc in (0x7DE3,):
        hits.append(('door', m.frame))
    if pc == 0xEDB0 and c.mpr[7] == 0 and hits and hits[-1][0] == 'door':
        hits.append(('edb0', m.frame))


ovbl = m.vblank


def vbl():
    if done[0]:
        snap = bytes(m.ram)
        if pend[0] is not None:
            pairs.append((m.frame, pend[0], snap))
        pend[0] = snap
    ovbl()


m.vblank = vbl
m.cpu.trace = tr
for f in range(frames):
    m.pad = sum(b for a, z, b in ev if a <= f <= z)
    m.run_frame()
with open(out, 'wb') as fh:
    fh.write(struct.pack('<4sI', b'SS2P', len(pairs)))
    for fr, a, b in pairs:
        fh.write(struct.pack('<H', fr & 0xFFFF))
        fh.write(a)
        fh.write(b)
print(len(pairs), 'pairs', hits)

"""cap.py OUT K FRAMES 'SCRIPT' WX WY DX [ADDR=VAL ...]

VBlank-bracketed work-RAM pairs (capture_vbl.py format, replay with
SS2_CONTINUOUS=1 tools/ss2/hosttest/run.sh OUT ss2_frame_vbl) next to a
townsperson at world (WX, WY) (decimal, a spawns.py record).

Townspeople ($17 $DEFB) only spawn off-screen, so the state injection has
two phases, both before anything is recorded:
 1. at the first frame start every object slot is cleared and the camera
    put so the record sits just past the right screen edge (screen x
    $120, y $A0); PRE (environment, default 8) frames run unrecorded and
    the townsperson spawns;
 2. at a frame start the camera and player are moved so the player stands
    on the nearest floor about DX px left of the record (negative DX: to
    the right), as research/bank17/play.py warp= does, and every live
    object's screen position is moved by the camera change (objects keep
    their world place). The RAM pokes (hex, $2000-based work RAM, e.g.
    207A=02 220B=09) are applied and recording starts.
SCRIPT frames count from phase 1. Logs the NPC/shop entry points reached.
SETTLE (environment, default 0) delays the recording by that many frames
after phase 2 (the first source frames after a warp can overrun a VBlank
while the spawn scan fills the slots). PROBE=1 prints the player and live
townspeople every 20 frames.
"""
import os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
sys.setrecursionlimit(10000)
import snapshot
from run import parse_script

out, k, frames = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
ev = parse_script(sys.argv[4].split()) if sys.argv[4] else []
tx, ty, tdx = int(sys.argv[5]), int(sys.argv[6]), int(sys.argv[7])
pokes = [tuple(int(v, 16) for v in p.split('=')) for p in sys.argv[8:]]
PRE = int(os.environ.get('PRE', 8))
SETTLE = int(os.environ.get('SETTLE', 0))
PROBE = bool(os.environ.get('PROBE'))
ROMB = open(snapshot.ROM, 'rb').read()
m = snapshot.stage_start(k)
pairs, pend, hits = [], [None], []
phase = [0, 0]                               # phase, frame of phase 1
WATCH = {(3, 0x997F): 'npc', (2, 0x6C12): 'cont', (3, 0x9E7B): 'shop',
         (4, 0xA414): 'shop-exit', (3, 0x9E41): 'give', (2, 0x6E47): 'death',
         (4, 0xA093): 'buy', (2, 0x6984): 'msg', (3, 0x9DFA): 'end'}


def attr(r, wx, wy):
    wx &= 0x7FF
    wy &= 0x7FF
    a = 0x3000 | ((wy >> 8) << 9) | ((wx >> 8) << 6) | ((wy & 0xE0) >> 2) | ((wx & 0xE0) >> 5)
    mt = r[a - 0x2000]
    q = ((wx & 0x10) >> 3) | ((wy & 0x10) >> 2)
    t = ((r[0x5E] << 8) | r[0x5D]) + mt * 8
    bank = 6 | ((r[0x69] >> 2) & 1)
    return ROMB[bank * 0x2000 + ((t + q + 1) & 0x1FFF)] & 0x0F


def solid(r, wx, wy):
    return attr(r, wx, wy) not in (0, 2)


def set_camera(r, cx, cy):
    r[0x45], r[0x46], r[0x47] = cx >> 8, cx & 0xFF, 0
    r[0x48], r[0x49], r[0x4A] = cy >> 8, cy & 0xFF, 0
    for i in range(6):
        r[0x4B + i] = r[0x45 + i]
    r[0x298], r[0x299], r[0x29A] = 0, 0x80, 0
    r[0x29B], r[0x29C], r[0x29D] = 0, 0xA0, 0
    r[0x2A2] = r[0x2A3] = r[0x2A4] = r[0x2A5] = 0


def phase1():
    r = m.ram
    for x in range(32):
        if r[0x45A + x]:
            s = r[0x81A + x]
            if s != 0xBF and r[0xB5A + s] not in (0, 0xFF):
                r[0xB5A + s] -= 1
            r[0x45A + x] = 0
    set_camera(r, (tx - 0x120) & 0x7FF, (ty - 0xA0) & 0x7FF)


def phase2():
    r = m.ram
    best = None
    for off in range(0, 0x80, 8):
        for sgn in (1, -1):
            px = (tx - tdx + sgn * off) & 0x7FF
            for py0 in sorted(range(-0x60, 0x70, 0x10), key=abs):
                py = ((ty + py0) & 0x7F0)
                if solid(r, px + 0x10, py + 0x20) and not solid(r, px + 0x10, py + 0x10) and not solid(r, px + 0x10, py):
                    best = (px, py)
                    break
            if best:
                break
        if best:
            break
    if not best:
        print('warp: no floor found'); best = ((tx - tdx) & 0x7FF, ty & 0x7F0)
    px, py = best
    ocx, ocy = r[0x45] << 8 | r[0x46], r[0x48] << 8 | r[0x49]
    cx, cy = (px - 0x80) & 0x7FF, (py - 0xA0) & 0x7FF
    dx = ((cx - ocx + 0x400) & 0x7FF) - 0x400
    dy = ((cy - ocy + 0x400) & 0x7FF) - 0x400
    for x in range(32):
        if r[0x45A + x]:
            for hi, lo, d in ((0x47A, 0x49A, dx), (0x4DA, 0x4FA, dy)):
                v = ((r[hi + x] << 8 | r[lo + x]) - d) & 0xFFFF
                r[hi + x], r[lo + x] = v >> 8, v & 0xFF
    set_camera(r, cx, cy)
    for a, v in pokes:
        r[(a - 0x2000) & 0x1FFF] = v
    print('warp: frame %d player world %d,%d camera %03X,%03X' % (m.frame, px, py, cx, cy))


def tr(c, pc, op):
    if phase[0] < 2 and pc == 0xEC42 and c.mpr[7] == 0:
        if phase[0] == 0:
            phase1()
            phase[:] = [1, m.frame]
        elif m.frame - phase[1] >= PRE:
            phase2()
            phase[:] = [2, m.frame]
    w = WATCH.get((c.mpr[pc >> 13], pc))
    if w:
        hits.append((w, m.frame))


ovbl = m.vblank


def vbl():
    if phase[0] == 2 and m.frame - phase[1] >= SETTLE:
        snap = bytes(m.ram)
        if pend[0] is not None:
            pairs.append((m.frame, pend[0], snap))
        pend[0] = snap
    ovbl()


m.vblank = vbl
m.cpu.trace = tr
f0 = m.frame
for f in range(frames):
    m.pad = sum(b for a, z, b in ev if a <= f <= z)
    m.run_frame()
    if PROBE and f % 20 == 19:
        r = m.ram
        cx = (r[0x45] << 8 | r[0x46]); cy = (r[0x48] << 8 | r[0x49])
        px = cx + (r[0x298] << 8 | r[0x299]); py = cy + (r[0x29B] << 8 | r[0x29C])
        objs = ['%d:t%d s%02X (%d,%d)' % (x, r[0x97A + x], r[0x81A + x],
                (r[0x47A + x] << 8 | r[0x49A + x]), (r[0x4DA + x] << 8 | r[0x4FA + x]))
                for x in range(0x20) if r[0x45A + x] and r[0x6BA + x] == 29]
        print(f, 'player', px & 0x7FF, py & 0x7FF, 'AA B1 B0', r[0x2AA], r[0x2B1], r[0x2B0], objs,
              [(w, fr - f0) for w, fr in hits[-3:]])
with open(out, 'wb') as fh:
    fh.write(struct.pack('<4sI', b'SS2P', len(pairs)))
    for fr, a, b in pairs:
        fh.write(struct.pack('<H', fr & 0xFFFF))
        fh.write(a)
        fh.write(b)
print(len(pairs), 'pairs', [(w, fr - f0) for w, fr in hits])
r = m.ram
print('final money', bytes(r[0x20B:0x213]).hex(), '7A/7B/7E', hex(r[0x7A]), hex(r[0x7B]), hex(r[0x7E]),
      'C1', hex(r[0x2C1]), 'BF', hex(r[0x2BF]), 'life', r[0x2B8], r[0x2B9], 'magic', r[0x2BB], '72', r[0x72],
      'prices', bytes(r[0x98:0xB8]).hex())

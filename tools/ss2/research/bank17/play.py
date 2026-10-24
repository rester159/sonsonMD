"""play.py OUT K FRAMES [BOT|'SCRIPT'] [opts]

Whole-frame research capture from snapshot.stage_start(K) (K = 0..6).
Writes OUT (capture_pairs format: pre/post work RAM at consecutive $EC42
visits), OUT.cov (executed (bank, pc) counts for banks $17/$07 at $C000 and
fixed bank 4 at $A000) and OUT.obj ({(bank, normal, hit handler): frames}).

BOT: 'explore' (census bot: walk right, sometimes left, attack, jump),
     'left' (mirror), or a run.py style script 'a-b:KEYS ...'.
opts: life   -- refill life at each frame start (before the pre snapshot,
                so the pair stays a faithful record of one source frame)
      seed=N -- vary the bot phase
      warp=SX,SY,X,Y[,DX] -- before the first pair: clear the objects and
                move camera and player to stand on the nearest floor about
                DX (default $90) px left of world point screen (SX,SY) +
                (X,Y), a spawn record position (see spawns.py -v); with a
                negative DX the player stands to the right
      magic  -- refill magic like life
      weak   -- each frame start: bank $17/$07 objects keep at most 1 HP
      cloud  -- with warp: give the cloud ($22C1 bit 3)
      boss   -- with warp: set the boss-room lock $7F once
      solo   -- at each frame start remove objects of the other code banks
                ($05/$09/$0F/$1F) and block their spawners ($2B5A = $FF),
                so the frames exercise banks $17/$07/fixed 4 in isolation
"""
import os, pickle, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))
sys.setrecursionlimit(10000)
import snapshot
from run import parse_script

out, k, frames = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
bot = sys.argv[4] if len(sys.argv) > 4 else 'explore'
opts = sys.argv[5:]
seed = 0
for o in opts:
    if o.startswith('seed='):
        seed = int(o[5:])
life = 'life' in opts
magic = 'magic' in opts
warp = None
for o in opts:
    if o.startswith('warp='):
        v = [int(t, 16) if not t.startswith('-') else -int(t[1:], 16) for t in o[5:].split(',')]
        warp = (v[0] * 256 + v[2], v[1] * 256 + v[3], v[4] if len(v) > 4 else 0x90)
m = snapshot.stage_start(k)
m.cpu.trace = None
ev = parse_script(bot.split()) if ':' in bot else None


def pad_for(f):
    if ev is not None:
        return sum(b for a, z, b in ev if a <= f <= z)
    g = f + seed * 97
    phase = g % 600
    right, left = (0x02, 0x08) if bot != 'left' else (0x08, 0x02)
    p = left if 420 <= phase < 520 else right
    if g % 12 < 3: p |= 0x20
    if 30 <= g % 90 < 40: p |= 0x10
    if 200 <= g % 400 < 230: p |= 0x01
    return p


ROMB = open(snapshot.ROM, 'rb').read()
OTHER = (0x05, 0x09, 0x0F, 0x1F)
solo = 'solo' in opts
blocked = set()
if solo:
    bb = lambda bank, a: ROMB[bank * 0x2000 + (a & 0x1FFF)]
    lst = bb(4, 0xBD08 + 2 * k) | bb(4, 0xBD09 + 2 * k) << 8
    for scr in range(64):
        rec = bb(8, lst + 2 * scr) | bb(8, lst + 2 * scr + 1) << 8
        while bb(8, rec) != 0xFF:
            if bb(8, rec + 3) in OTHER:
                blocked.add(bb(8, rec + 2))
            rec += 6


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


def do_warp(r):
    tx, ty, dx = warp
    best = None
    for off in range(0, 0x80, 8):
        for sgn in (1, -1):
            px = (tx - dx + sgn * off) & 0x7FF
            for py0 in range(-0x60, 0x70, 0x10):
                py = ((ty + py0) & 0x7F0)
                # feet at py + $20 on solid, body clear
                if solid(r, px + 0x10, py + 0x20) and not solid(r, px + 0x10, py + 0x10) and not solid(r, px + 0x10, py):
                    best = (px, py)
                    break
            if best:
                break
        if best:
            break
    if not best:
        print('warp: no floor found'); best = ((tx - dx) & 0x7FF, ty & 0x7F0)
    px, py = best
    cx = (px - 0x80) & 0x7FF
    cy = (py - 0xA0) & 0x7FF
    for x in range(32):
        if r[0x45A + x]:
            s = r[0x81A + x]
            if s != 0xBF and r[0xB5A + s] not in (0, 0xFF):
                r[0xB5A + s] -= 1
            r[0x45A + x] = 0
    r[0x45], r[0x46], r[0x47] = cx >> 8, cx & 0xFF, 0
    r[0x48], r[0x49], r[0x4A] = cy >> 8, cy & 0xFF, 0
    for i in range(6):
        r[0x4B + i] = r[0x45 + i]
    r[0x298], r[0x299], r[0x29A] = 0, 0x80, 0
    r[0x29B], r[0x29C], r[0x29D] = 0, 0xA0, 0
    r[0x2A2] = r[0x2A3] = r[0x2A4] = r[0x2A5] = 0
    print('warp: player world %03X,%03X camera %03X,%03X' % (px, py, cx, cy))


pairs = []
pending = [None]
cov = {}
objs = {}


def tr(c, pc, op):
    b = c.mpr[pc >> 13]
    if (0xC000 <= pc < 0xE000 and b in (0x17, 0x07)) or (0xA000 <= pc < 0xC000 and b == 4):
        key = (b, pc)
        cov[key] = cov.get(key, 0) + 1
    if pc == 0xEC42 and c.mpr[7] == 0:
        if pending[0] is not None:
            pairs.append((m.frame, pending[0], bytes(m.ram)))
        if warp and pending[0] is None:
            do_warp(m.ram)
            if 'cloud' in opts:
                m.ram[0x2C1] |= 0x08
            if 'boss' in opts:
                m.ram[0x7F] = 1       # boss spawners wait for the room lock
        if 'weak' in opts:
            for x in range(32):
                if m.ram[0x45A + x] and m.ram[0x75A + x] in (0x17, 0x07) and m.ram[0x6DA + x] > 1:
                    m.ram[0x6DA + x] = 1
        if solo:
            for x in range(32):
                if m.ram[0x45A + x] and m.ram[0x75A + x] in OTHER:
                    m.ram[0x45A + x] = 0
            for sp in blocked:
                m.ram[0xB5A + sp] = 0xFF
        if magic:
            m.ram[0x2BB] = m.ram[0x2BC]
            m.ram[0x2BD] = m.ram[0x2BC]
        if life:
            m.ram[0x2B8] = m.ram[0x2B9]
            m.ram[0x2BA] = m.ram[0x2B9]
        pending[0] = bytes(m.ram)
        r = memoryview(m.ram)[0:0x2000]
        R = lambda a: r[a - 0x2000]
        for x in range(32):
            if R(0x245A + x):
                key = (R(0x275A + x), R(0x28BA + x) << 8 | R(0x28DA + x), R(0x28FA + x) << 8 | R(0x291A + x))
                objs[key] = objs.get(key, 0) + 1


m.cpu.trace = tr
for f in range(frames):
    m.pad = pad_for(f)
    m.run_frame()
with open(out, 'wb') as fh:
    fh.write(struct.pack('<4sI', b'SS2P', len(pairs)))
    for fr, a, b in pairs:
        fh.write(struct.pack('<H', fr & 0xFFFF))
        fh.write(a)
        fh.write(b)
pickle.dump(cov, open(out + '.cov', 'wb'))
pickle.dump(objs, open(out + '.obj', 'wb'))
print(len(pairs), 'pairs; stage', k + 1, 'camera %02X%02X,%02X%02X' % (m.ram[0x45], m.ram[0x46], m.ram[0x48], m.ram[0x49]),
      'life', m.ram[0x2B8], 'mode %02X' % m.ram[0x297])

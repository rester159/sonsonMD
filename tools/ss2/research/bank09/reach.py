"""reach.py K FRAMES ['SCRIPT' | bot] -- which bank $09/$0F/$1F object code
runs when playing stage index K from snapshot.stage_start(K).

Logs the first frame each executed (bank, pc) in bank $09 >= $DC00, $0F and
$1F appears, plus the player world position every 100 frames.
"""
import os, sys
sys.setrecursionlimit(10000)
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))
import snapshot
from run import parse_script

k, frames = int(sys.argv[1]), int(sys.argv[2])
script = sys.argv[3] if len(sys.argv) > 3 else ''
m = snapshot.stage_start(k)
ev = parse_script(script.split()) if script and script != 'bot' else None
seen = {}
cur = [0]


def tr(c, pc, op):
    if 0xC000 <= pc < 0xE000:
        b = c.mpr[6]
        if (b == 9 and pc >= 0xDC00) or b in (0x0F, 0x1F):
            if (b, pc) not in seen:
                seen[(b, pc)] = cur[0]


def pos():
    r = m.ram
    cx = (r[0x45] << 8 | r[0x46]) + (r[0x298] << 8 | r[0x299])
    cy = (r[0x48] << 8 | r[0x49]) + (r[0x29B] << 8 | r[0x29C])
    return cx & 0x7FF, cy & 0x7FF


m.cpu.trace = tr
for f in range(frames):
    cur[0] = f
    if ev is not None:
        m.pad = sum(b for a, z, b in ev if a <= f <= z)
    else:
        phase = f % 600
        pad = 0x08 if 420 <= phase < 520 else 0x02
        if f % 12 < 3: pad |= 0x20
        if 30 <= f % 90 < 40: pad |= 0x10
        if 200 <= f % 400 < 230: pad |= 0x01
        m.pad = pad
    m.run_frame()
    if f % 100 == 0:
        print('f %5d player world %s stage %d lock7F %02X' % (f, pos(), m.ram[0x69], m.ram[0x7F]))
# entry points: executed addresses whose predecessor was not executed
by = {}
for (b, pc), fr in seen.items():
    by.setdefault(b, []).append((pc, fr))
for b in sorted(by):
    pcs = sorted(by[b])
    runs = []
    for pc, fr in pcs:
        if runs and pc - runs[-1][1] <= 3:
            runs[-1][1] = pc
            runs[-1][2] = min(runs[-1][2], fr)
        else:
            runs.append([pc, pc, fr])
    for a, z, fr in runs:
        print('bank %02X %04X-%04X first frame %d' % (b, a, z, fr))

"""capture_magic.py OUT FRAMES 'SCRIPT'

Work-RAM pairs from the stage-1 snapshot with the player's magic state
injected, for the shot/magic system (bank 9 $D03D) and the magic launch
paths ($D1E8 from the staff + jump buttons, pause selection $FE0C/$FEAE).

Injection (documented state edit, applied once at the first frame start
$EC42): INJ='22BB=10,22BC=10,22BD=10,22BF=3F,22C0=1' sets work-RAM bytes
(hex address = hex value). Magic is cast with II+I pressed together
('2' and '1' in the script) when $22C0 is non-zero and $22BB covers the
cost at bank 1 $4CEA + $22C0; $22BF is the owned-magic mask tested by the
pause selection (RUN pauses, R / L cycle, RUN resumes).

Brackets: whole frame ($EC42 .. next $EC42, replay with
ss2_frame_tick_then_vblank) or, with VBL=1, VBlank to VBlank (replay with
SS2_CONTINUOUS=1 ... ss2_frame_vbl), needed while the pause loop runs.
Prints the frames on which the magic entry points ran; COV=FILE appends
the executed source addresses of the ported ranges (bank 9 $C4B8-$C564,
$CE4B-$D9FA, bank 0 $FE0C-$FECB) to FILE, one hex address per line.
"""
import os, struct, sys
sys.setrecursionlimit(10000)
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))
import snapshot
from run import parse_script

out, frames = sys.argv[1], int(sys.argv[2])
ev = parse_script(sys.argv[3].split()) if sys.argv[3] else []
VBL = bool(os.environ.get('VBL'))
inj = []
for it in filter(None, os.environ.get('INJ', '').split(',')):
    a, v = it.split('=')
    inj.append((int(a, 16), int(v, 16)))
m = snapshot.stage1_start()
pairs, pend, done = [], [None], [False]
WATCH = {0xD1E8: 'cast', 0xD1FB: 'm1', 0xD351: 'm2', 0xD375: 'm3', 0xD78F: 'm5',
         0xD622: 'shield', 0xD702: 'unshield', 0xD7BC: 'sweep', 0xCE74: 'cloud',
         0xD4A9: 'pillar', 0xD32E: 'fizzle', 0xFE0C: 'select', 0xC4D7: 'c4d7', 0xC4B8: 'c4b8'}
hits = {}
COV = os.environ.get('COV')
cov = set()


def tr(c, pc, op):
    if c.mpr[7] != 0:
        return
    if pc in WATCH and (pc >= 0xE000 or c.mpr[6] == 9):
        hits.setdefault(WATCH[pc], []).append(m.frame)
    if COV and ((c.mpr[6] == 9 and (0xC4B8 <= pc < 0xC565 or 0xCE4B <= pc < 0xD9FB))
                or 0xFE0C <= pc < 0xFECC):
        cov.add(pc)
    if pc != 0xEC42:
        return
    if not done[0]:
        for a, v in inj:
            m.ram[a - 0x2000] = v
        done[0] = True
    if VBL:
        return
    if pend[0] is not None:
        pairs.append((m.frame, pend[0], bytes(m.ram)))
    pend[0] = bytes(m.ram)


ovbl = m.vblank


def vbl():
    if VBL and done[0]:
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
print(len(pairs), 'pairs')
if COV:
    with open(COV, 'a') as fh:
        fh.write(''.join('%04X\n' % a for a in sorted(cov)))
for k, v in sorted(hits.items()):
    print('  %-9s %d frames, first %s' % (k, len(v), v[:6]))

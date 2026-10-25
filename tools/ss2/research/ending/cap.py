"""cap.py OUT FRAMES 'SCRIPT' [ADDR=VAL ...]

The ending (bank 4 $AD5A) from snapshot.stage_start(6) (stage 7): at the
first frame start ($EC42, bank 0 mapped) the RAM pokes (hex, $2000-based
work RAM, e.g. 220B=03) are applied and the PC goes to $F617 (the stage
clear, which with $69 = 6 jumps to $AD5A). Records VBlank-bracketed pairs
(capture_vbl.py format); the first pair runs from the injection to the
next VBlank, so the replay driver starts the native tasks there
(hosttest/frame_tick.c ss2_frame_vbl_ending). Keys as run.py (1 = I).

Logs (pair index, scanline) of the first hits of the JSR return points in
bank 4 $AD5A-$B2C0 (to measure source steps that hold the CPU across
VBlanks), to OUT.log. SAVE=i:path,i:path pickles the machine when pair i
is complete (at that VBlank, before its IRQ); SAVEF=f:path,... pickles it
after script frame f (a frame boundary: the screens for the converters).
"""
import os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
sys.setrecursionlimit(10000)
import snapshot, huc6280
from run import parse_script

out, frames = sys.argv[1], int(sys.argv[2])
ev = parse_script(sys.argv[3].split()) if len(sys.argv) > 3 and sys.argv[3] else []
pokes = [(int(a, 16) - 0x2000, int(v, 16)) for a, v in (p.split('=') for p in sys.argv[4:])]
saves = {}
for it in filter(None, os.environ.get('SAVE', '').split(',')):
    i, p = it.split(':')
    saves[int(i)] = p
savef = {}
for it in filter(None, os.environ.get('SAVEF', '').split(',')):
    i, p = it.split(':')
    savef[int(i)] = p

# JSR/BSR return points in the ending's main line and its helpers
ROMB = open(snapshot.ROM, 'rb').read()
rd = lambda a: ROMB[4 * 0x2000 + (a & 0x1FFF)]          # bank 4 at $A000
rets = set()
pc = 0xAD5A
while pc < 0xB4D3:
    t, k, _ = huc6280.disasm_one(rd, pc)
    if t.startswith('JSR') or t.startswith('BSR'):
        rets.add(pc + k)
    pc += k

m = snapshot.stage_start(6)
pairs, pend, log = [], [None], []
done = [0]
hits = {}


def tr(c, pc, op):
    if not done[0]:
        if pc == 0xEC42 and c.mpr[7] == 0:
            for a, v in pokes:
                m.ram[a] = v
            c.pc = 0xF617
            done[0] = 1
            pend[0] = bytes(m.ram)
        return
    if c.mpr[5] == 4 and pc in rets:
        n = hits.get(pc, 0)
        if n < 3:
            hits[pc] = n + 1
            log.append('%04X pair %d line %d' % (pc, len(pairs), m.line))


ovbl = m.vblank


def vbl():
    if done[0]:
        snap = bytes(m.ram)
        pairs.append((m.frame, pend[0], snap))
        pend[0] = snap
        if len(pairs) in saves:
            import pickle
            t, m.cpu.trace = m.cpu.trace, None
            v, m.vblank = m.vblank, ovbl
            pickle.dump(m, open(saves[len(pairs)], 'wb'))
            m.cpu.trace, m.vblank = t, v
    ovbl()


m.vblank = vbl
m.cpu.trace = tr
for f in range(frames):
    m.pad = sum(b for a, z, b in ev if a <= f <= z)
    m.run_frame()
    if f in savef:
        import pickle
        t, m.cpu.trace = m.cpu.trace, None
        v, m.vblank = m.vblank, ovbl
        pickle.dump(m, open(savef[f], 'wb'))
        m.cpu.trace, m.vblank = t, v
with open(out, 'wb') as fh:
    fh.write(struct.pack('<4sI', b'SS2P', len(pairs)))
    for fr, a, b in pairs:
        fh.write(struct.pack('<H', fr & 0xFFFF))
        fh.write(a)
        fh.write(b)
open(out + '.log', 'w').write('\n'.join(log) + '\n')
print(len(pairs), 'pairs')

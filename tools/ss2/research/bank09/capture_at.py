"""capture_at.py OUT FRAMES 'SCRIPT'|bot K [WX WY]

Whole-frame RAM pairs ($EC42 .. next $EC42, like capture_pairs.py with
ENTRY = EXIT = EC42) starting from snapshot.stage_start(K).

With WX WY (world pixels, decimal) the research state is injected at the
first frame start, before anything is recorded: the camera ($45-$4A and
the previous-frame copy $4B-$50) is moved so the player (screen x $70,
y $80) stands at world (WX, WY), and every object slot is cleared. The
stage map lives whole in work RAM ($3000-$3FFF), so collision and
spawning are consistent after the move; only the VDC picture (not
compared) is stale. LOCK7F=1 also sets the boss-room lock $7F that the
boss door (bank 2 $7E08) sets, so boss spawns run outside their room. ISOLATE=1 blocks every spawner of the
stage whose records use other code banks (live count $2B5A = $FF, which
the spawn scan skips), so frames are not lost to objects not yet rebuilt;
it applies with or without WX WY. This is a documented state injection used to start
traces next to objects that the scripted bot does not reach.
"""
import os, struct, sys
sys.setrecursionlimit(10000)
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))
import snapshot
from run import parse_script

out, frames = sys.argv[1], int(sys.argv[2])
BOT = sys.argv[3] == 'bot'
ev = parse_script(sys.argv[3].split()) if sys.argv[3] and not BOT else []


def bot(f):
    # the census explorer: right (left for 100 of every 600 frames),
    # attack, jump and up on fixed cycles
    pad = 0x08 if 420 <= f % 600 < 520 else 0x02
    if f % 12 < 3: pad |= 0x20
    if 30 <= f % 90 < 40: pad |= 0x10
    if 200 <= f % 400 < 230: pad |= 0x01
    return pad
k = int(sys.argv[4])
inj = (int(sys.argv[5]), int(sys.argv[6])) if len(sys.argv) > 6 else None
m = snapshot.stage_start(k)
pairs = []
pending = [None]
ISOLATE = bool(os.environ.get('ISOLATE'))
done = [inj is None and not ISOLATE]


def other_spawners():
    # spawner ids of this stage's records whose code bank is not $09/$0F/$1F
    rom = m.rom
    def b(bank, a): return rom[bank * 0x2000 + (a & 0x1FFF)]
    lst = b(4, 0xBD08 + 2 * k) | b(4, 0xBD09 + 2 * k) << 8
    mine, other = set(), set()
    for scr in range(64):
        r = b(8, lst + 2 * scr) | b(8, lst + 2 * scr + 1) << 8
        while b(8, r) != 0xFF:
            (mine if b(8, r + 3) in (9, 0x0F, 0x1F) else other).add(b(8, r + 2))
            r += 6
    return other - mine


def inject():
    r = m.ram
    if ISOLATE:
        # spawn scan skips spawners whose live count is $FF (bank 4 $BC10)
        for sp in other_spawners():
            r[0xB5A + sp] = 0xFF
        for x in range(0x20):
            if r[0x75A + x] not in (9, 0x0F, 0x1F):
                r[0x45A + x] = 0
    if inj is None:
        return
    wx, wy = inj
    cx = (wx - 0x70) & 0x7FF
    cy = (wy - 0x80) & 0x7FF
    for base in (0x45, 0x4B):
        r[base] = cx >> 8; r[base + 1] = cx & 0xFF; r[base + 2] = 0
        r[base + 3] = cy >> 8; r[base + 4] = cy & 0xFF; r[base + 5] = 0
    r[0x298] = 0; r[0x299] = 0x70; r[0x29A] = 0
    r[0x29B] = 0; r[0x29C] = 0x80; r[0x29D] = 0
    for x in range(0x20):
        r[0x45A + x] = 0
    if os.environ.get('LOCK7F'):
        r[0x7F] = 1                     # boss-room lock (normally set by the door, bank 2 $7E08)
    for s in range(0x100):
        if r[0xB5A + s] != 0xFF:
            r[0xB5A + s] = 0


# bytes written from interrupt context during this capture (as
# tools/ss2/irq_mask.py does for its stage-1 run): saved as OUT.irq, OR-ed
# into the mask by replay.c (other stages' music touches more driver RAM)
irq_depth = [0]
irq_addrs = set()
orig_irq = m.cpu.irq


def irq(vector):
    irq_depth[0] += 1
    orig_irq(vector)


m.cpu.irq = irq
orig_write = m.write


def write(a, v):
    if irq_depth[0] and 0x1F0000 <= a < 0x1F2000:
        irq_addrs.add(a - 0x1F0000)
    orig_write(a, v)


m.write = write


def tr(c, pc, op):
    if irq_depth[0] and op == 0x40 and c.mpr[pc >> 13] == 0:   # RTI
        irq_depth[0] -= 1
    if c.mpr[7] != 0 or pc != 0xEC42:
        return
    if not done[0]:
        inject()
        done[0] = 1
    if pending[0] is not None:
        pairs.append((m.frame, pending[0], bytes(m.ram)))
    pending[0] = bytes(m.ram)


m.cpu.trace = tr
for f in range(frames):
    m.pad = bot(f) if BOT else sum(b for a, z, b in ev if a <= f <= z)
    m.run_frame()
with open(out, 'wb') as fh:
    fh.write(struct.pack('<4sI', b'SS2P', len(pairs)))
    for fr, a, b in pairs:
        fh.write(struct.pack('<H', fr & 0xFFFF))
        fh.write(a)
        fh.write(b)
open(out + '.irq', 'wb').write(bytes(1 if i in irq_addrs else 0 for i in range(0x2000)))
r = m.ram
print(len(pairs), 'pairs; end player world (%d, %d)' % (
    ((r[0x45] << 8 | r[0x46]) + (r[0x298] << 8 | r[0x299])) & 0x7FF,
    ((r[0x48] << 8 | r[0x49]) + (r[0x29B] << 8 | r[0x29C])) & 0x7FF))

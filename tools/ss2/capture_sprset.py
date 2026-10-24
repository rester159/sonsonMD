"""capture_sprset.py OUT STAGE_BEFORE FRAMES

Warp from the stage-1 snapshot into the stage-clear path ($F861 with
$69 = STAGE_BEFORE) and record work RAM around each call of the bank 2
sprite-set loaders $774F, $77A5 and $77D0 (entry .. their RTS). Writes one
pairs file per loader, OUT_774F.bin etc., in capture_pairs.py's format.
"""
import struct, sys
sys.setrecursionlimit(10000)
import snapshot

out, before, frames = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
ENT = {0x774F: 0x77A4, 0x77A5: 0x77CF, 0x77D0: 0x7802}
m = snapshot.stage1_start()
pairs, pend, done = [], {}, [0]


def tr(c, pc, op):
    if not done[0] and pc == 0xEC42 and c.mpr[7] == 0:
        m.ram[0x69] = before
        c.pc = 0xF861
        done[0] = 1
        return
    if c.mpr[pc >> 13] != 2:
        return
    if pc in ENT:
        pend[ENT[pc]] = (pc, bytes(m.ram))
    elif pc in pend:
        e, a = pend.pop(pc)
        pairs.append((e, m.frame, a, bytes(m.ram)))


m.cpu.trace = tr
for f in range(frames):
    m.pad = 0x80 if (f % 150) in range(60, 64) else 0
    m.run_frame()
for e in ENT:
    sel = [p for p in pairs if p[0] == e]
    with open('%s_%04X.bin' % (out, e), 'wb') as fh:
        fh.write(struct.pack('<4sI', b'SS2P', len(sel)))
        for _, fr, a, b in sel:
            fh.write(struct.pack('<H', fr & 0xFFFF))
            fh.write(a)
            fh.write(b)
    print('%04X' % e, len(sel), 'pairs')

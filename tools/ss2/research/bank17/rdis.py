"""rdis.py BANK LO HI SEED... [cov=FILE...] -- flow-following disassembly of
one bank window (LO..HI), seeded by entry points, executed PCs from play.py
.cov files and handler/script pointers found in 22-byte object records
(LDA #lo / STA <$26 / LDA #hi / STA <$27 before JSR $8451) and in direct
stores to $28DA/$28BA, $291A/$28FA. Jump tables (JMP ($T,X)) are printed;
add their targets as seeds by hand.
"""
import os, pickle, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))
import huc6280
ROOT = HERE.rsplit('/tools/', 1)[0]
rom = open(ROOT + '/arcade/Son_Son_II_J_TEng.pce', 'rb').read()

bank = int(sys.argv[1], 16)
lo, hi = int(sys.argv[2], 16), int(sys.argv[3], 16)
seeds, covs = [], []
for a in sys.argv[4:]:
    if a.startswith('cov='):
        covs.append(a[4:])
    else:
        seeds.append(int(a, 16))
mpr = [0xFF, 0xF8, 1, 2, 3, 4, bank if lo >= 0xC000 else 5, 0]


def rd(a):
    a &= 0xFFFF
    b = mpr[a >> 13]
    if b >= 0x80:
        return 0
    return rom[(b % 32) * 0x2000 + (a & 0x1FFF)]


for c in covs:
    for (b, pc), n in pickle.load(open(c, 'rb')).items():
        if b == bank and lo <= pc < hi:
            seeds.append(pc)
code = {}
records = set()
work = list(seeds)
labels = set(seeds)
notes = {}
while work:
    pc = work.pop()
    while lo <= pc < hi and pc not in code:
        t, n, tg = huc6280.disasm_one(rd, pc)
        code[pc] = (t, n)
        op = rd(pc)
        for x in tg:
            labels.add(x)
            if lo <= x < hi:
                work.append(x)
        # record pointer pattern
        if op == 0xA9 and rd(pc + 2) == 0x85 and rd(pc + 3) == 0x26 and rd(pc + 4) == 0xA9 and rd(pc + 6) == 0x85 and rd(pc + 7) == 0x27:
            r = rd(pc + 1) | rd(pc + 5) << 8
            records.add(r)
            if lo <= r < hi:
                for o in (16, 18):
                    v = rd(r + o) | rd(r + o + 1) << 8
                    if lo <= v < hi:
                        labels.add(v)
                        work.append(v)
        if op == 0xA9 and rd(pc + 2) == 0x9D and rd(pc + 4) in (0x28, 0x29) and rd(pc + 5) == 0xA9 and rd(pc + 7) == 0x9D:
            f1 = rd(pc + 3) | rd(pc + 4) << 8
            f2 = rd(pc + 8) | rd(pc + 9) << 8
            v = rd(pc + 1) | rd(pc + 6) << 8
            if (f1, f2) in ((0x28DA, 0x28BA), (0x291A, 0x28FA)):
                labels.add(v)
                work.append(v)
        if op == 0x20 and (rd(pc + 1) | rd(pc + 2) << 8) in (0x8428, 0x8436):
            labels.add(pc + 4)
            work.append(pc + 4)       # alloc success returns past the RTS
        if op == 0x7C:
            notes[pc] = 'JUMP TABLE'
        if op in (0x60, 0x40, 0x4C, 0x6C, 0x7C, 0x80):
            break
        pc += n
for r in sorted(records):
    if lo <= r < hi:
        b = [rd(r + i) for i in range(22)]
        h = b[16] | b[17] << 8
        hh = b[18] | b[19] << 8
        s = b[20] | b[21] << 8
        print('; record $%04X: handler $%04X hit $%04X script $%04X  bank %02X' % (r, h, hh, s, b[8]))
        for x in (h, hh):
            if lo <= x < hi and x not in code:
                print(';   (re-run with seed %04X)' % x)
last = None
for pc in sorted(code):
    if last is not None and pc != last:
        print('        ; ---- data $%04X-$%04X: %s' % (last, pc - 1, ' '.join('%02X' % rd(a) for a in range(last, min(pc, last + 24)))))
    t, n = code[pc]
    lab = 'L%04X:' % pc if pc in labels else ''
    print('%-7s %04X  %-12s %s%s' % (lab, pc, ' '.join('%02X' % rd(pc + i) for i in range(n)), t, '   ; ' + notes[pc] if pc in notes else ''))
    last = pc + n

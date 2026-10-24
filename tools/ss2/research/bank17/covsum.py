"""covsum.py ASM COVFILE... -- executed/total instructions per labelled
block of an rdis.py listing, over the union of play.py .cov files; lists
the unexecuted labels."""
import pickle, re, sys
asm = open(sys.argv[1]).read().splitlines()
cov = set()
for c in sys.argv[2:]:
    cov |= {pc for (b, pc) in pickle.load(open(c, 'rb'))}
blocks, cur = [], None
for ln in asm:
    m = re.match(r'(L[0-9A-F]{4}:)?\s+([0-9A-F]{4})  ', ln)
    if not m:
        continue
    pc = int(m.group(2), 16)
    if m.group(1):
        cur = [pc, 0, 0]
        blocks.append(cur)
    if cur:
        cur[1] += 1
        cur[2] += pc in cov
tot = sum(b[1] for b in blocks)
hit = sum(b[2] for b in blocks)
print('instructions executed: %d / %d' % (hit, tot))
print('never entered:', ' '.join('%04X' % b[0] for b in blocks if not b[2]))

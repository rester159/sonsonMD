"""irq_mask.py OUT -- work-RAM bytes written from interrupt context.

Runs varied stage-1 play and records every RAM write made while an IRQ
handler is active (VDC IRQ $E0B9..$E1A4, pad read $FEDE.., sound driver in
banks $0A/$0B). Routine-level host tests mask these: an interrupt can land
inside any bracketed routine.
"""
import sys
sys.setrecursionlimit(10000)
import snapshot
from run import parse_script
m = snapshot.stage1_start()
ev = parse_script("0-300:R 40-46:1 90-96:2 300-500:L 320-330:1 500-1200:R 600-640:1 700-704:2 900-910:1".split())
depth = [0]
addrs = set()
orig_irq = m.cpu.irq
def irq(vector):
    depth[0] += 1
    orig_irq(vector)
m.cpu.irq = irq
def tr(c, pc, op):
    if depth[0] and op == 0x40 and c.mpr[pc >> 13] == 0:   # RTI
        depth[0] -= 1
m.cpu.trace = tr
ow = m.write
def w(a, v):
    if depth[0] and 0x1F0000 <= a < 0x1F2000:
        addrs.add(a - 0x1F0000)
    ow(a, v)
m.write = w
for f in range(1200):
    m.pad = sum(b for a, z, b in ev if a <= f <= z)
    m.run_frame()
open(sys.argv[1], 'wb').write(bytes(1 if i in addrs else 0 for i in range(0x2000)))
print(len(addrs), 'bytes:', ' '.join('$%04X' % (a + 0x2000) for a in sorted(addrs) if a < 0x100))

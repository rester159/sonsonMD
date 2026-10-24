"""coverage.py SCRIPT FRAMES OUT -- record (bank,pc) executed from the stage-1 snapshot."""
import sys, pickle
sys.setrecursionlimit(10000)
import snapshot
from run import parse_script
m = snapshot.stage1_start()
ev = parse_script(sys.argv[1].split()) if sys.argv[1] else []
cov = {}
def tr(c, pc, op):
    k = (c.mpr[pc >> 13], pc)
    cov[k] = cov.get(k, 0) + 1
m.cpu.trace = tr
for f in range(int(sys.argv[2])):
    m.pad = sum(b for a, z, b in ev if a <= f <= z)
    m.run_frame()
m.cpu.trace = None
pickle.dump(cov, open(sys.argv[3], 'wb'))
print(len(cov), 'pcs')

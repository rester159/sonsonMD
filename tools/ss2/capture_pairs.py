"""capture_pairs.py OUT FRAMES 'SCRIPT' [ENTRY EXIT]

Run the research model from the stage-1 snapshot with a scripted pad and
record work RAM immediately before and after one routine call each frame.
Default bracket: the player update, bank 0 $EC57 (JSR $C000) .. $EC5D (after
JSR $D03D is excluded: $EC5A is the D03D call, so the bracket ends there).

Output: little header then per pair: u16 frame, 8192 bytes pre, 8192 post.
"""
import struct, sys
sys.setrecursionlimit(10000)
import snapshot
from run import parse_script

out, frames = sys.argv[1], int(sys.argv[2])
ev = parse_script(sys.argv[3].split()) if sys.argv[3] else []
entry = int(sys.argv[4], 16) if len(sys.argv) > 4 else 0xEC57
exit_ = int(sys.argv[5], 16) if len(sys.argv) > 5 else 0xEC5A
m = snapshot.stage1_start()
pairs = []
pending = None


def tr(c, pc, op):
    global pending
    if c.mpr[7] != 0:
        return
    if pc == entry:
        pending = bytes(m.ram)
    elif pc == exit_ and pending is not None:
        pairs.append((m.frame, pending, bytes(m.ram)))
        pending = None


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

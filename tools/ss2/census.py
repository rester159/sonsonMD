"""census.py K [FRAMES] -- sprite (cell, palette) usage for stage index K.

Plays from snapshot.stage_start(K) with an explorer bot (walk right, turn
back now and then, jump and attack on a cycle) and records every SATB
cell/palette pair except the streamed player cells. The converter emits
Genesis tiles for these pairs; anything not observed falls back at run
time and is counted (ss2_sprite_misses).
Output: out/ss2/s{K}_sprite_use.pkl  {(cell, pal): count}
"""
import os, pickle, sys
sys.setrecursionlimit(10000)
import snapshot

k = int(sys.argv[1])
frames = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
m = snapshot.stage_start(k)
use = {}
for f in range(frames):
    phase = f % 600
    pad = 0x08 if 420 <= phase < 520 else 0x02          # left for a while, else right
    if f % 12 < 3: pad |= 0x20                          # II: attack
    if f % 90 >= 30 and f % 90 < 40: pad |= 0x10        # I: jump
    if f % 400 >= 200 and f % 400 < 230: pad |= 0x01    # up (vines, doors)
    m.pad = pad
    m.run_frame()
    if f % 3:
        continue
    sat = m.satb
    for i in range(64):
        y, x, pat, at = sat[i * 4:i * 4 + 4]
        if not (y & 0x3FF):
            continue
        cw = 2 if at & 0x100 else 1
        ch = (1, 2, 4, 4)[(at >> 12) & 3]
        c0 = (pat >> 1) & 0x3FF
        if cw == 2: c0 &= ~1
        if ch == 2: c0 &= ~2
        if ch == 4: c0 &= ~6
        for cy in range(ch):
            for cx in range(cw):
                c = c0 + cy * 2 + cx
                if 0x100 <= c < 0x104:
                    continue                            # streamed player frame
                use[(c, at & 15)] = use.get((c, at & 15), 0) + 1
pickle.dump(use, open(os.path.join(snapshot.OUT, 's%d_sprite_use.pkl' % k), 'wb'))
print('stage', k + 1, 'pairs', len(use), 'cells', len({c for c, p in use}))

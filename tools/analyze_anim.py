#!/usr/bin/env python3
"""analyze_anim.py -- recover SonSon actor animation frame-sequences + cadence
from the live-spriteram capture (D:/portForge/tools/mame/ss_anim_cap_raw.txt).

The arcade rebuilds spriteram every frame from entity records, so a single
hardware SLOT does NOT track one logical actor (the link order changes). Instead
we cluster active-sprite observations into ACTOR TRACKS by spatial+temporal
continuity (bank constant, position continuous frame-to-frame), then for each
track emit the sprite-code sequence and per-code hold durations (the cadence).

This is the empirical cross-check of the ROM anim scripts (the authoritative
frame order + timing). Output: per-bank code timelines + detected run cycles.
"""
import sys
from collections import defaultdict, Counter

RAW = r"D:/portForge/tools/mame/ss_anim_cap_raw.txt"

def load():
    obs = []  # (fr, slot, code, bank, x, y, fx, fy)
    for line in open(RAW):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        p = line.split()
        if len(p) < 8:
            continue
        obs.append(tuple(int(v) for v in p[:8]))
    return obs

def main():
    obs = load()
    print("total active-sprite observations:", len(obs))
    frames = sorted(set(o[0] for o in obs))
    print("frame span:", frames[0], "..", frames[-1], "(", len(frames), "frames )")

    # group by bank -> what codes appear, and movement character
    by_bank = defaultdict(list)
    for o in obs:
        by_bank[o[3]].append(o)

    print("\n=== per-bank summary (bank: nobs, distinct-codes, code-range, "
          "flipx mix, y-range, x-motion) ===")
    for bank in sorted(by_bank, key=lambda b: -len(by_bank[b])):
        rows = by_bank[bank]
        codes = sorted(set(r[2] for r in rows))
        fx = Counter(r[6] for r in rows)
        ys = sorted(set(r[5] for r in rows))
        # crude x-motion: stddev of x within same-frame-adjacent? just range
        xs = [r[4] for r in rows]
        if len(codes) > 18:
            crange = "%d..%d (%d)" % (codes[0], codes[-1], len(codes))
        else:
            crange = str(codes)
        print("  bank %2d: n=%4d codes=%s fx=%s yrange=%d..%d xr=%d..%d"
              % (bank, len(rows), crange, dict(fx), ys[0], ys[-1],
                 min(xs), max(xs)))

    # ---- ACTOR TRACKING: link observations across frames by continuity -------
    # For each frame, we have a set of active sprites. Track = greedy nearest
    # (same bank, |dx|<=6, |dy|<=10) match to the previous frame's tracks.
    # Build per-frame lists.
    per_frame = defaultdict(list)
    for o in obs:
        per_frame[o[0]].append(o)

    tracks = []           # each: list of obs in order
    live = []             # list of (track_idx, last_obs)
    for fr in frames:
        cur = per_frame[fr]
        used = [False]*len(cur)
        new_live = []
        # try to extend existing live tracks
        for ti, last in live:
            lo = last
            best = -1; bestd = 999
            for i, o in enumerate(cur):
                if used[i]: continue
                if o[3] != lo[3]: continue          # same bank
                dx = abs(o[4]-lo[4]); dy = abs(o[5]-lo[5])
                # allow wrap at screen edges (x near 0/255)
                if dx > 200: dx = 256-dx
                if dx <= 7 and dy <= 12:
                    d = dx+dy
                    if d < bestd: bestd=d; best=i
            if best >= 0:
                used[best]=True
                tracks[ti].append(cur[best])
                new_live.append((ti, cur[best]))
        # start new tracks for unmatched
        for i,o in enumerate(cur):
            if used[i]: continue
            tracks.append([o])
            new_live.append((len(tracks)-1, o))
        live = new_live

    # keep substantial tracks
    longtracks = [t for t in tracks if len(t) >= 30]
    longtracks.sort(key=lambda t: -len(t))
    print("\n=== tracks (>=30 frames): idx len bank fxmix yrange xspan code-seq-summary ===")
    for ti, t in enumerate(longtracks[:25]):
        bank = t[0][3]
        fx = Counter(o[6] for o in t)
        ys = [o[5] for o in t]
        xs = [o[4] for o in t]
        # compress the code stream to (code,runlen) pairs
        seq = []
        for o in t:
            c = o[2]
            if seq and seq[-1][0]==c:
                seq[-1][1]+=1
            else:
                seq.append([c,1])
        # net x travel direction
        xnet = xs[-1]-xs[0]
        codeset = sorted(set(o[2] for o in t))
        print("  T%2d len=%4d bank=%2d fx=%s y=%d..%d xnet=%+d codes=%s"
              % (ti, len(t), bank, dict(fx), min(ys), max(ys), xnet,
                 codeset if len(codeset)<=16 else "%d..%d(%d)"%(codeset[0],codeset[-1],len(codeset))))
        # print the compressed code/duration stream (first 40 entries)
        ss = " ".join("%d:%d"%(c,n) for c,n in seq[:40])
        print("       seq:", ss, ("..." if len(seq)>40 else ""))

if __name__ == "__main__":
    main()

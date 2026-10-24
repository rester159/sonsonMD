"""Palette-line fitting: group 8-colour PCE palettes into Genesis lines.

Colours are PCE 9-bit GRB words (same 3-bit depth per channel as Genesis
RGB333). Within a group whose colour union exceeds `cap`, the closest pair
(weighted by pixel use) is merged agglomeratively.
"""
import itertools

def comp(c):
    return ((c >> 3) & 7, (c >> 6) & 7, c & 7)       # r, g, b

def dist(a, b):
    ra, ga, ba = comp(a); rb, gb, bb = comp(b)
    return 3 * (ra - rb) ** 2 + 4 * (ga - gb) ** 2 + 2 * (ba - bb) ** 2

def merge_group(weights, cap):
    """weights: {colour: pixel count}. Returns (mapping colour->kept, error)."""
    cols = dict(weights)
    mapping = {c: c for c in cols}
    err = 0
    while len(cols) > cap:
        best = None
        for a, b in itertools.combinations(cols, 2):
            # merge the lighter into the heavier
            lo, hi = (a, b) if cols[a] <= cols[b] else (b, a)
            e = dist(a, b) * cols[lo]
            if best is None or e < best[0]:
                best = (e, lo, hi)
        e, lo, hi = best
        err += e
        cols[hi] += cols.pop(lo)
        for k, v in mapping.items():
            if v == lo: mapping[k] = hi
    return mapping, err

def best_partition(pal_weights, nlines, cap=15):
    """pal_weights: {pce_pal: {colour: count}}. Exhaustive for small sets."""
    P = sorted(pal_weights)
    best = None
    for assign in itertools.product(range(nlines), repeat=len(P)):
        if assign[0] != 0: continue
        groups = [{} for _ in range(nlines)]
        for p, a in zip(P, assign):
            for c, w in pal_weights[p].items():
                groups[a][c] = groups[a].get(c, 0) + w
        if best is not None and any(len(g) > cap for g in groups) and best[0] == 0:
            continue
        tot = 0; maps = []
        for g in groups:
            mp, e = merge_group(g, cap); tot += e; maps.append(mp)
            if best is not None and tot >= best[0]: break
        else:
            if best is None or tot < best[0]:
                best = (tot, dict(zip(P, assign)), maps)
    return best

#!/usr/bin/env python3
"""gen_selector_art.py -- bake the two mode-selector logos (cut out of the cover
images in tools/selector_art/) into native Genesis tiles + tilemaps + palettes
for src/selector.c. Emits src/selector_art.c + inc/selector_art.h with
selector_art_* symbols.

Each logo is keyed off its background (mask -> index 0, so it floats on the
black backdrop), cropped to the mask's bounds and scaled to fit the SAME
144x80 box (18x10 tiles), centred. Then reduced to Genesis colour: every
channel snapped to the 3-bit VDP DAC ladder, 15 median-cut colours (from logo
pixels only) with the brightest forced to white at index 15 (so the selector's
white text can share the logo's palette line). No dithering. Identical tiles
are shared per logo.
  ARCADE = SONSON logo from the arcade flyer (with its katakana)
  HOME   = SON SON II logo from the PC Engine box"""
import os
import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage as nd

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
ART  = os.path.join(HERE, "selector_art")

TW, TH = 18, 10                                    # logo box in tiles
W, H = TW * 8, TH * 8                              # 144 x 80 px
LV = [0, 52, 87, 116, 144, 172, 206, 255]          # Genesis DAC levels (3 bits/channel)

def lvl(v): return min(range(8), key=lambda i: abs(LV[i] - v))
def cram(rgb): return lvl(rgb[2]) << 9 | lvl(rgb[1]) << 5 | lvl(rgb[0]) << 1

def keep_big(m, frac):
    """Drop connected blobs smaller than frac of the largest."""
    lab, n = nd.label(m)
    sz = nd.sum(m, lab, range(1, n + 1))
    return np.isin(lab, [i + 1 for i, v in enumerate(sz) if v >= frac * sz.max()])

def window(m, x0, y0, x1, y1):
    w = np.zeros_like(m); w[y0:y1, x0:x1] = m[y0:y1, x0:x1]
    return w

def key_arcade(r, g, b):
    """Flyer: the logo sits on white paper / sky blue. Flood that background
    in from the edges; everything it can't reach (navy outline, pink, magenta
    shadow, highlights) is logo. The white katakana printed on the sky is
    added back by window (CAPCOM is left out: illegible at 144 px)."""
    white = (r > 185) & (g > 185) & (b > 185)
    bg = white | ((b > r + 25) & (b > g) & (g > 95) & (b > 150))
    lab, _ = nd.label(bg)
    edge = set(np.unique(np.r_[lab[0], lab[-1], lab[:, 0], lab[:, -1]])) - {0}
    m = nd.binary_fill_holes(~np.isin(lab, list(edge)))
    m = keep_big(nd.binary_opening(m, iterations=2), 0.05)
    m |= nd.binary_dilation(window(white, 52, 74, 118, 98))     # ソンソン
    m &= ~nd.binary_dilation(window(white, 185, 80, 265, 117), iterations=2)  # no CAPCOM
    return keep_big(m, 0.05)

def key_home(r, g, b):
    """PC Engine box: every letter has a closed purple outline -> fill its
    holes. The orange "II" (below, on the cloud) is keyed the same way by its
    red outline inside a window, so the cloud + scenery stay out."""
    purple = (r > 90) & (r < 215) & (g < 120) & (b > 80) & (r > g + 40) & (b > g + 25)
    m = keep_big(nd.binary_fill_holes(nd.binary_closing(purple, iterations=2)), 0.05)
    red = window((r > 170) & (g < 150) & (b < 120) & (r > g + 60), 140, 180, 240, 265)
    return m | keep_big(nd.binary_fill_holes(nd.binary_closing(red, iterations=2)), 0.3)

COVERS = [  # (symbol, source, region holding the logo in source px, keyer)
    ("arcade", "arcade_flyer.png", (0, 18, 265, 135), key_arcade),
    ("home",   "home_box.webp",    (0, 0, 380, 270),  key_home),
]

def bake(src, box, keyer):
    im = Image.open(os.path.join(ART, src)).convert("RGB").crop(box)
    a = np.array(im)
    m = keyer(*(a[..., i].astype(int) for i in range(3)))
    ys, xs = np.nonzero(m)
    x0, y0, x1, y1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
    a[~m] = 0
    s = min(W / (x1 - x0), H / (y1 - y0))           # fit the box, keep aspect
    sw, sh = round((x1 - x0) * s), round((y1 - y0) * s)
    logo = Image.fromarray(a[y0:y1, x0:x1]).resize((sw, sh), Image.LANCZOS)
    logo = logo.filter(ImageFilter.UnsharpMask(radius=1, percent=80, threshold=2))
    mask = Image.fromarray((m[y0:y1, x0:x1] * 255).astype(np.uint8)).resize((sw, sh), Image.LANCZOS)
    im = Image.new("RGB", (W, H)); im.paste(logo, ((W - sw) // 2, (H - sh) // 2))
    mk = Image.new("L", (W, H)); mk.paste(mask, ((W - sw) // 2, (H - sh) // 2))
    im = im.point(lambda v: LV[lvl(v)])            # pre-snap to the 512-colour space
    on = np.array(mk) >= 128
    sample = Image.fromarray(np.array(im)[on][None, :, :])   # logo pixels only
    q = sample.quantize(15, method=Image.MEDIANCUT, dither=Image.NONE).getpalette()[:45]
    rgb = [tuple(LV[lvl(c)] for c in q[i:i + 3]) for i in range(0, 45, 3)]
    wi = min(range(15), key=lambda i: sum(255 - v for v in rgb[i]))
    del rgb[wi]
    rgb.append((255, 255, 255))                    # brightest -> index 15 = white (text colour)
    p = Image.new("P", (1, 1))
    p.putpalette(sum(rgb, ()) + rgb[0] * (256 - 15))   # pad with a real entry
    q = np.array(im.quantize(palette=p, dither=Image.NONE))
    px = np.where(on, np.minimum(q, 14) + 1, 0)    # off-logo -> index 0 (transparent)
    pal = [0x000] + [cram(c) for c in rgb]
    tiles, index, tmap = [], {}, []
    for ty in range(TH):
        for tx in range(TW):
            t = tuple(
                sum(int(px[ty * 8 + y, tx * 8 + x]) << (28 - 4 * x) for x in range(8))
                for y in range(8))
            if t not in index:
                index[t] = len(tiles); tiles.append(t)
            tmap.append(index[t])
    return pal, tiles, tmap

baked = [(sym, *bake(src, box, keyer)) for sym, src, box, keyer in COVERS]

h = open(os.path.join(REPO, "inc", "selector_art.h"), "w", newline="\n")
h.write("/* selector_art.h -- GENERATED by tools/gen_selector_art.py. DO NOT EDIT.\n")
h.write(" * The ARCADE / HOME logos for the mode selector: both 18x10 tiles\n")
h.write(" * (144x80 px), one palette line each (index 15 = white, 0 transparent =\n")
h.write(" * the black backdrop round each keyed-out logo). */\n")
h.write("#ifndef SELECTOR_ART_H\n#define SELECTOR_ART_H\n#include <genesis.h>\n\n")
h.write("#define SELECTOR_ART_TW  %d   /* tiles */\n#define SELECTOR_ART_TH  %d\n" % (TW, TH))
for sym, pal, tiles, tmap in baked:
    h.write("\n#define SELECTOR_ART_%s_TILE_COUNT  %d\n" % (sym.upper(), len(tiles)))
    h.write("extern const u16 selector_art_%s_pal[16];\n" % sym)
    h.write("extern const u32 selector_art_%s_tiles[SELECTOR_ART_%s_TILE_COUNT * 8];\n" % (sym, sym.upper()))
    h.write("extern const u16 selector_art_%s_map[SELECTOR_ART_TW * SELECTOR_ART_TH];  /* local tile ids */\n" % sym)
h.write("\n#endif\n")
h.close()

c = open(os.path.join(REPO, "src", "selector_art.c"), "w", newline="\n")
c.write("/* selector_art.c -- GENERATED by tools/gen_selector_art.py. DO NOT EDIT. */\n")
c.write("#include \"selector_art.h\"\n")
for sym, pal, tiles, tmap in baked:
    c.write("\nconst u16 selector_art_%s_pal[16] = {\n    %s\n};\n" % (sym, ",".join("0x%04X" % v for v in pal)))
    c.write("\nconst u32 selector_art_%s_tiles[SELECTOR_ART_%s_TILE_COUNT * 8] = {\n" % (sym, sym.upper()))
    for t in tiles:
        c.write("    %s,\n" % ",".join("0x%08X" % r for r in t))
    c.write("};\n")
    c.write("\nconst u16 selector_art_%s_map[SELECTOR_ART_TW * SELECTOR_ART_TH] = {\n" % sym)
    for r in range(TH):
        c.write("    %s,\n" % ",".join("%d" % v for v in tmap[r * TW:(r + 1) * TW]))
    c.write("};\n")
c.close()
for sym, pal, tiles, tmap in baked:
    print("%-6s %3d unique tiles  pal %s" % (sym, len(tiles), " ".join("%03X" % v for v in pal)))

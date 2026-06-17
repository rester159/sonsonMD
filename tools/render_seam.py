#!/usr/bin/env python3
"""render_seam.py -- render each LEVEL-TO-LEVEL SEAM of the seamless ribbon to a PNG,
straight from the SHIPPED data (the generated ss_level.c maps + per-level tile bases
+ the verbatim arcade char palette). This paints the EXACT tile words the column
streamer DMAs to Plane A, so it is a faithful picture of what the player sees as the
scenery flows from one segment into the next -- proving the seam carries contiguous
valid art with NO blank/garbage gap.

For each seam i (between level i and level i+1, and the wrap seam 8->1), it paints a
strip = last SEAM_COLS columns of level i + first SEAM_COLS columns of level i+1,
rows BLANK_TOP..31 (the playfield), at 8px/cell.

USAGE: python render_seam.py            # writes tools/ribbon_snaps/seam_N.png
Reuses gen_assets (bake_char_tile, SS_PAL_CHAR) + re-bakes each level exactly like
gen_level.py so the absolute map tile indices resolve to the right pixels.
"""
import os, sys, json, re, struct, zlib
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import gen_assets as G
import gen_level as GL

SEAM_COLS = 20          # columns of each side of the seam to show
BLANK_TOP = 5           # shipped HUD blank band (rows 0..4)
ROWS = 32

def char_palette_rgb():
    out = []
    for w in G.SS_PAL_CHAR:
        r = (w & 0x00F); g = (w & 0x0F0) >> 4; b = (w & 0xF00) >> 8
        out.append((r*16, g*16, b*16))
    return out

def write_png(path, w, h, rgb):
    def chunk(typ, data):
        c = typ + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            r,g,b = rgb[y*w+x]; raw += bytes((r,g,b))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)

def bake_levels():
    """Re-bake each level exactly like gen_level --multi (same per-level bases) and
    return a list of dicts {tiles:[u32], map:[u16], cols:int, base:int}."""
    roms = G.Roms(os.path.join(REPO, "arcade"))
    caps = [os.path.join(HERE, "level%d_capture.json" % i) for i in range(1, 9)]
    levels = []
    cur_base = 562
    for cp in caps:
        cap = json.load(open(cp))
        tiles, nm, cols, rows, n_tiles, _ = GL.assemble_level(roms, cap, cur_base, BLANK_TOP)
        levels.append({"tiles": tiles, "map": nm, "cols": cols, "base": cur_base})
        cur_base += n_tiles
    return levels

def paint_cell(rgb, W, tiles_local, x0, y0, pal):
    """tiles_local = 8 u32 (one baked 8x8 4bpp tile). Paint at pixel (x0,y0)."""
    for py in range(8):
        wpix = tiles_local[py]
        for px in range(8):
            pen = (wpix >> ((7-px)*4)) & 0xF
            color = pal[pen] if pen != 0 else (0,0,0)
            rgb[(y0+py)*W + (x0+px)] = color

def strip_for(levels, li, lj, pal):
    """Paint last SEAM_COLS cols of level li + first SEAM_COLS of level lj."""
    L = levels[li]; R = levels[lj]
    nL = min(SEAM_COLS, L["cols"]); nR = min(SEAM_COLS, R["cols"])
    cols = nL + nR
    h = (ROWS - BLANK_TOP) * 8
    W = cols * 8
    rgb = [(0,0,0)] * (W*h)
    def emit(level, startCol, ncol, xoff):
        cmap = level["map"]; ccols = level["cols"]; tiles = level["tiles"]; base = level["base"]
        for ci in range(ncol):
            col = startCol + ci
            for row in range(BLANK_TOP, ROWS):
                word = cmap[row*ccols + col]
                ti = (word & 0x07FF) - base          # local tile index
                if ti < 0 or (ti*8+8) > len(tiles):
                    continue                          # blank/transparent (hole/sky)
                paint_cell(rgb, W, tiles[ti*8:ti*8+8],
                           (xoff+ci)*8, (row-BLANK_TOP)*8, pal)
    emit(L, L["cols"]-nL, nL, 0)        # tail of left level
    emit(R, 0, nR, nL)                  # head of right level
    return W, h, rgb, nL

def main():
    pal = char_palette_rgb()
    levels = bake_levels()
    outdir = os.path.join(HERE, "ribbon_snaps")
    os.makedirs(outdir, exist_ok=True)
    seams = [(i, i+1) for i in range(7)] + [(7, 0)]   # 7 inter-level + the wrap seam
    for n, (li, lj) in enumerate(seams):
        W, h, rgb, nL = strip_for(levels, li, lj, pal)
        # mark the seam column boundary with a faint 1px magenta guide for the eye
        for y in range(h):
            rgb[y*W + nL*8] = (255, 0, 255)
        path = os.path.join(outdir, "seam_%d_lvl%d_to_lvl%d.png" % (n+1, li+1, lj+1))
        write_png(path, W, h, rgb)
        print("wrote %s (%dx%d, seam @ x=%d between level %d|%d)"
              % (os.path.relpath(path, REPO), W, h, nL*8, li+1, lj+1))

if __name__ == "__main__":
    main()

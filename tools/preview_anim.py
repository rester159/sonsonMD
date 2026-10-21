#!/usr/bin/env python3
"""preview_anim.py -- render the baked anim frames to a PNG contact sheet so the
sequences can be eyeballed (is 72/73/81 really the running hero? is 304-306 the
enemy walk? is 84 a distinct action pose?). Uses the same decode + PAL1 palette
as the bake. Stdlib only (writes a minimal PNG).
"""
import os, sys, struct, zlib
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_assets as G

# 9-bit VDP word -> 8-bit RGB for preview
def vdp_rgb(w):
    r = (w >> 1) & 7; g = (w >> 5) & 7; b = (w >> 9) & 7
    return (r*36, g*36, b*36)

def write_png(path, w, h, rgb_rows):
    def chunk(typ, data):
        c = struct.pack(">I", len(data)) + typ + data
        return c + struct.pack(">I", zlib.crc32(typ+data) & 0xffffffff)
    raw = b""
    for row in rgb_rows:
        raw += b"\x00" + bytes(row)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)

# Each anim frame: render the 16x16 sprite from spr_pen via the sprite LUT + PAL1.
PLAYER_BANK = 1; ENEMY_BANK = 2
FRAMES = [
    # bank 0 player (the red hero) -- run / climb / action
    ("RUN0",  [(64,0),(65,0),(80,0),(65,0)]),
    ("CLIMB0",[(66,0),(67,0),(68,0),(69,0)]),
    ("ACT0",  [(83,0),(80,0)]),
    # bank 1 player (palette mate) for comparison
    ("RUN1",  [(72,1),(73,1),(81,1),(73,1)]),
    ("CLIMB1",[(74,1),(75,1),(76,1),(77,1)]),
    # enemy bank 2 walk + death
    ("EWALK", [(304,ENEMY_BANK),(305,ENEMY_BANK),(306,ENEMY_BANK),(305,ENEMY_BANK)]),
    ("EDEATH",[(307,ENEMY_BANK),(308,ENEMY_BANK),(309,ENEMY_BANK),(310,ENEMY_BANK)]),
]

def render_sprite(roms, code, bank):
    """16x16 RGB grid (list of 16 rows of 16 (r,g,b)), arcade orientation,
    pen0/transparent shown as magenta checker so we can see the silhouette."""
    pen_rows = roms.spr_pen[code]
    lut = roms.sprite_lut[bank*8:bank*8+8]
    pal = G.SS_PAL_SPRITE
    grid = []
    for y in range(16):
        row = []
        for x in range(16):
            pen = pen_rows[y][x]
            if pen == 0:
                # transparent -> checker
                row.append((40,0,40) if ((x>>1)+(y>>1))&1 else (20,0,20))
            else:
                gp = lut[pen] & 0xF
                if gp == 0: gp = 15
                row.append(vdp_rgb(pal[gp]))
        grid.append(row)
    return grid

def main():
    roms = G.Roms(os.path.join(G.REPO, "arcade"))
    SCALE = 6
    PAD = 4
    GAP = 8
    maxframes = max(len(f[1]) for f in FRAMES)
    cellw = 16*SCALE + PAD
    cellh = 16*SCALE + PAD
    labelh = 0
    W = PAD + maxframes*cellw
    H = PAD + len(FRAMES)*(cellh+GAP)
    # white canvas
    canvas = [[(255,255,255) for _ in range(W)] for _ in range(H)]
    for ri,(name, frames) in enumerate(FRAMES):
        oy = PAD + ri*(cellh+GAP)
        for ci,(code,bank) in enumerate(frames):
            ox = PAD + ci*cellw
            g = render_sprite(roms, code, bank)
            for y in range(16):
                for x in range(16):
                    col = g[y][x]
                    for sy in range(SCALE):
                        for sx in range(SCALE):
                            py = oy + y*SCALE + sy; px = ox + x*SCALE + sx
                            if 0<=py<H and 0<=px<W:
                                canvas[py][px] = col
    rows = [sum(([r,gg,b] for (r,gg,b) in line), []) for line in canvas]
    out = os.path.join(HERE, "anim_preview.png")
    write_png(out, W, H, rows)
    print("wrote", out, "(%dx%d)"%(W,H))
    print("rows top->bottom:", [f[0] for f in FRAMES])

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""bank_scan.py -- render ONE sprite code across MANY palette banks, so the
right color bank for an item/fruit can be found. Reuses sheet_browse decode.

USAGE: python bank_scan.py <code> [banks_csv] [scale] [out.png]
  default banks = 0..15
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sheet_browse as SB
import gen_assets as G

def main():
    code  = int(sys.argv[1]) if len(sys.argv) > 1 else 232
    banks = [int(b) for b in sys.argv[2].split(",")] if len(sys.argv) > 2 else list(range(16))
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 7
    out   = sys.argv[4] if len(sys.argv) > 4 else os.path.join(HERE, "bank_scan.png")
    roms = G.Roms(os.path.join(G.REPO, "arcade"))
    COLS = 8; PAD = 3; LBL = 7; GAP = 2
    cellw = 16*scale + PAD; cellh = 16*scale + LBL + GAP
    rows = (len(banks)+COLS-1)//COLS
    W = PAD + COLS*cellw; H = PAD + rows*cellh
    canvas = [[(255,255,255) for _ in range(W)] for _ in range(H)]
    for i, bank in enumerate(banks):
        r = i//COLS; c = i % COLS
        ox = PAD + c*cellw; oy = PAD + r*cellh
        SB.draw_label(canvas, ox, oy, "b%d"%bank, W, H)
        g, nb = SB.render_sprite(roms, code, bank)
        for y in range(16):
            for x in range(16):
                col = g[y][x]
                for sy in range(scale):
                    for sx in range(scale):
                        py = oy+LBL+y*scale+sy; px = ox+x*scale+sx
                        if 0 <= py < H and 0 <= px < W: canvas[py][px] = col
    SB.write_png(out, W, H, canvas)
    print("wrote", out, "(%dx%d) code=%d banks=%s" % (W, H, code, banks))

if __name__ == "__main__":
    main()

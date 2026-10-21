#!/usr/bin/env python3
"""quad_preview.py -- assemble 4 sprite codes into a 32x32 composite and render
several candidate quads side by side, so a coherent large 2x2 actor can be picked.

A SonSon 16x16 sprite = 4 col-major 8x8 tiles. A 2x2 composite of sprites is laid
out as the arcade lays consecutive sprite codes: codes increment column-major too,
so a common 32x32 = TL=c, BL=c+1, TR=c+2, BR=c+3 (same col-major ordering as the
4 tiles within one sprite). We render BOTH that and a row-major guess.

USAGE: python quad_preview.py <bank> <c0,c1,...> [scale]
  Each c0 is the base of a 4-code quad. We try col-major [c,c+1,c+2,c+3].
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sheet_browse as SB
import gen_assets as G

def main():
    bank  = int(sys.argv[1]) if len(sys.argv) > 1 else 2
    bases = [int(c) for c in sys.argv[2].split(",")] if len(sys.argv) > 2 else [356, 360]
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 6
    out   = os.path.join(HERE, "quad_preview.png")
    roms = G.Roms(os.path.join(G.REPO, "arcade"))
    # each candidate renders a 32x32 (2 sprites wide, 2 tall) in col-major code order
    PAD = 6; LBL = 7
    cell = 32*scale + PAD
    COLS = 4
    rows = (len(bases)+COLS-1)//COLS
    W = PAD + COLS*(cell)
    H = PAD + rows*(cell+LBL)
    canvas = [[(255,255,255) for _ in range(W)] for _ in range(H)]
    # quad layout col-major: TL=base, BL=base+1, TR=base+2, BR=base+3
    quad_off = [(0,0),(0,1),(1,0),(1,1)]  # (sxcell, sycell) for base+0..+3
    for qi, base in enumerate(bases):
        r = qi//COLS; c = qi % COLS
        ox = PAD + c*cell; oy = PAD + r*(cell+LBL)
        SB.draw_label(canvas, ox, oy, str(base), W, H)
        for k,(cx,cy) in enumerate(quad_off):
            code = base + k
            if code >= len(roms.spr_pen): continue
            g,_ = SB.render_sprite(roms, code, bank)
            for y in range(16):
                for x in range(16):
                    col = g[y][x]
                    for sy in range(scale):
                        for sx in range(scale):
                            py = oy+LBL+(cy*16+y)*scale+sy
                            px = ox+(cx*16+x)*scale+sx
                            if 0<=py<H and 0<=px<W: canvas[py][px]=col
    SB.write_png(out, W, H, canvas)
    print("wrote", out, "(%dx%d) bank=%d bases=%s (col-major quad c..c+3)"%(W,H,bank,bases))

if __name__ == "__main__":
    main()

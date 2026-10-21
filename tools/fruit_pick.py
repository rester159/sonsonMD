#!/usr/bin/env python3
"""fruit_pick.py -- big-scale labelled render of a hand-picked LIST of sprite
codes (one bank), so candidate FRUIT/ITEM sprites can be eyeballed clearly.
Reuses sheet_browse's decode + PNG writer. Stdlib only.

USAGE: python fruit_pick.py <bank> <code,code,...> [scale] [out.png]
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sheet_browse as SB
import gen_assets as G

def main():
    bank  = int(sys.argv[1]) if len(sys.argv) > 1 else 2
    codes = [int(c) for c in sys.argv[2].split(",")] if len(sys.argv) > 2 else list(range(264, 296))
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 8
    out   = sys.argv[4] if len(sys.argv) > 4 else os.path.join(HERE, "fruit_pick.png")
    roms = G.Roms(os.path.join(G.REPO, "arcade"))
    COLS = 8; PAD = 3; LBL = 7; GAP = 2
    cellw = 16*scale + PAD; cellh = 16*scale + LBL + GAP
    rows = (len(codes)+COLS-1)//COLS
    W = PAD + COLS*cellw; H = PAD + rows*cellh
    canvas = [[(255,255,255) for _ in range(W)] for _ in range(H)]
    for i, code in enumerate(codes):
        if code >= len(roms.spr_pen): continue
        r = i//COLS; c = i % COLS
        ox = PAD + c*cellw; oy = PAD + r*cellh
        SB.draw_label(canvas, ox, oy, str(code), W, H)
        g, nb = SB.render_sprite(roms, code, bank)
        for y in range(16):
            for x in range(16):
                col = g[y][x]
                for sy in range(scale):
                    for sx in range(scale):
                        py = oy+LBL+y*scale+sy; px = ox+x*scale+sx
                        if 0 <= py < H and 0 <= px < W: canvas[py][px] = col
    SB.write_png(out, W, H, canvas)
    print("wrote", out, "(%dx%d) bank=%d codes=%s" % (W, H, bank, codes))

if __name__ == "__main__":
    main()

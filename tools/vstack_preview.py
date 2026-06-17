#!/usr/bin/env python3
"""vstack_preview.py -- assemble pairs of codes as a VERTICAL 16x32 stack
(top code over bottom code), to find coherent 2-tall actors (totems/idols/
large enemies). USAGE: python vstack_preview.py <bank> <top,bot;top,bot;...> [scale]
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sheet_browse as SB
import gen_assets as G

def main():
    bank = int(sys.argv[1]) if len(sys.argv) > 1 else 2
    pairs = []
    spec = sys.argv[2] if len(sys.argv) > 2 else "289,290;290,291"
    for p in spec.split(";"):
        a, b = p.split(","); pairs.append((int(a), int(b)))
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 7
    out = os.path.join(HERE, "vstack_preview.png")
    roms = G.Roms(os.path.join(G.REPO, "arcade"))
    PAD = 6; LBL = 7
    cw = 16*scale + PAD; ch = 32*scale + LBL + PAD
    COLS = 8
    rows = (len(pairs)+COLS-1)//COLS
    W = PAD + COLS*cw; H = PAD + rows*ch
    canvas = [[(255,255,255) for _ in range(W)] for _ in range(H)]
    for i,(t,b) in enumerate(pairs):
        r=i//COLS; c=i%COLS
        ox=PAD+c*cw; oy=PAD+r*ch
        SB.draw_label(canvas, ox, oy, "%d/%d"%(t,b), W, H)
        for cy,code in enumerate([t,b]):
            g,_=SB.render_sprite(roms, code, bank)
            for y in range(16):
                for x in range(16):
                    col=g[y][x]
                    for sy in range(scale):
                        for sx in range(scale):
                            py=oy+LBL+(cy*16+y)*scale+sy; px=ox+x*scale+sx
                            if 0<=py<H and 0<=px<W: canvas[py][px]=col
    SB.write_png(out, W, H, canvas)
    print("wrote", out, "(%dx%d) bank=%d pairs=%s"%(W,H,bank,pairs))

if __name__=="__main__":
    main()

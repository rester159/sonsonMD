#!/usr/bin/env python3
"""sheet_browse.py -- one-off: render a labelled contact sheet of a RANGE of
arcade sprite codes (one bank) so distinct archetype sprites can be picked for
the native enemy roster. Reuses gen_assets decode + PAL1. Stdlib only.

USAGE: python sheet_browse.py [bank] [start] [count] [out.png]
"""
import os, sys, struct, zlib
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_assets as G

def vdp_rgb(w):
    r = (w >> 1) & 7; g = (w >> 5) & 7; b = (w >> 9) & 7
    return (r*36, g*36, b*36)

# tiny 3x5 digit font for labels
DIGITS = {
 '0':["111","101","101","101","111"],'1':["010","110","010","010","111"],
 '2':["111","001","111","100","111"],'3':["111","001","111","001","111"],
 '4':["101","101","111","001","001"],'5':["111","100","111","001","111"],
 '6':["111","100","111","101","111"],'7':["111","001","010","010","010"],
 '8':["111","101","111","101","111"],'9':["111","101","111","001","111"],
}
def draw_label(canvas, ox, oy, text, W, H):
    cx = ox
    for ch in text:
        pat = DIGITS.get(ch)
        if pat:
            for ry,rowp in enumerate(pat):
                for rx,c in enumerate(rowp):
                    if c=='1':
                        py=oy+ry; px=cx+rx
                        if 0<=py<H and 0<=px<W: canvas[py][px]=(0,0,0)
        cx += 4

def write_png(path, w, h, canvas):
    def chunk(typ, data):
        c = struct.pack(">I", len(data)) + typ + data
        return c + struct.pack(">I", zlib.crc32(typ+data) & 0xffffffff)
    raw = b""
    for line in canvas:
        raw += b"\x00" + bytes(sum(([r,g,b] for (r,g,b) in line), []))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)

def render_sprite(roms, code, bank):
    pen_rows = roms.spr_pen[code]
    lut = roms.sprite_lut[bank*8:bank*8+8]
    pal = G.SS_PAL_SPRITE
    nonblank = 0
    grid=[]
    for y in range(16):
        row=[]
        for x in range(16):
            pen = pen_rows[y][x]
            if pen==0:
                row.append((40,0,40) if ((x>>1)+(y>>1))&1 else (20,0,20))
            else:
                nonblank+=1
                gp = lut[pen]&0xF
                if gp==0: gp=15
                row.append(vdp_rgb(pal[gp]))
        grid.append(row)
    return grid, nonblank

def main():
    bank  = int(sys.argv[1]) if len(sys.argv)>1 else 2
    start = int(sys.argv[2]) if len(sys.argv)>2 else 256
    count = int(sys.argv[3]) if len(sys.argv)>3 else 96
    out   = sys.argv[4] if len(sys.argv)>4 else os.path.join(HERE,"sheet.png")
    roms = G.Roms(os.path.join(G.REPO, "arcade"))
    COLS=12; SCALE=4; PAD=2; LBL=6; GAP=2
    cellw=16*SCALE+PAD; cellh=16*SCALE+LBL+GAP
    rows=(count+COLS-1)//COLS
    W=PAD+COLS*cellw; H=PAD+rows*cellh
    canvas=[[(255,255,255) for _ in range(W)] for _ in range(H)]
    nonblanks=[]
    for i in range(count):
        code=start+i
        if code>=len(roms.spr_pen): break
        r=i//COLS; c=i%COLS
        ox=PAD+c*cellw; oy=PAD+r*cellh
        draw_label(canvas, ox, oy, str(code), W, H)
        g,nb=render_sprite(roms, code, bank)
        if nb>20: nonblanks.append(code)
        for y in range(16):
            for x in range(16):
                col=g[y][x]
                for sy in range(SCALE):
                    for sx in range(SCALE):
                        py=oy+LBL+y*SCALE+sy; px=ox+x*SCALE+sx
                        if 0<=py<H and 0<=px<W: canvas[py][px]=col
    write_png(out, W, H, canvas)
    print("wrote", out, "(%dx%d) bank=%d codes %d..%d"%(W,H,bank,start,start+count-1))
    print("non-blank codes (>20px):", nonblanks)

if __name__=="__main__":
    main()

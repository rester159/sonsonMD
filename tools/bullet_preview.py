import os, sys
HERE=os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0,HERE)
import gen_assets as G
import sheet_browse as SB
roms=G.Roms(os.path.join(G.REPO,"arcade"))
# (label-code, code, bank)
CANDS=[("402",402,24),("408",408,24),("409",409,24),("410",410,24),   # $E26F player-shot default
       ("186",186,9),("187",187,9),("190",190,9),("191",191,9),       # $E24D variant
       ("70",70,2),("71",71,2),                                       # enemy shot / my fix
       ("230",230,14),("231",231,14)]                                 # native ET_ORB current
SCALE=6; CW=16*SCALE+8; LBL=6
W=8+len(CANDS)*CW; Hh=8+LBL+16*SCALE
canvas=[[(255,255,255) for _ in range(W)] for _ in range(Hh)]
for i,(lab,code,bank) in enumerate(CANDS):
    ox=8+i*CW; oy=8
    SB.draw_label(canvas,ox,oy,lab,W,Hh)
    g,nb=SB.render_sprite(roms,code,bank)
    for y in range(16):
        for x in range(16):
            col=g[y][x]
            for sy in range(SCALE):
                for sx in range(SCALE):
                    py=oy+LBL+y*SCALE+sy; px=ox+x*SCALE+sx
                    if 0<=py<Hh and 0<=px<W: canvas[py][px]=col
    print(f"{lab}: code={code} bank={bank} nonblank={nb}")
SB.write_png(os.path.join(HERE,"bullet_preview.png"),W,Hh,canvas)
print("wrote bullet_preview.png")

import os, sys
HERE=os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0,HERE)
import gen_assets as G, sheet_browse as SB
roms=G.Roms(os.path.join(G.REPO,"arcade"))
CANDS=[("real183",183,15),("real184",184,15),("real185",185,15),
       ("nat296",296,2),("nat297",297,2)]
SCALE=7; CW=16*SCALE+10; LBL=6
W=10+len(CANDS)*CW; Hh=10+LBL+16*SCALE
canvas=[[(255,255,255) for _ in range(W)] for _ in range(Hh)]
for i,(lab,code,bank) in enumerate(CANDS):
    ox=10+i*CW; oy=10
    SB.draw_label(canvas,ox,oy,str(code),W,Hh)
    g,nb=SB.render_sprite(roms,code,bank)
    for y in range(16):
        for x in range(16):
            col=g[y][x]
            for sy in range(SCALE):
                for sx in range(SCALE):
                    py=oy+LBL+y*SCALE+sy; px=ox+x*SCALE+sx
                    if 0<=py<Hh and 0<=px<W: canvas[py][px]=col
    print(f"{lab}: nonblank={nb}")
SB.write_png(os.path.join(HERE,"cmp_flyer.png"),W,Hh,canvas)
print("wrote")

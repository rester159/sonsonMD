import os, sys
HERE=os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0,HERE)
import gen_assets as G, sheet_browse as SB
roms=G.Roms(os.path.join(G.REPO,"arcade"))
CANDS=[("C16",16,5),("C17",17,5),("C18",18,5),("nat264",264,2),("nat265",265,2),
       ("|",None,None),
       ("T148",148,10),("T149",149,10),("T150",150,10),("T151",151,10),("nat320",320,2),("nat321",321,2)]
SCALE=6; CW=16*SCALE+8; LBL=6
W=8+len(CANDS)*CW; Hh=8+LBL+16*SCALE
canvas=[[(255,255,255) for _ in range(W)] for _ in range(Hh)]
for i,(lab,code,bank) in enumerate(CANDS):
    ox=8+i*CW; oy=8
    if code is None: continue
    SB.draw_label(canvas,ox,oy,str(code),W,Hh)
    g,nb=SB.render_sprite(roms,code,bank)
    for y in range(16):
        for x in range(16):
            col=g[y][x]
            for sy in range(SCALE):
                for sx in range(SCALE):
                    py=oy+LBL+y*SCALE+sy; px=ox+x*SCALE+sx
                    if 0<=py<Hh and 0<=px<W: canvas[py][px]=col
SB.write_png(os.path.join(HERE,"cmp2.png"),W,Hh,canvas)
print("wrote cmp2.png")

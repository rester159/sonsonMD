import os, sys
HERE=os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0,HERE)
import gen_assets as G, sheet_browse as SB
roms=G.Roms(os.path.join(G.REPO,"arcade"))
# native baked enemy codes (label, code). Render at a fixed bank for SHAPE check.
CANDS=[("W304",304),("W305",305),("FLY296",296),("FLY297",297),("THR320",320),("THR321",321),
       ("CRG272",272),("CRG273",273),("CRC264",264),("CRC265",265),("CMP352",352),("CMP353",353),
       ("HOP388",388),("HOP389",389),("LRG322",322),("LRG323",323),("EMT318",318),("EMT319",319)]
SCALE=5; CW=16*SCALE+8; LBL=6
W=8+len(CANDS)*CW; Hh=8+LBL+16*SCALE
canvas=[[(255,255,255) for _ in range(W)] for _ in range(Hh)]
for i,(lab,code) in enumerate(CANDS):
    ox=8+i*CW; oy=8
    SB.draw_label(canvas,ox,oy,str(code),W,Hh)
    g,nb=SB.render_sprite(roms,code,2)
    for y in range(16):
        for x in range(16):
            col=g[y][x]
            for sy in range(SCALE):
                for sx in range(SCALE):
                    py=oy+LBL+y*SCALE+sy; px=ox+x*SCALE+sx
                    if 0<=py<Hh and 0<=px<W: canvas[py][px]=col
    print(f"{lab}: nonblank={nb}")
SB.write_png(os.path.join(HERE,"native_sprite_check.png"),W,Hh,canvas)
print("wrote native_sprite_check.png")

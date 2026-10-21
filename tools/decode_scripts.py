import os, sys
HERE=os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0,HERE)
import gen_assets as G, sheet_browse as SB
roms=G.Roms(os.path.join(G.REPO,"arcade"))
ARC=os.path.join(G.REPO,"arcade")
def rombytes(addr,n):
    if 0x4000<=addr<0x8000: fn,base="ss.01e",0x4000
    elif 0x8000<=addr<0xC000: fn,base="ss.02e",0x8000
    else: fn,base="ss.03e",0xC000
    with open(os.path.join(ARC,fn),"rb") as f: f.seek(addr-base); return f.read(n)
def decode_script(addr,maxf=4):
    b=rombytes(addr,maxf*3); frames=[]
    for i in range(maxf):
        cl,attr,ctrl=b[i*3],b[i*3+1],b[i*3+2]
        code=cl+((attr&0x20)<<3); bank=attr&0x1F
        frames.append((code,bank))
        if ctrl&0x80: break
    return frames
# (label, script_addr)  -- direct body-script assignments found in the disasm
SCRIPTS=[("classC",0xBD01),("enemyMain",0xBB7F),("enemyA_walk",0xDDE1),("enemyA_x",0xDE6B),
         ("obj16_D5CE",0xD5CE),("obj16_D848",0xD848),("obj16_D8BC",0xD8BC),("o_D50C",0xD50C)]
allframes=[]
for lab,addr in SCRIPTS:
    fr=decode_script(addr)
    print(f"{lab} (${addr:04X}): frames(code,bank)={fr}")
    allframes.append((lab,fr[0][0],fr[0][1]))   # first frame = body
# render the first-frame body sprite of each
SCALE=6; CW=16*SCALE+10; LBL=6
W=10+len(allframes)*CW; Hh=10+LBL+16*SCALE
canvas=[[(255,255,255) for _ in range(W)] for _ in range(Hh)]
for i,(lab,code,bank) in enumerate(allframes):
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
SB.write_png(os.path.join(HERE,"scripts_preview.png"),W,Hh,canvas)
print("wrote scripts_preview.png  (order: "+", ".join(l for l,_,_ in allframes)+")")

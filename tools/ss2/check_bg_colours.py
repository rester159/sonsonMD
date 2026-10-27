#!/usr/bin/env python3
"""Compare generated background pixels against HuCard tiles/palettes.

Usage: check_bg_colours.py BASELINE_DATA_C [CANDIDATE_DATA_C]
Checks every metatile (including ones not initially visible), transparency,
per-tile colour error, and the two-line BG ownership contract.
"""
import collections, json, re, sys
from pathlib import Path
import gen_ss2 as g

def arrays(path):
    text = Path(path).read_text()
    return {name: [int(v,16) for v in re.findall(r'0x[0-9A-Fa-f]+', body)]
            for name,body in re.findall(r'const u(?:16|32) (ss2_s\d+_(?:palette|tiles|meta))\[\d+\] = \{(.*?)\};',text,re.S)}

def pixels(a,s,w):
    pal=a[f'ss2_s{s}_palette']; data=a[f'ss2_s{s}_tiles']
    assert not w & 0xC000, 'BG must use lines 0/1 only'
    rows=data[(w&0x7ff)*8:(w&0x7ff)*8+8]
    assert len(rows)==8
    out=[]
    for y in range(8):
        row=[]
        for x in range(8):
            yy=7-y if w&0x1000 else y; xx=7-x if w&0x800 else x
            c=(rows[yy] >> (28-4*xx))&15
            rgb=pal[((w>>13)&3)*16+c]
            pce=((rgb>>1)&7)<<3 | ((rgb>>5)&7)<<6 | ((rgb>>9)&7)
            row.append((c!=0,pce))
        out.append(row)
    return out

old=arrays(sys.argv[1]); new=arrays(sys.argv[2] if len(sys.argv)>2 else 'src/ss2_data.c')
report=[]
for s in range(7):
    tiles=g.level.stage_tiles(g.ROM,s); pals=g.stage_bg_palettes(s)
    mt=g.level.metatiles(g.ROM,s); counts=collections.Counter(g.level.stage_map(g.ROM,s))
    before=after=changed=checks=0
    for m in range(len(new[f'ss2_s{s}_meta'])//16):
        for bi in range(4):
            bw=g.level.block_words(*mt[m*8+bi*2:m*8+bi*2+2])
            for yo in range(2):
                for xo in range(2):
                    ix=m*16+((bi>>1)*2+yo)*4+(bi&1)*2+xo
                    e=bw[yo][xo]; src=tiles[e&0xfff]; p=e>>12
                    b=pixels(old,s,old[f'ss2_s{s}_meta'][ix]); a=pixels(new,s,new[f'ss2_s{s}_meta'][ix])
                    eb=ea=0
                    for y in range(8):
                        for x in range(8):
                            c=src[y][x]; assert a[y][x][0]==b[y][x][0]==bool(c)
                            if c:
                                eb+=g.palfit.dist(pals[p][c],b[y][x][1]); ea+=g.palfit.dist(pals[p][c],a[y][x][1])
                            changed+=(a[y][x]!=b[y][x])*counts[m]; checks+=1
                    assert ea<=eb, (s,m,bi,yo,xo,eb,ea)
                    before+=eb*counts[m]; after+=ea*counts[m]
    report.append(dict(stage=s+1,pixels_checked=checks,weighted_error_before=before,weighted_error_after=after,changed_world_pixels=changed))
print(json.dumps(report,indent=2))

#!/usr/bin/env python3
from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parents[1];b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def byte(a):return b[a-0x4000]
def word(a):return byte(a)*256+byte(a+1)
roots=[0xbb65+3*i for i in range(4)]+[word(0xbb99),word(0xbb9b),0xbb7f];frames={}
for root in roots:
 a=root
 while a not in frames:
  assert 0xbb65<=a<0xbbc5
  code,attr,ctl=[byte(a+i)for i in range(3)];nxt=word(a+3)if ctl&128 else a+3
  frames[a]=(code,attr,ctl&127,nxt);a=nxt
addresses=sorted(frames);ids={a:i for i,a in enumerate(addresses)}
s='#include "arcade_main_actor.h"\nconst ArcadeMainFrame arcade_main_frames[]={\n'
for a in addresses:
 code,attr,timer,nxt=frames[a];s+='    {%d,%d,%d,%d}, /* %04X */\n'%(ids[nxt],code,attr,timer,a)
s+='};\nconst u16 arcade_main_walk[4]={'+','.join(str(ids[a])for a in roots[:4])+'};\n'
s+='const u16 arcade_main_death[2]={'+','.join(str(ids[a])for a in roots[4:6])+'};\nconst u16 arcade_main_wounded=%d;\n'%ids[roots[6]]
dx=[];dy=[]
for i in range(5):
 a=0xbbc5+i*5;x=(byte(a)<<16)|word(a+1);x=x-(1<<24)if x&(1<<23)else x;y=word(a+3);y=y-65536 if y&32768 else y;assert -32768<=x<=32767;dx.append(x);dy.append(y)
for name,values in [('dx',dx),('dy',dy)]:s+='const s16 arcade_main_%s[5]={%s};\n'%(name,','.join(map(str,values)))
(r/'src/arcade_main_actor_data.c').write_text(s)
o=r/'out/parity-audit/main-actor';o.mkdir(parents=True,exist_ok=True);(o/'manifest.json').write_text(json.dumps({'roots':roots,'addresses':addresses,'dx':dx,'dy':dy,'scope':'ordinary actor animation/motion; bonus state handled separately'},indent=2)+'\n');print(len(frames),'frames',dx,dy)

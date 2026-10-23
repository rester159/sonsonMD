#!/usr/bin/env python3
"""Extract complete D direction/death/score animation graphs from pinned ROM."""
from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parents[1];b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def byte(a):return b[a-0x4000]
def word(a):return byte(a)*256+byte(a+1)
roots=[word(0xc129+i*2)for i in range(16)]+[word(a)for a in (0xc1a2,0xc1a4,0xc1c8,0xc1ca)]
frames={}
for p in roots:
 while p not in frames:
  assert 0xc149<=p<0xc1ee
  ctl=byte(p+2);n=word(p+3)if ctl&128 else p+3
  frames[p]=(byte(p),byte(p+1),ctl&127,n);p=n
addresses=sorted(frames);ids={p:i for i,p in enumerate(addresses)}
s='#include "arcade_d_actor.h"\nconst ArcadeDFrame arcade_d_frames[]={\n'
for p in addresses:
 code,attr,timer,n=frames[p];s+='    {%d,%d,%d,%d}, /* %04X */\n'%(ids[n],code,attr,timer,p)
s+='};\n'
for name,group in [('poses',roots[:16]),('death',roots[16:18]),('score',roots[18:])]:
 s+='const u16 arcade_d_%s[%d]={%s};\n'%(name,len(group),','.join(str(ids[p])for p in group))
s+='const u8 arcade_d_wait[4]={'+','.join(str(byte(0xc007+i))for i in range(4))+'};\n'
(r/'src/arcade_d_actor_data.c').write_text(s)
o=r/'out/parity-audit/d-actor';o.mkdir(parents=True,exist_ok=True)
(o/'manifest.json').write_text(json.dumps({'frames':addresses,'roots':roots},indent=2)+'\n');print(len(frames),'D animation frames')

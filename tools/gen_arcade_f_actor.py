#!/usr/bin/env python3
"""Extract complete F flight/death/score animation graphs from pinned ROM."""
from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parents[1];b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def byte(a):return b[a-0x4000]
def word(a):return byte(a)*256+byte(a+1)
roots=[0xe032,0xe040]+[word(a)for a in (0xe04e,0xe050,0xe074,0xe076)]
frames={}
for p in roots:
 while p not in frames:
  assert 0xe032<=p<0xe09a
  ctl=byte(p+2);n=word(p+3)if ctl&128 else p+3
  frames[p]=(byte(p),byte(p+1),ctl&127,n);p=n
addresses=sorted(frames);ids={p:i for i,p in enumerate(addresses)}
s='#include "arcade_f_actor.h"\nconst ArcadeFFrame arcade_f_frames[]={\n'
for p in addresses:
 code,attr,timer,n=frames[p];s+='    {%d,%d,%d,%d}, /* %04X */\n'%(ids[n],code,attr,timer,p)
s+='};\n'
for name,group in [('flight',roots[:2]),('death',roots[2:4]),('score',roots[4:])]:
 s+='const u16 arcade_f_%s[%d]={%s};\n'%(name,len(group),','.join(str(ids[p])for p in group))
(r/'src/arcade_f_actor_data.c').write_text(s)
o=r/'out/parity-audit/f-actor';o.mkdir(parents=True,exist_ok=True)
(o/'manifest.json').write_text(json.dumps({'frames':addresses,'roots':roots},indent=2)+'\n');print(len(frames),'F animation frames')

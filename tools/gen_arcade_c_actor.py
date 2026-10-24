#!/usr/bin/env python3
from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parents[1];b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def byte(a):return b[a-0x4000]
def word(a):return byte(a)*256+byte(a+1)
assert word(0xbdaf)==0xbdb1
script=[];a=word(0xbdaf)
while True:
 d,t,p=byte(a),byte(a+1),word(a+2);script.append((a,d,t,p));a+=4
 if d&128:break
assert script[-1]==(0xbe81,255,190,0x86be)
roots=[0xbd01,word(0xbd63),word(0xbd65),word(0xbd89),word(0xbd8b)]
frames={}
for root in roots+[v[3]for v in script[:-1]]:
 a=root
 while a not in frames:
  assert 0xbd01<=a<0xbdaf
  ctl=byte(a+2);n=word(a+3)if ctl&128 else a+3
  frames[a]=(byte(a),byte(a+1),ctl&127,n);a=n
# Terminal script falls into adjacent bytes and reads this one pose before exit.
# Its continuation8880 is never consumed by the recovered cleanup path.
frames[0x86be]=(byte(0x86be),byte(0x86bf),byte(0x86c0)&127,None)
assert word(0x86c1)==0x8880
addresses=sorted(frames);ids={a:i for i,a in enumerate(addresses)}
s='#include "arcade_c_actor.h"\nconst ArcadeCFrame arcade_c_frames[]={\n'
for a in addresses:
 code,attr,timer,n=frames[a];s+='    {%d,%d,%d,%d}, /* %04X */\n'%(65535 if n is None else ids[n],code,attr,timer,a)
s+='};\nconst u16 arcade_c_initial=%d;\n'%ids[roots[0]]
for name,rr in [('death',roots[1:3]),('score',roots[3:5])]:s+='const u16 arcade_c_%s[2]={%s};\n'%(name,','.join(str(ids[x])for x in rr))
s+='const ArcadeCScript arcade_c_script[]={\n'
for a,d,t,p in script:s+='    {%d,%d,%d}, /* %04X */\n'%(ids[p],d,t,a)
s+='};\nconst ArcadeCVector arcade_c_vectors[2][17]={\n'
vectors=[]
for stopped in range(2):
 base=word(0xbe82+stopped*2);row=[]
 for i in range(17):
  a=base+i*5;x=(byte(a)<<16)|word(a+1);x=x-(1<<24)if x&(1<<23)else x;y=word(a+3);y=y-65536 if y&32768 else y;assert -32768<=x<=32767;row.append((x,y))
 vectors.append(row);s+='    {'+','.join('{%d,%d}'%v for v in row)+'},\n'
s+='};\n';(r/'src/arcade_c_actor_data.c').write_text(s)
o=r/'out/parity-audit/c-actor';o.mkdir(parents=True,exist_ok=True)
(o/'manifest.json').write_text(json.dumps({'frames':addresses,'roots':roots,'script':script,'vectors':vectors,'terminal_animation_continuation':0x8880},indent=2)+'\n')
print(len(frames),'animation frames;',len(script),'script steps;34 movement vectors')

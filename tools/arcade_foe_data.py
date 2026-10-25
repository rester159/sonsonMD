"""Versioned semantic G/H/weapon animation extraction; no runtime ROM image."""
from pathlib import Path
import hashlib
SOURCE_SHA='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def extract(directory):
 data=b''.join((Path(directory)/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e'])
 assert hashlib.sha256(data).hexdigest()==SOURCE_SHA
 def byte(a):
  assert 0x4000<=a<0x10000
  return data[a-0x4000]
 def word(a):return byte(a)*256+byte(a+1)
 roots={}
 for name,table,n in [('g_walk',0xca71,4),('g_unarmed',0xca79,4),('g_throw',0xc805,4),('g_recover',0xc87f,4),('h_walk',0xd1b5,4),('weapon_init',0xcc33,4),('weapon_fly',0xcc63,4)]:roots[name]=[word(table+i*2)for i in range(n)]
 for name,table in [('g_death',0xc9cb),('g_exit',0xca19),('h_death',0xd1f0),('h_exit',0xd270)]:roots[name]=[word(word(table+style*2)+direction*2)for style in range(2)for direction in range(5)]
 roots['weapon_end']=[0xcd0b,0xcd03]
 frames={}
 def visit(a):
  while a not in frames:
   code,attr,ctl=byte(a),byte(a+1),byte(a+2)
   nxt=word(a+3)if ctl&128 else a+3
   frames[a]={'address':a,'code':code,'attr':attr,'duration':ctl&127,'next_address':nxt}
   assert len(frames)<1024
   a=nxt
 for values in roots.values():
  for a in values:visit(a)
 addresses=sorted(frames);index={a:i for i,a in enumerate(addresses)}
 assert len(addresses)<65536
 for f in frames.values():f['next']=index[f['next_address']]
 poses=sorted({(f['code']+((f['attr']&32)<<3),f['attr']&31)for f in frames.values()} | {(code,9)for code in range(0x60,0x6e)} | {(0x1a,7),(0x1b,7),(0x1c,7),(0,7),(3,7),(0x31,4),(0x31,3)})
 # Player death, idle and entry graphs used by AD9B/AE99. Keep their
 # semantic poses separate from the enemy graph while sharing baked tiles.
 life_poses=set()
 for root in (0xaffc,0xb00a,0xafac,0xafd0,0xaf9a,0xaf9f):
  seen=set();a=root
  while a not in seen:
   seen.add(a);code,attr,ctl=byte(a),byte(a+1),byte(a+2)
   life_poses.add((code+((attr&32)<<3),attr&31))
   a=word(a+3)if ctl&128 else a+3
   assert len(seen)<16
 poses=sorted(set(poses)|life_poses)
 vectors=[]
 for base in [0xcb96,0xcbaf,0xd2f0]:
  row=[]
  for direction in range(5):
   a=base+direction*5;raw=(byte(a)<<16)|word(a+1);dx=raw-(1<<24)if raw&0x800000 else raw;raw=word(a+3);dy=raw-65536 if raw&32768 else raw
   assert -32768<=dx<=32767
   row.append((dx,dy))
  vectors.append(row)
 return {'schema':1,'life_poses':sorted(life_poses),'program_sha256':SOURCE_SHA,'roots':{k:[index[a]for a in v]for k,v in roots.items()},'frames':[frames[a]for a in addresses],'poses':poses,'vectors':vectors,'empty_target':{'x':word(0xfff5),'y':byte(0xfff8)}}

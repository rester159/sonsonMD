#!/usr/bin/env python3
"""Check source AA03 normal initialization after reentry, including clamp/drift."""
from pathlib import Path
import ctypes as C,hashlib,json,struct,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
class Player(C.Structure):_fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ('xf','y','yf','state','phase','timer','stamp','facing','previous','active','attached')]
class Animation(C.Structure):_fields_=[(n,C.c_uint8)for n in ("owner","cursor","timer","code","attr")]
class Fire(C.Structure):_fields_=[('x',C.c_uint16),('y',C.c_uint8),('facing',C.c_uint8)]
Floor=C.CFUNCTYPE(C.c_int16,C.c_uint16,C.c_uint8,C.c_void_p)
queries=0
@Floor
def floor(x,y,ctx):
 global queries
 queries+=1;return -32767
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_player.c'),'-o',str(o/'arcade_player_init.dylib')],check=True)
fn=C.CDLL(str(o/'arcade_player_init.dylib')).arcade_player_step_animated;fn.argtypes=[C.POINTER(Player),C.POINTER(Player),C.c_uint8,C.c_uint16,Floor,C.c_void_p,C.POINTER(Fire),C.POINTER(Animation)]
def decode(raw,previous):
 b=bytes.fromhex(raw)
 return Player(int.from_bytes(b[5:7],'big'),b[7],b[8],b[9],b[1],b[2],b[26],b[26],b[10],previous,b[0],b[29])
pack=lambda a:struct.pack('>H11B',a.x,*[getattr(a,n)for n,_ in Player._fields_[1:]])
sha=lambda b:hashlib.sha256(b).hexdigest();program=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'));assert sha(program)=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
trace=o/'player-init-oracle.jsonl';rows=[json.loads(x)for x in trace.read_text().splitlines()];assert 'PLAYER_INIT_DONE'in(o/'player-init-source.log').read_text();assert len(rows)==1344,len(rows)
target=o/'player-init-animation-target.csv'
with target.open('w')as f:
 f.write('player,other,input,camera,after_player,fire,animation,after_animation\n')
 for d in rows:
  p=decode(d['before'],d['previous']);q=Player(active=d['other'],state=5);before=pack(p);fire=Fire();expected=decode(d['after'],d['input'])
  raw=bytes.fromhex(d['before']);owner=raw[4];root=0xafac if owner==0 else 0xafd0
  anim=Animation(owner,(int.from_bytes(raw[14:16],'big')-root)//3,raw[13],raw[16],0xc0|owner)
  before_anim=bytes(anim).hex()
  fn(C.byref(p),C.byref(q),d['input'],d['camera'],floor,None,C.byref(fire),C.byref(anim))
  assert pack(p)==pack(expected),(d,pack(p).hex(),pack(expected).hex())
  assert(fire.x,fire.y,fire.facing)==(d['fire_x'],d['fire_y'],d['fire_facing']),d
  after=bytes.fromhex(d['after'])
  assert(root+anim.cursor*3,anim.timer,anim.code,anim.attr)==(int.from_bytes(after[14:16],'big'),after[13],after[16],after[17]),d
  assert queries==0
  pose=struct.pack('>HBB',d['fire_x'],d['fire_y'],d['fire_facing'])
  f.write(f'{before.hex()},{d["other"]},{d["input"]},{d["camera"]},{pack(expected).hex()},{pose.hex()},{before_anim},{bytes(anim).hex()}\n')
report={'scope':'Original A9A7 normal state0 including AA03/clamp/fire pose/drift; animation included; live integration separate','program_sha256':sha(program),'trace_sha256':sha(trace.read_bytes()),'target_sha256':sha(target.read_bytes()),'native_sha256':sha((r/'src/arcade_player.c').read_bytes()),'cases':len(rows),'byte_checks':len(rows)*22,'differences':0}
(o/'player-init-animation-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

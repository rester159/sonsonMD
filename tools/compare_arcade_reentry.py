#!/usr/bin/env python3
"""Original AE99 reentry, neutral expiry, held/edge input and firing pose."""
from pathlib import Path
import ctypes as C,hashlib,json,struct,subprocess,collections
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
class Life(C.Structure):
 _fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ('xf','y','yf','active','state','phase','kind','owner','facing','timer','reentry_lock','cursor','anim_timer','code','attr')]
class Fire(C.Structure):_fields_=[('x',C.c_uint16),('y',C.c_uint8),('facing',C.c_uint8)]
class Input(C.Structure):_fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ('y','facing','state','kind','available','input','previous')]
addresses=[base+3*k for base in (0xaffc,0xb00a,0xafac,0xafd0)for k in range(4)]+[0xaf9a,0xaf9f]
def decode(raw):
 b=bytes.fromhex(raw);p=int.from_bytes(b[14:16],'big');path=int.from_bytes(b[22:24],'big')
 return Life(int.from_bytes(b[5:7],'big'),b[7],b[8],b[9],b[0],b[1],b[2],b[3],b[4],b[10],b[26],b[27],addresses.index(p)if p else 255,b[13],b[16],b[17]),255 if not path else (path-0xaf62)//2
pack=lambda a:struct.pack('>H15B',a.x,*[getattr(a,k)for k,_ in Life._fields_[1:]])
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_life.c'),str(r/'src/arcade_shots.c'),'-o',str(o/'arcade_reentry.dylib')],check=True)
lib=C.CDLL(str(o/'arcade_reentry.dylib'));fn=lib.arcade_life_reentry;fn.argtypes=[C.POINTER(Life),C.POINTER(C.c_uint8),C.c_uint8,C.c_uint8,C.c_uint8,C.c_uint8,C.POINTER(Fire)];fn.restype=C.c_uint8
shoot=lib.arcade_player_shot_fire;shoot.argtypes=[C.POINTER(Input),C.c_uint8,C.POINTER(Fire)];shoot.restype=C.c_uint8
sha=lambda b:hashlib.sha256(b).hexdigest();program=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'));assert sha(program)=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
trace=o/'reentry-oracle.jsonl';rows=[json.loads(x)for x in trace.read_text().splitlines()];assert 'REENTRY_DONE'in(o/'reentry-source.log').read_text();assert sum(d['tick']==0 for d in rows)==80
phases=collections.Counter();target=o/'reentry-target.csv';shots=0
with target.open('w')as f:
 f.write('life,path,clock,stopped,input,previous,after_life,after_path,fire,fire_pose\n')
 for d in rows:
  a,path=decode(d['before']);expected,afterpath=decode(d['after']);before=pack(a);pointer=C.c_uint8(path);fire=Fire();b=bytes.fromhex(d['before']);phases[a.phase]+=1
  ready=fn(C.byref(a),C.byref(pointer),d['clock'],d['stopped'],b[24],b[25],C.byref(fire))
  assert pack(a)==pack(expected),(d,pack(a).hex(),pack(expected).hex())
  assert pointer.value==afterpath,(d,pointer.value,afterpath)
  assert (ready,fire.x,fire.y,fire.facing)==(d['fire'],d['fire_x'],d['fire_y'],d['fire_facing']),d
  actual=bytearray(64)
  if ready:
   request=Input(fire.x,fire.y,fire.facing,0,0,2,b[24],b[25]);birth=Fire();slot=shoot(C.byref(request),0,C.byref(birth))
   if slot!=255:
    assert slot==0;actual[0]=1;actual[5:7]=birth.x.to_bytes(2,'big');actual[8]=birth.y;actual[10]=birth.facing;shots+=1
  assert actual==bytes.fromhex(d['shots']),d
  pose=struct.pack('>HBB',d['fire_x'],d['fire_y'],d['fire_facing'])
  f.write(f'{before.hex()},{path},{d["clock"]},{d["stopped"]},{b[24]},{b[25]},{pack(expected).hex()},{afterpath},{ready},{pose.hex()}\n')
report={'scope':'Original AD9B/AE99 state7; direct input/fire takeover; actor integration pending','program_sha256':sha(program),'trace_sha256':sha(trace.read_bytes()),'target_sha256':sha(target.read_bytes()),'native_sha256':sha((r/'src/arcade_life.c').read_bytes()),'trajectories':80,'updates':len(rows),'phases':dict(phases),'shots':shots,'byte_checks':len(rows)*87,'differences':0}
(o/'reentry-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

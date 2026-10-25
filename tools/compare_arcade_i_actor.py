#!/usr/bin/env python3
"""Compare ordinary class-I motion/animation/death to original D317 dispatch."""
import ctypes as C,hashlib,json,struct,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
class Body(C.Structure):
 _fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ('xf','y','yf','active','state','phase','delay','facing')]
class Actor(C.Structure):
 _fields_=[('x_speed',C.c_uint32),('y_speed',C.c_uint32),('body',Body)]+[(n,C.c_uint8)for n in ('cursor','code','attr','anim_timer','hop_timer','hit_style')]
assert C.sizeof(Actor)==24
cursor_addresses=[0xd50c+k*2 for k in range(14)]+[base+k*3 for base in (0xd4cb,0xd4d9,0xd4eb,0xd4f9)for k in range(4)]
def cursor(p):return 255 if p==0 else cursor_addresses.index(p)
def actor(raw):
 b=bytes.fromhex(raw)
 return Actor(int.from_bytes(b[18:22],'big'),int.from_bytes(b[22:25],'big'),Body(int.from_bytes(b[5:7],'big'),b[7],b[8],b[9],b[0],b[1],b[2],0,b[10]),cursor(int.from_bytes(b[14:16],'big')),b[16],b[17],b[13],b[29],b[4])
def pack(a):return struct.pack('>IIH14B',a.x_speed,a.y_speed,a.body.x,*[getattr(a.body,k)for k in ('xf','y','yf','active','state','phase','delay','facing')],*[getattr(a,k)for k in ('cursor','code','attr','anim_timer','hop_timer','hit_style')])
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_i_actor.c'),'-o',str(o/'arcade_i_actor.dylib')],check=True)
fn=C.CDLL(str(o/'arcade_i_actor.dylib')).arcade_i_actor_step;fn.argtypes=[C.POINTER(Actor),C.c_uint16,C.c_uint8,C.POINTER(C.c_uint8)]
sha=lambda b:hashlib.sha256(b).hexdigest()
program=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert sha(program)=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
trace=o/'i-actor-oracle.jsonl';rows=[json.loads(x)for x in trace.read_text().splitlines()];assert len(rows)==6144,len(rows)
states={};target=o/'i-actor-target.csv'
with target.open('w')as f:
 f.write('actor,camera,stopped,live,after_actor,after_live\n')
 for d in rows:
  a=actor(d['before']);expected=actor(d['after']);before=pack(a);live=C.c_uint8(d['live_before']);states[a.body.state]=states.get(a.body.state,0)+1
  fn(C.byref(a),d['camera'],d['stopped'],C.byref(live))
  assert pack(a)==pack(expected),(d,pack(a).hex(),pack(expected).hex())
  assert live.value==d['live_after'],d
  f.write(f'{before.hex()},{d["camera"]},{d["stopped"]},{d["live_before"]},{pack(expected).hex()},{d["live_after"]}\n')
report={'scope':'isolated ordinary class-I D317 actor trajectories; shared FF bonus state and gameplay integration pending','program_sha256':sha(program),'trace_sha256':sha(trace.read_bytes()),'target_sha256':sha(target.read_bytes()),'native_sha256':sha((r/'src/arcade_i_actor.c').read_bytes()),'cases':len(rows),'byte_checks':len(rows)*25,'states':states,'differences':0}
(o/'i-actor-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

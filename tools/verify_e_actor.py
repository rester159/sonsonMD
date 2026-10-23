#!/usr/bin/env python3
"""E state transitions against a ROM-addressed offline reference."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/e-actor';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def byte(p):return b[p-0x4000]
def word(p):return byte(p)*256+byte(p+1)
assert b[0xc318-0x4000:0xc333-0x4000]==bytes.fromhex('c6 01 e7 10 cc c5 ee ed 1b bd c4 b5 0a a4 0c a9 c6 10 e7 0c c6 09 e7 0c 6c 12 39')
assert b[0xc3a6-0x4000:0xc3b4-0x4000]==bytes.fromhex('c6 10 e7 0d 6c 12 6a 0d 26 04 c6 00 e7 12')
assert b[0xc41e-0x4000:0xc429-0x4000]==bytes.fromhex('4f 5f e7 10 ed 11 0c a4 0a a9 39')
m=json.loads((o/'manifest.json').read_text());ids={p:i for i,p in enumerate(m['frames'])}
names='xf y yf active state phase delay turns direction anim_timer timer owner bonus_kind code attr target'.split()
class Actor(C.Structure):_fields_=[('x',C.c_uint16),('animation',C.c_uint16)]+[(n,C.c_uint8)for n in names]
files=['arcade_e_actor','arcade_e_actor_data','arcade_e_motion','arcade_e_motion_data','arcade_bonus_art']
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),*[str(r/'src'/f'{f}.c')for f in files],'-o',str(o/'test.dylib')],check=True)
l=C.CDLL(str(o/'test.dylib'));fn=l.arcade_e_actor_step;fn.argtypes=[C.POINTER(Actor),C.c_uint8,C.c_uint16,C.c_uint8,C.c_uint8,C.c_uint8,C.c_uint16,C.c_uint8,C.c_uint8,C.c_uint8,C.POINTER(C.c_uint8),C.POINTER(C.c_uint8),C.POINTER(C.c_uint8),C.c_uint8]
def animate(a):
 if a['anim_timer']:a['anim_timer']-=1;return
 p=a['animation'];ctl=byte(p+2);a.update(code=byte(p),attr=byte(p+1),anim_timer=ctl&127,animation=word(p+3)if ctl&128 else p+3)
def pose(a,p):a.update(animation=p,anim_timer=0);animate(a)
def direction_pose(a):
 if a['direction']:pose(a,word(0xc4c8+(a['direction']-1)*2))
def addx(a,dx):
 x=((a['x']<<8)|a['xf'])+dx;a['x']=(x>>8)&65535;a['xf']=x&255
def move(a,stop):
 p=word(0xc5ee+stop*2)+a['direction']*5;addx(a,(byte(p)<<16)|word(p+1))
 y=((a['y']<<8)|a['yf'])+word(p+3);a['y']=(y>>8)&255;a['yf']=y&255;animate(a)
def edges(a,camera,vertical=True):
 if ((a['x']-camera)&255)>=248 or (vertical and a['y']>=248):a.update(state=3,phase=2)
def turn(a,tx,ty):
 d=a['direction'];handler=word(0xc45b+2*d)
 if handler==0xc4b2:return 1
 if handler==0xc48f:inc=a['y']>=ty
 elif handler==0xc486:inc=a['x']>=tx
 elif handler==0xc497:inc=a['y']<ty
 else:assert handler==0xc47d;inc=a['x']<tx
 return (1 if d==16 else d+1)if inc else (16 if d==1 else d-1)
def reference(a,slot,camera,clock,stop,tx,ty,target,eligible,live,available):
 if not a['active']:return live,available
 state,phase=a['state'],a['phase']
 if state==0:
  if phase==0:a['active']=1;direction_pose(a);available=(available-1)&255;live=(live+1)&255;a.update(turns=9,phase=1)
  elif phase==1:
   if a['delay']==0:a.update(state=1,phase=0)
   else:
    a['delay']-=1
    if not stop:addx(a,141)
 elif state==1:
  if phase==0:
   if (clock+1-slot)&1==0:
    a['target']=target
    if not eligible:a.update(state=2,phase=0);return live,available
    a['direction']=turn(a,tx,ty);direction_pose(a);a.update(delay=8,phase=1);a['turns']=(a['turns']-1)&255
    if a['turns']==0:a.update(turns=9,phase=2)
   move(a,stop);edges(a,camera)
  elif phase==1:
   a['delay']=(a['delay']-1)&255
   if a['delay']==0:a['phase']=0
   move(a,stop);edges(a,camera)
  elif phase in (2,3):
   if phase==2:a.update(delay=16,phase=3)
   a['delay']=(a['delay']-1)&255
   if a['delay']==0:a['phase']=0
   animate(a);edges(a,camera)
   if not stop:addx(a,141)
 elif state==2:
  if phase==0:a['direction']=9;direction_pose(a);a['phase']=1
  if a['phase']==1:move(a,stop);edges(a,camera,False)
 elif state==3:
  if phase==0:pose(a,word(0xc5c8+a['owner']*2));a.update(timer=32,phase=1)
  elif phase==1:
   a['timer']=(a['timer']-1)&255
   if a['timer']:animate(a)
   else:a['phase']=2
  elif phase==2:a.update(active=0,state=0,phase=0);live=(live-1)&255;available=(available+1)&255
 return live,available
checks=0;seen=set()
def run(a,slot,mode,camera,steps):
 global checks
 ref={n:getattr(a,n)for n,_ in Actor._fields_};ref['animation']=m['frames'][a.animation]
 live=C.c_uint8(0 if a.state==0 else 1);available=C.c_uint8(0 if a.state==0 else 255);next_=C.c_uint8(0)
 rl,ra=live.value,available.value;fraction=255
 for tick in range(steps):
  stop=mode==1 or (mode==2 and tick%80>=40)
  if not stop:fraction+=141;camera=(camera+(fraction>>8))&65535;fraction&=255
  tx=(camera+128)&65535;ty=128;eligible=tick<300
  fn(C.byref(a),slot,camera,tick&255,stop,0,tx,ty,slot,eligible,C.byref(live),C.byref(available),C.byref(next_),10)
  rl,ra=reference(ref,slot,camera,tick&255,stop,tx,ty,slot,eligible,rl,ra)
  for n,_ in Actor._fields_:assert getattr(a,n)==(ids[ref[n]]if n=='animation' else ref[n]),(slot,mode,tick,n,getattr(a,n),ref[n])
  assert (live.value,available.value)==(rl,ra);checks+=1;seen.add((a.state,a.phase))
  if not a.active:break
for slot in range(2):
 for mode in range(3):
  for camera in (0,65500):
   for delay in (0,16,80):
    a=Actor();a.x=(camera+248)&65535;a.y=136;a.xf=a.yf=255;a.active=128;a.direction=9;a.delay=delay
    run(a,slot,mode,camera,900)
# Explicit phase entry fixtures ensure every path, including rare pauses, is reached.
for state,phase in [(1,0),(1,1),(1,2),(1,3),(2,0),(2,1),(3,0),(3,1),(3,2)]:
 for mode in range(3):
  for owner in range(2):
   a=Actor();a.x=128;a.y=128;a.active=1;a.state=state;a.phase=phase;a.direction=9;a.turns=1;a.delay=1;a.timer=1;a.owner=owner
   run(a,0,mode,0,400)
# Shared B291 bonus graph, with original source pointers as the reference.
bonus_roots=[word(0xb291+i*2)for i in range(11)]
bonus_ids={p:i for i,p in enumerate(sorted(set(bonus_roots+[p+3 for p in bonus_roots])))}
bonus_cases=0
def bonus_step(a,live,available,next_,limit=10,purge=0):
 fn(C.byref(a),0,0,0,0,purge,100,100,0,1,C.byref(live),C.byref(available),C.byref(next_),limit)
for kind in range(11):
 for limit in range(256):
  for owner in range(2):
   a=Actor();a.x=100;a.active=2;a.state=255;a.owner=owner;a.turns=77
   live=C.c_uint8(1);available=C.c_uint8(23);next_=C.c_uint8(kind)
   bonus_step(a,live,available,next_,limit)
   root=bonus_roots[kind];ctl=byte(root+2);nxt=word(root+3)if ctl&128 else root+3
   assert (a.active,a.phase,a.bonus_kind,a.turns)==(64,1,kind,77)
   assert next_.value==(0 if kind+1>limit else kind+1)
   assert (a.code,a.attr,a.anim_timer,a.animation)==(byte(root),byte(root+1),ctl&127,bonus_ids[nxt])
   a.active=2;a.phase=2;a.anim_timer=0
   bonus_step(a,live,available,next_,limit)
   assert a.phase==3 and a.code==byte(nxt) and a.attr==(0xc3 if owner else byte(nxt+1))
   a.x=248
   bonus_step(a,live,available,next_,limit)
   assert (a.state,a.phase,live.value,available.value)==(3,2,1,23)
   bonus_step(a,live,available,next_,limit)
   assert (a.active,live.value,available.value)==(0,0,24)
   bonus_cases+=1
# Inactive-target exit skips movement, but not the post-handler purge.
for x in (7,8,240,241):
 a=Actor();a.x=x;a.y=100;a.active=1;a.state=1;a.direction=9
 live=C.c_uint8(1);available=C.c_uint8(23);next_=C.c_uint8(0)
 fn(C.byref(a),0,0,1,0,1,100,100,0,0,C.byref(live),C.byref(available),C.byref(next_),10)
 assert (a.x,a.state,a.phase)==(x,255,0)if 8<=x<=240 else (a.x,a.state,a.phase)==(x,2,0)
a=Actor();a.x=8;a.active=2;a.state=3;a.phase=2
live=C.c_uint8(1);available=C.c_uint8(255);next_=C.c_uint8(0)
bonus_step(a,live,available,next_,purge=1)
assert (a.active,a.state,a.phase,live.value,available.value)==(0,255,0,0,0)
report={'trajectories':90,'state_comparisons':checks,'bonus_cases':bonus_cases,'states_seen':sorted(seen),'scope':'ROM-addressed offline reference for movement, pause fallthrough, inactive-target exit, full24-bit stopped vectors, counters and death. Bonus graph/rotation/owner/edge/cleanup and purge boundaries checked. Live E binding remains pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

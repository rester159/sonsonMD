#!/usr/bin/env python3
"""D native actor trajectories against ROM-addressed offline behavior reference."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/d-actor';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def byte(a):return b[a-0x4000]
def word(a):return byte(a)*256+byte(a+1)
assert b[0xbfa8-0x4000:0xbfad-0x4000]==bytes.fromhex('8d 17 8d 01 39')
assert b[0xc018-0x4000:0xc021-0x4000]==bytes.fromhex('d6 c8 5a db 24 c4 07 26 12')
assert b[0xc034-0x4000:0xc03b-0x4000]==bytes.fromhex('8d 8b 8d 03 8d 0e 39')
m=json.loads((o/'manifest.json').read_text());ids={p:i for i,p in enumerate(m['frames'])}
names='xf y yf active state phase delay kind direction anim_timer timer owner bonus_kind code attr target'.split()
class Actor(C.Structure):_fields_=[('x',C.c_uint16),('target_offset',C.c_uint16),('animation',C.c_uint16)]+[(n,C.c_uint8)for n in names]
files=['arcade_d_actor','arcade_d_actor_data','arcade_d_motion','arcade_d_motion_data','arcade_bonus_art']
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),*[str(r/'src'/f'{f}.c')for f in files],'-o',str(o/'test.dylib')],check=True)
l=C.CDLL(str(o/'test.dylib'));fn=l.arcade_d_actor_step
fn.argtypes=[C.POINTER(Actor),C.c_uint8,C.c_uint16,C.c_uint8,C.c_uint8,C.c_uint8,C.c_uint8,C.c_uint16,C.c_uint8,C.c_uint8,C.POINTER(C.c_uint8),C.POINTER(C.c_uint8),C.c_uint8]
def animate(a):
 if a['anim_timer']:a['anim_timer']-=1;return
 p=a['animation'];ctl=byte(p+2);a.update(code=byte(p),attr=byte(p+1),anim_timer=ctl&127,animation=word(p+3)if ctl&128 else p+3)
def pose(a,p):a['animation']=p;a['anim_timer']=0;animate(a)
def addx(a,d):
 x=((a['x']<<8)|a['xf'])+d;a['x']=(x>>8)&65535;a['xf']=x&255
def move(a,stop):
 p=word(0xc1ee+stop*2)+a['direction']*5;dx=(byte(p)<<16)|word(p+1)
 addx(a,dx);y=((a['y']<<8)|a['yf'])+word(p+3);a['y']=(y>>8)&255;a['yf']=y&255;animate(a)
def aim(x,y,tx,ty):
 dx=abs(tx-x)&255;dy=abs(ty-y);q,rem=divmod(dy,dx)if dx else (255,dy)
 step=4 if q>5 else 3 if q>=2 else 1 if dy//2<rem else 2
 return (17-step if ty>=y else 1+step)if tx>=x else (9+step if ty>=y else 9-step)
def reference(a,slot,camera,clock,diff,stop,tx,ty,target,live):
 if not a['active']:return live
 state,phase=a['state'],a['phase']
 if state==0:
  if phase==0:
   if a['direction']:pose(a,word(0xc129+(a['direction']-1)*2))
   live=(live+1)&255;a['phase']=1
  if a['phase']==1:
   if a['delay']==0:a.update(active=1,state=1,phase=0)
   else:
    a['delay']-=1
    if stop==0:addx(a,141)
 elif state==1:
  move(a,stop)
  if ((a['target_offset']+camera-a['x']+4)&65535)<=8:a.update(state=2,phase=0)
 elif state==2:
  if phase==0:a.update(direction=0,delay=byte(0xc007+((diff&48)>>4)),phase=1)
  elif phase==1:
   move(a,stop);a['delay']=(a['delay']-1)&255
   if a['delay']==0:a['phase']=2
  elif phase==2:
   if (clock+3-slot)&7==0:
    a['target']=target;a['direction']=aim(a['x'],a['y'],tx,ty);pose(a,word(0xc129+(a['direction']-1)*2));a.update(delay=24,phase=3)
  elif phase==3:
   move(a,stop)
   if a['y']>=248 or ((a['x']-camera)&255)>=248:a.update(state=4,phase=2)
 elif state==4:
  if phase in (0,4):pose(a,word((0xc1a2 if phase==0 else 0xc1c8)+a['owner']*2));a.update(timer=32,phase=1)
  elif phase==1:
   a['timer']=(a['timer']-1)&255
   if a['timer']:animate(a)
   else:a['phase']=2
  elif phase==2:a.update(active=255,phase=3)
  elif phase==3:a.update(active=0,state=0,phase=0);live=(live-1)&255
 return live
checks=trajectories=0;seen=set()
for slot in range(4):
 for diff in (0,16,32,48):
  for stopmode in range(3):
   for delay in (0,24,72):
    camera=65500 if delay==72 else 0;fraction=255
    a=Actor();a.x=(camera+248)&65535;a.y=136;a.xf=255;a.yf=255;a.active=128;a.direction=9;a.delay=delay;a.target_offset=144
    ref={n:getattr(a,n)for n,_ in Actor._fields_};ref['animation']=m['frames'][0]
    live=C.c_uint8(0);next_=C.c_uint8(0);rlive=0
    for tick in range(1800):
     stop=stopmode==1 or (stopmode==2 and tick%300>=150)
     if not stop:
      fraction+=141;camera=(camera+(fraction>>8))&65535;fraction&=255
     tx=(camera+(40 if tick&128 else 200))&65535;ty=40 if slot&1 else 200
     fn(C.byref(a),slot,camera,tick&255,diff,stop,0,tx,ty,slot&1,C.byref(live),C.byref(next_),10)
     rlive=reference(ref,slot,camera,tick&255,diff,stop,tx,ty,slot&1,rlive)
     for n,_ in Actor._fields_:
      expected=ids[ref[n]]if n=='animation' else ref[n]
      assert getattr(a,n)==expected,(slot,diff,stopmode,delay,tick,n,getattr(a,n),expected)
     assert live.value==rlive;seen.add((a.state,a.phase));checks+=1
     if not a.active:break
    assert not a.active,('trajectory did not clean up',slot,diff,stopmode,delay)
    trajectories+=1
# Owner-specific hit death and formation-clear score, plus inactive/state3 retention.
for owner in range(2):
 for phase in (0,4):
  a=Actor();a.active=2;a.state=4;a.phase=phase;a.owner=owner;a.x=100;a.y=100
  ref={n:getattr(a,n)for n,_ in Actor._fields_};ref['animation']=m['frames'][0];live=C.c_uint8(1);next_=C.c_uint8(0);rlive=1
  for tick in range(35):
   fn(C.byref(a),0,0,tick,0,0,0,0,0,0,C.byref(live),C.byref(next_),10);rlive=reference(ref,0,0,tick,0,0,0,0,0,rlive)
   for n,_ in Actor._fields_:assert getattr(a,n)==(ids[ref[n]]if n=='animation' else ref[n]),n
   assert live.value==rlive;checks+=1
  assert not a.active
assert {(1,0),(2,0),(2,1),(2,2),(2,3),(4,2),(4,3),(0,0)}<=seen
# Shared bonus graph decoded independently from B291, including owner palette.
bonus_roots=[word(0xb291+i*2)for i in range(11)]
bonus_ids={p:i for i,p in enumerate(sorted(set(bonus_roots+[p+3 for p in bonus_roots])))}
bonus_cases=0
for kind in range(11):
 for limit in range(256):
  for owner in range(2):
   a=Actor();a.x=100;a.active=2;a.state=255;a.owner=owner;a.kind=77
   live=C.c_uint8(1);next_=C.c_uint8(kind)
   fn(C.byref(a),0,0,0,0,0,0,0,0,0,C.byref(live),C.byref(next_),limit)
   root=bonus_roots[kind];ctl=byte(root+2);nxt=word(root+3)if ctl&128 else root+3
   assert (a.active,a.phase,a.bonus_kind,a.kind)==(64,1,kind,77)
   assert next_.value==(0 if kind+1>limit else kind+1)
   assert (a.code,a.attr,a.anim_timer,a.animation)==(byte(root),byte(root+1),ctl&127,bonus_ids[nxt])
   a.active=2;a.phase=2;a.anim_timer=0
   fn(C.byref(a),0,0,0,0,0,0,0,0,0,C.byref(live),C.byref(next_),limit)
   assert a.phase==3 and a.code==byte(nxt) and a.attr==(0xc3 if owner else byte(nxt+1))
   a.x=248
   fn(C.byref(a),0,0,0,0,0,0,0,0,0,C.byref(live),C.byref(next_),limit)
   assert a.state==4 and a.phase==2 and live.value==1
   bonus_cases+=1
for x in (7,8,240,241):
 a=Actor();a.x=x;a.active=1;a.state=3;a.phase=7
 live=C.c_uint8(1);next_=C.c_uint8(0)
 fn(C.byref(a),0,0,0,0,0,1,0,0,0,C.byref(live),C.byref(next_),10)
 assert (a.state,a.phase)==((255,0)if 8<=x<=240 else (3,7))
a=Actor();a.x=8;a.active=255;a.state=4;a.phase=3;live=C.c_uint8(1)
fn(C.byref(a),0,0,0,0,0,1,0,0,0,C.byref(live),C.byref(next_),10)
assert (a.active,a.state,a.phase,live.value)==(0,255,0,0)
report={'bonus_cases':bonus_cases,'trajectories':trajectories,'state_comparisons':checks,'states_seen':sorted(seen),'scope':'ROM-addressed offline reference, entry/delay/stop toggles/wrap/aim/cleanup and both owner death/score paths. Bonus pose/rotation/owner/edge and post-cleanup purge checked separately; live D integration pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

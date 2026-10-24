#!/usr/bin/env python3
"""F state machine versus ROM-addressed offline arithmetic/animation reference."""
from pathlib import Path
import ctypes as C,json,hashlib,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/f-actor';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def byte(a):return b[a-0x4000]
def word(a):return byte(a)*256+byte(a+1)
m=json.loads((o/'manifest.json').read_text());ids={p:i for i,p in enumerate(m['frames'])}
class Actor(C.Structure):_fields_=[(n,C.c_uint16)for n in ('x','animation','wave_phase')]+[(n,C.c_uint8)for n in 'xf y yf active state phase delay direction anim_timer origin_y half amplitude owner bonus_kind code attr'.split()]
files=['arcade_f_actor','arcade_f_actor_data','arcade_f_motion','arcade_f_motion_data','arcade_bonus_art']
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),*[str(r/'src'/f'{f}.c')for f in files],'-o',str(o/'test.dylib')],check=True)
l=C.CDLL(str(o/'test.dylib'));fn=l.arcade_f_actor_step;fn.argtypes=[C.POINTER(Actor),C.c_uint16,C.c_uint8,C.c_uint8,C.POINTER(C.c_uint8),C.POINTER(C.c_uint8),C.c_uint8]
def animate(a):
 if a['anim_timer']:a['anim_timer']-=1;return
 p=a['animation'];ctl=byte(p+2);a.update(code=byte(p),attr=byte(p+1),anim_timer=ctl&127,animation=word(p+3)if ctl&128 else p+3)
def pose(a,p):a.update(animation=p,anim_timer=0);animate(a)
def addx(a,dx):
 x=((a['x']<<8)|a['xf'])+dx;a['x']=(x>>8)&65535;a['xf']=x&255
def edge(a,camera):
 if ((a['x']-camera)&255)>=248:a.update(state=1,phase=2)
def reference(a,camera,stop,purge,live):
 if not a['active']:return live
 state,phase=a['state'],a['phase']
 if state==0:
  if phase==0:pose(a,0xe032 if a['direction']==1 else 0xe040);a.update(half=0,phase=1);live=(live+1)&255
  if a['phase']==1:
   if a['delay']==0:a.update(active=1,state=2,phase=0)
   else:
    a['delay']-=1
    if not stop:addx(a,141)
 elif state==1:
  if phase in (0,4):pose(a,word((0xe04e if phase==0 else 0xe074)+a['owner']*2));a.update(origin_y=32,phase=1)
  elif phase==1:
   a['origin_y']=(a['origin_y']-1)&255
   if a['origin_y']:animate(a)
   else:a['phase']=2
  elif phase==2:a.update(active=255,phase=3)
  elif phase==3:a.update(active=0,state=0,phase=0);live=(live-1)&255
 elif state==2:
  if phase==0:a.update(delay=16,phase=1)
  if a['phase']==1:
   p=0xe09a+a['direction']*5;addx(a,(byte(p)<<16)|word(p+1))
   y=((a['y']<<8)|a['yf'])+word(p+3);a['y']=(y>>8)&255;a['yf']=y&255;animate(a)
   a['delay']=(a['delay']-1)&255
   if not a['delay']:a.update(state=3,phase=0)
 elif state==3:
  if phase==0:a.update(amplitude=48,wave_phase=0,phase=1)
  if a['phase']==1:
   p=0xe0c7+a['direction']*3;addx(a,(byte(p)<<16)|word(p+1))
   v=(a['wave_phase']+word(0xe030))&65535
   if v>=32768:v-=32768;a['half']^=1
   a['wave_phase']=v;offset=(a['amplitude']*byte(0xe0e2+(v>>8)))//256
   a['y']=(a['origin_y']+(-offset if a['half'] else offset))&255
   animate(a);edge(a,camera)
 if purge and ((a['x']-camera-8)&65535)<=232:a.update(state=255,phase=0)
 return live
checks=trajectories=0;seen=set()
def run(a,camera,mode,steps):
 global checks,trajectories
 ref={n:getattr(a,n)for n,_ in Actor._fields_};ref['animation']=m['frames'][a.animation]
 live=C.c_uint8(0 if a.state==0 else 1);rl=live.value;next_=C.c_uint8(0);fraction=0
 for tick in range(steps):
  stop=mode==1 or (mode==2 and tick%80>=40)
  if not stop:fraction+=141;camera=(camera+(fraction>>8))&65535;fraction&=255
  fn(C.byref(a),camera,stop,0,C.byref(live),C.byref(next_),10);rl=reference(ref,camera,stop,0,rl)
  for n,_ in Actor._fields_:assert getattr(a,n)==(ids[ref[n]]if n=='animation'else ref[n]),(tick,n,getattr(a,n),ref[n])
  assert live.value==rl;checks+=1;seen.add((a.state,a.phase))
  if not a.active:break
 trajectories+=1
for direction in (1,5):
 for camera in (0,65500):
  for mode in range(3):
   for delay in (0,24,88):
    a=Actor();a.x=(camera+(-8 if direction==1 else 248))&65535;a.xf=a.yf=255;a.y=a.origin_y=168;a.active=128;a.delay=delay;a.direction=direction
    run(a,camera,mode,1200)
for owner in range(2):
 for phase in (0,4):
  a=Actor();a.x=a.y=100;a.active=2;a.state=1;a.phase=phase;a.owner=owner;run(a,0,1,36)
bonus_roots=[word(0xb291+i*2)for i in range(11)]
bonus_ids={p:i for i,p in enumerate(sorted(set(bonus_roots+[p+3 for p in bonus_roots])))}
bonus_cases=0
for kind in range(11):
 for limit in range(256):
  for owner in range(2):
   a=Actor();a.x=100;a.active=2;a.state=255;a.owner=owner;a.origin_y=77
   live=C.c_uint8(1);next_=C.c_uint8(kind)
   fn(C.byref(a),0,0,0,C.byref(live),C.byref(next_),limit)
   root=bonus_roots[kind];ctl=byte(root+2);nxt=word(root+3)if ctl&128 else root+3
   assert (a.active,a.phase,a.bonus_kind,a.origin_y)==(64,1,kind,77)
   assert next_.value==(0 if kind+1>limit else kind+1)
   assert (a.code,a.attr,a.anim_timer,a.animation)==(byte(root),byte(root+1),ctl&127,bonus_ids[nxt])
   a.active=2;a.phase=2;a.anim_timer=0
   fn(C.byref(a),0,0,0,C.byref(live),C.byref(next_),limit)
   assert a.phase==3 and a.code==byte(nxt) and a.attr==(0xc3 if owner else byte(nxt+1))
   a.x=248
   fn(C.byref(a),0,0,0,C.byref(live),C.byref(next_),limit)
   assert (a.state,a.phase,live.value)==(1,2,1)
   fn(C.byref(a),0,0,0,C.byref(live),C.byref(next_),limit)
   assert (a.active,a.phase,live.value)==(255,3,1)
   fn(C.byref(a),0,0,0,C.byref(live),C.byref(next_),limit)
   assert (a.active,live.value)==(0,0)
   bonus_cases+=1
for x in (7,8,240,241):
 a=Actor();a.x=x;a.active=2;a.state=1;a.phase=1;a.origin_y=2
 live=C.c_uint8(1);next_=C.c_uint8(0)
 fn(C.byref(a),0,0,1,C.byref(live),C.byref(next_),10)
 assert (a.state,a.phase)==((255,0)if 8<=x<=240 else (1,1))
a=Actor();a.x=8;a.active=255;a.state=1;a.phase=3
live=C.c_uint8(1);next_=C.c_uint8(0)
fn(C.byref(a),0,0,1,C.byref(live),C.byref(next_),10)
assert (a.active,a.state,a.phase,live.value)==(0,255,0,0)
report={'trajectories':trajectories,'state_comparisons':checks,'bonus_cases':bonus_cases,'states_seen':sorted(seen),'scope':'ROM-addressed offline movement, entry/delay, fixed16-step entry, wave phase, retained fractions, death/score/hidden cleanup. Bonus root/rotation/owner/edge/hidden cleanup and post-handler purge checked. Actor sanitizers, target and live binding pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

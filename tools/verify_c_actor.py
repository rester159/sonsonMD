#!/usr/bin/env python3
"""Typed C actor against a ROM-addressed offline reference, not source execution."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/c-actor';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def byte(a):return b[a-0x4000]
def word(a):return byte(a)*256+byte(a+1)
assert b[0xbc0e-0x4000:0xbc17-0x4000]==bytes.fromhex('c1 f8 23 05 cc 02 02 ed 11')
assert b[0xbcd9-0x4000:0xbce9-0x4000]==bytes.fromhex('c6 ff e7 10 6c 12 39 4f 5f e7 10 ed 11 0a a7 39')
manifest=json.loads((o/'manifest.json').read_text());ids={addr:i for i,addr in enumerate(manifest['frames'])}
names='xf y yf active state phase delay kind direction script_timer anim_timer timer owner bonus_kind code attr'.split()
class Actor(C.Structure):_fields_=[('x',C.c_uint16),('animation',C.c_uint16),('script',C.c_uint16)]+[(n,C.c_uint8)for n in names]
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),*[str(r/'src'/n)for n in ['arcade_c_actor.c','arcade_c_actor_data.c','arcade_bonus_art.c']],'-o',str(o/'test.dylib')],check=True)
l=C.CDLL(str(o/'test.dylib'));fn=l.arcade_c_actor_step;fn.argtypes=[C.POINTER(Actor),C.c_uint16,C.c_uint8,C.c_uint8,C.POINTER(C.c_uint8),C.POINTER(C.c_uint8),C.c_uint8]
def animate(a):
 if a['anim_timer']:a['anim_timer']-=1;return
 p=a['animation'];a['code']=byte(p);a['attr']=byte(p+1);ctl=byte(p+2);a['anim_timer']=ctl&127;a['animation']=word(p+3)if ctl&128 else p+3
def pose(a,p):a['animation']=p;a['anim_timer']=0;animate(a)
def sequence(a):
 if a['script_timer']:a['script_timer']-=1;return
 p=a['script'];a['direction']=byte(p);a['script_timer']=byte(p+1);a['script']=p+4;pose(a,word(p+2))
def addx(a,d):
 v=((a['x']<<8)|a['xf'])+d;a['x']=(v>>8)&65535;a['xf']=v&255
def reference(a,camera,stopped,purge,live):
 if not a['active']:return live
 if a['state']==0:
  if not a['phase']:pose(a,0xbd01);a['script']=word(0xbdaf);a['script_timer']=0;sequence(a);live=(live+1)&255;a['phase']=1
  if a['phase']==1:
   if not a['delay']:a['active']=1;a['state']=1;a['phase']=0
   else:
    a['delay']-=1
    if not stopped:addx(a,141)
 elif a['state']==1:
  p=word(0xbe82+stopped*2)+a['direction']*5;dx=(byte(p)<<16)|word(p+1);dy=word(p+3)
  addx(a,dx);y=((a['y']<<8)|a['yf'])+dy;a['y']=(y>>8)&255;a['yf']=y&255
  animate(a);sequence(a)
  if ((a['x']-camera)&255)>248 or a['direction']&128:a['state']=2;a['phase']=2
 elif a['state']==2:
  phase=a['phase']
  if phase in (0,4):pose(a,word((0xbd63 if phase==0 else 0xbd89)+a['owner']*2));a['timer']=32;a['phase']=1
  elif phase==1:
   a['timer']=(a['timer']-1)&255
   if a['timer']:animate(a)
   else:a['phase']=2
  elif phase==2:a['active']=255;a['phase']=3
  elif phase==3:a['active']=a['state']=a['phase']=0;live=(live-1)&255
 if purge and ((a['x']-camera-8)&65535)<=232:a['state']=255;a['phase']=0
 return live
def compare(a,d):
 for n in ['x']+names:assert getattr(a,n)==d[n],(n,getattr(a,n),d[n])
 assert a.animation==ids.get(d['animation'],65535),(a.animation,d['animation'])
 assert a.script==(d['script']-0xbdb1)//4,(a.script,d['script'])
checks=0;trajectories=0
for camera0 in (0,65500):
 for mode in range(3):
  for delay in (0,1,24,120):
   for frac in (0,255):
    a=Actor();a.x=(camera0+248)&65535;a.xf=frac;a.y=192;a.yf=frac;a.active=128;a.delay=delay
    d={n:getattr(a,n)for n in ['x']+names};d.update(animation=0xbd01,script=0xbdb1)
    live=C.c_uint8(0);ref_live=0;next_kind=C.c_uint8(0);camera_fraction=camera0*256
    for tick in range(1200):
     stopped=int(mode==1 or (mode==2 and tick%34>=17));camera=(camera_fraction>>8)&65535
     fn(C.byref(a),camera,stopped,0,C.byref(live),C.byref(next_kind),10);ref_live=reference(d,camera,stopped,0,ref_live);compare(a,d);assert live.value==ref_live;checks+=1
     if not a.active:break
     if not stopped:camera_fraction+=141
    assert not a.active;trajectories+=1
# Full death and score-pose lifetimes for both owners, with retained fractions.
for owner in range(2):
 for phase in (0,4):
  a=Actor();a.active=2;a.state=2;a.phase=phase;a.owner=owner;a.x=100;a.xf=255;a.y=100;a.yf=127;a.animation=ids[0xbd01]
  d={n:getattr(a,n)for n in ['x']+names};d.update(animation=0xbd01,script=0xbdb1)
  live=C.c_uint8(1);ref_live=1;next_kind=C.c_uint8(0)
  for tick in range(36):
   fn(C.byref(a),0,0,0,C.byref(live),C.byref(next_kind),10);ref_live=reference(d,0,0,0,ref_live);compare(a,d);assert live.value==ref_live;checks+=1
# Force terminal fetch: source reads FF/BE/86BE then cleanup, never vector255.
a=Actor();a.active=1;a.state=1;a.x=100;a.y=100;a.direction=9;a.script=52;a.animation=ids[0xbd01]
live=C.c_uint8(1);next_kind=C.c_uint8(0)
fn(C.byref(a),0,1,0,C.byref(live),C.byref(next_kind),10)
assert (a.state,a.phase,a.direction,a.code,a.attr,a.anim_timer,a.animation)==(2,2,255,128,8,8,65535)
fn(C.byref(a),0,1,0,C.byref(live),C.byref(next_kind),10);assert a.active==255 and a.phase==3
fn(C.byref(a),0,1,0,C.byref(live),C.byref(next_kind),10);assert a.active==0 and live.value==0
# Bonus edge uses strict >248; purge follows cleanup even when inactive.
for x in (248,249,65535):
 a=Actor();a.active=64;a.state=255;a.phase=1;a.x=x
 fn(C.byref(a),0,0,0,C.byref(live),C.byref(next_kind),10);assert a.state==(255 if x==248 else 2)
a=Actor();a.active=255;a.state=2;a.phase=3;a.x=8;live.value=0
fn(C.byref(a),0,0,1,C.byref(live),C.byref(next_kind),10);assert not a.active and a.state==255 and a.phase==0 and live.value==255
report={'trajectories':trajectories,'state_comparisons':checks,'terminal_and_boundary_cases':7,'scope':'Offline ROM-addressed reference for ordinary C entry/movement/script/death; generated frames/vectors read independently from raw ROM. Terminal fallthrough and cleanup ordering included. Bonus animation/collect, source execution, art and live binding remain unproven.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

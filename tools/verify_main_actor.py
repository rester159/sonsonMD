#!/usr/bin/env python3
"""Source-contract checks for ordinary enemyMain entry and movement boundaries."""
from pathlib import Path
import ctypes as C,json,subprocess,itertools,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/main-actor';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
meta=json.loads((o/'manifest.json').read_text());ids={a:i for i,a in enumerate(meta['addresses'])}
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_main_actor.c'),str(r/'src/arcade_main_actor_data.c'),'-o',str(o/'test.dylib')],check=True)
class Actor(C.Structure):_fields_=[(n,C.c_uint16)for n in ('x','origin','cursor')]+[(n,C.c_uint8)for n in ('xf','y','yf','active','state','phase','direction','delay','variant','hp','hit','owner','timer','anim_timer','code','attr')]
lib=C.CDLL(str(o/'test.dylib'));lib.arcade_main_actor_step.argtypes=[C.POINTER(Actor),C.c_uint8,C.c_uint16,C.c_uint8,C.c_uint8,C.c_uint8,C.POINTER(C.c_uint8)]
def step(a,slot=0,camera=0,clock=0,stopped=0,purge=0,live=9):
 n=C.c_uint8(live);lib.arcade_main_actor_step(C.byref(a),slot,camera,clock,stopped,purge,C.byref(n));return n.value
checks=0
for slot,delay,clock,stopped in itertools.product(range(16),range(256),(0,1,31,32),(0,1)):
 a=Actor();a.x=123;a.origin=4321;a.xf=251;a.y=104;a.yf=77;a.active=128;a.delay=delay;a.variant=2
 n=step(a,slot,65500,clock,stopped,live=255)
 x=(((65500+248)&65535)<<8)|251
 if delay and not stopped:x=(x+141)&0xffffff
 assert (a.x,a.xf,a.y,a.yf,a.active,a.state,a.phase,a.delay,a.hp,n)==(x>>8,x&255,104,77,130,1,0 if delay else 1,(delay-1)&255 if delay and clock%32==0 else delay,5,0)
 addr=0xbb65+3*((16-slot)&3);code,attr,ctl=rom[addr-0x4000:addr-0x4000+3]
 assert (a.code,a.attr,a.anim_timer,a.cursor)==(code,attr,ctl&127,ids[addr+3])
 assert a.origin==4321 and a.variant==2;checks+=1
# Bouncing movement: phase0 sets the origin without moving; thresholds inclusive.
for distance in (0,24,25,48,49,65535):
 for phase in (0,1,2):
  a=Actor();a.active=1;a.state=4;a.phase=phase;a.x=100;a.origin=(100+distance if phase==1 else 100-distance)&65535;a.xf=0;a.direction=3 if phase!=2 else 1;a.cursor=ids[0xbb65]
  old=a.x;step(a,camera=0)
  if phase==0:assert a.x==old and a.origin==old and a.phase==1
  else:
   raw=(old<<8)+(-96 if phase==1 else 96);expected=(raw>>8)&65535
   assert a.x==expected
   diff=((a.origin-a.x)if phase==1 else(a.x-a.origin))&65535
   # A turn resets origin, making diff zero.
   turned=a.phase!=phase
   assert (a.origin==a.x and a.direction==(1 if phase==1 else 3))if turned else diff<=(48 if phase==1 else 24)
  checks+=1
# Death cleanup still executes hit handling then conversion after deactivation.
for hit,purge in itertools.product((0,1),(0,1)):
 a=Actor();a.active=2;a.state=5;a.phase=2;a.hit=hit;a.x=100
 n=step(a,purge=purge,live=0)
 assert (a.active,a.state,a.phase,n)==(0,255 if purge else 0,0,255)
 assert a.hit==0;checks+=1
report={'cases':checks,'scope':'Offline source-contract initialization/delay, animation selection, bounce and cleanup ordering; generated animation compared to original ROM bytes. Ordinary states implemented, bonus state separate, full trajectory/source runtime and live binding pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

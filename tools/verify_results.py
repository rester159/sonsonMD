#!/usr/bin/env python3
"""Offline source-contract checks for native97F9 results sequencing."""
from pathlib import Path
import ctypes as C
import hashlib,itertools,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/results-flow'
o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
assert rom[0x587f:0x5888].hex()=='d67e5cc1092302c610'
assert rom[0x598f:0x599c].hex()=='d624c43f26060a0d26020c0e39'
# 0400/0480 read dynamic DC..DF, while0434/04B4 resolve30000 in ROM.
assert rom[0xd84:0xd86]==bytes.fromhex('00e0')
pointer=int.from_bytes(rom[0xd84+0x34*2:0xd86+0x34*2],'big')
assert rom[pointer-0x4000-4:pointer-0x4000]==bytes.fromhex('00030000')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_results.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'))
class State(C.Structure):
 _fields_=[(n,C.c_uint8)for n in ('stage','phase','timer','course')]
class Input(C.Structure):
 _fields_=[(n,C.c_uint8)for n in ('clock','busy','p1_out','p2_out','two_players','marker')]
class Event(C.Structure):
 _fields_=[('points',C.c_uint32*2),('flags',C.c_uint16)]+[(n,C.c_uint8)for n in ('players','marker','number')]
lib.arcade_results_step.argtypes=[C.POINTER(State),C.POINTER(Input),C.POINTER(Event)]
checks=0
def step(s,i):
 global checks
 e=Event((C.c_uint32*2)(0xffffffff,0xffffffff),65535,255,255,255)
 lib.arcade_results_step(C.byref(s),C.byref(i),C.byref(e));checks+=1
 return e
for course in range(256):
 s=State(2,0,0,course);e=step(s,Input())
 n=(course+1)%256
 assert (e.flags,e.number,s.phase)==(4,n if n<=9 else 16,1)
 s.phase=5;award=step(s,Input(0,0,0,0,1,0))
 assert list(award.points)==[min(n,10)*10000]*2
 s.phase=6;e=step(s,Input())
 assert (e.flags,s.course,s.stage)==(128,(course+1)%256,3)
 assert step(s,Input()).flags==0
for p1,p2,two,marker in itertools.product((0,1,128),(0,1,128),(0,1,2,3),(0,1,2,3,255)):
 i=Input(0,0,p1,p2,two,marker)
 for phase in (1,2,5):
  s=State(2,phase,0,0);e=step(s,i)
  mask=(1 if p1==0 else 0)+(2 if two%2 and p2==0 else 0)
  if phase!=5:mask&=phase
  assert e.players==mask
  expected_marker=(1 if marker==1 else 2) if phase==5 and marker else (marker if mask and marker==mask else 0)
  assert e.marker==expected_marker
  assert e.flags==(64 if phase==5 else 8 if mask else 0)
  assert list(e.points)==[(10000 if mask&(1<<p) else 0)+(30000 if expected_marker==p+1 else 0) if phase==5 else 0 for p in range(2)]
for start in range(256):
 s=State();i=Input(start,0,0,0,1,0)
 # First source phase falls through to gameplay, ending on first64 boundary.
 for offset in range((-start)%64+1):
  i.clock=(start+offset)%256
  assert step(s,i).flags==1
 assert (s.stage,s.phase)==(0,2)
 assert step(s,i).flags==0 and (s.stage,s.phase)==(1,0)
 # Loading waits on both sides of the native screen-preparation event.
 for phase in (0,1):
  i.busy=1
  for _ in range(3):
   assert step(s,i).flags==0 and (s.stage,s.phase)==(1,phase)
  i.busy=0;e=step(s,i)
  assert e.flags==(2 if phase==0 else 0)
 assert (s.stage,s.phase)==(2,0)
 assert step(s,i).flags==4
 assert step(s,i).players==1
 assert step(s,i).players==2
 assert step(s,i).flags==16 and s.timer==5
 # Delay ends on the fifth64 boundary, while still animating that update.
 for offset in range((-start)%64+4*64+1):
  i.clock=(start+offset)%256
  assert step(s,i).flags==32
 assert (s.phase,s.timer)==(5,0)
 assert step(s,i).flags==64
 assert step(s,i).flags==128 and s.course==1
report={'updates':checks,'starting_clocks':256,'player_selection_combinations':180,
 'course_values':256,'scope':'Offline inspected-source contract: gameplay settle, display-ready gating, player/marker eligibility, five64-clock presentation intervals, byte course wrap and one-shot restart. Display assets, score effects and live target integration remain pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

#!/usr/bin/env python3
"""Offline exhaustive A7F0 gate contract; no emulator execution."""
from pathlib import Path
import ctypes as C
import json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/finish'
o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert rom[0x67f0:0x6807].hex()=='8d15d624c43f260e0a9f260a0fa5c604d7080f0b0f0e39'
assert rom[0x6807:0x681f].hex()=='d624c50f2611cea81fc5102603cea827108e10f4bdb78a39'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_finish.c'),str(r/'src/arcade_finish_data.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'))
class Event(C.Structure):
 _fields_=[(n,C.c_uint8)for n in ('write','pattern','results')]
lib.arcade_finish_step.argtypes=[C.POINTER(C.c_uint8),C.c_uint8,C.POINTER(Event)]
for timer in range(256):
 for clock in range(256):
  s=C.c_uint8(timer);e=Event(99,99,99)
  lib.arcade_finish_step(C.byref(s),clock,C.byref(e))
  assert s.value==(timer-int(clock%64==0))%256
  assert e.write==int(clock%16==0)
  assert e.pattern==1-(clock//16)%2
  assert e.results==int(clock%64==0 and timer==1)
  if e.results:assert e.write and e.pattern==1
for start in range(256):
 s=C.c_uint8(15);e=Event();ticks=0
 while True:
  lib.arcade_finish_step(C.byref(s),(start+ticks)%256,C.byref(e));ticks+=1
  if e.results:break
  assert ticks<=960
 assert ticks==((-start)%64)+14*64+1
assert bytes((C.c_uint8*16).in_dll(lib,'arcade_finish_patterns'))==rom[0x681f:0x682f]
report={'single_update_cases':65536,'complete_countdowns':256,
 'scope':'Source-byte-guarded offline gate contract, including zero-timer wrap and flash on expiry. No independent arcade trace, target execution, live stop2 owner or results-screen integration.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

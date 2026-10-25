#!/usr/bin/env python3
"""Offline A893 contract checks; does not launch an emulator or claim live parity."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import subprocess

r=Path(__file__).resolve().parents[1]
o=r/'out/parity-audit/release-static';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/f).read_bytes() for f in ('ss.01e','ss.02e','ss.03e'))
digest=hashlib.sha256(rom).hexdigest()
assert digest=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
# Guard the independently inspected source contract: initialize08/00, clock+3
# masked03, frame increment, remaining decrement, clear only active/phase.
assert rom[0x68a6:0x68b6].hex()=='cc0800ed026c0139d624cb03c503262b'
assert rom[0x68d7:0x68e2].hex()=='6c036a0226044f5fed8439'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib',
    '-I'+str(r/'inc'),str(r/'src/arcade_release.c'),
    str(r/'src/arcade_release_data.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'))
class State(C.Structure):
    _fields_=[(k,C.c_uint8) for k in ('active','phase','remaining','frame')]
lib.arcade_release_begin.argtypes=[C.POINTER(State)]
lib.arcade_release_step.argtypes=[C.POINTER(State),C.c_uint8]
lib.arcade_release_step.restype=C.c_uint8
checks=0
for start in range(256):
    for retrigger in (None,1,7,29,33):
        s=State(0,0,99,123)
        lib.arcade_release_begin(C.byref(s))
        assert bytes(s)==bytes((1,0,99,123))
        origin=0;events=[]
        for tick in range(80):
            if tick==retrigger:
                tail=bytes(s)[2:]
                lib.arcade_release_begin(C.byref(s))
                assert bytes(s)==bytes((1,0))+tail
                origin=tick;events=[]
            clock=(start+tick)&255
            pose=lib.arcade_release_step(C.byref(s),clock)
            # Closed-form event schedule, independent of the native state machine.
            first=origin+1+((1-start-origin-1)%4)
            scheduled=list(range(first,first+32,4))
            expected=scheduled.index(tick) if tick in scheduled else 255
            assert pose==expected,(start,retrigger,tick,pose,expected)
            if pose!=255:events.append(pose)
            n=sum(t<=tick for t in scheduled)
            assert bytes(s)==bytes((int(n<8),int(n<8),8-n,n)),(start,tick,bytes(s))
            checks+=1
        assert events==list(range(8))
patterns=bytes((C.c_uint8*64).in_dll(lib,'arcade_release_patterns'))
assert patterns==rom[0x68e2:0x6922]
destinations=list((C.c_uint16*6).in_dll(lib,'arcade_release_destinations'))
assert destinations==[int.from_bytes(rom[a:a+2],'big') for a in range(0x6922,0x692e,2)]
report={'source_sha256':digest,'updates':checks,'starting_clock_phases':256,
    'retrigger_schedules':5,'patterns':8,'destinations':6,
    'scope':'Offline source-byte-guarded timing/table contract; no independent emulator trace, target execution or live background integration yet.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

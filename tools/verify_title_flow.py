#!/usr/bin/env python3
"""Source-contract checks, not a claim of original-CPU or integrated parity."""
import ctypes as C
import json
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[1]
out=root/'out/parity-audit/title-lifecycle'
subprocess.run(['python3',str(root/'tools/audit_title_lifecycle.py')],check=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib',
               '-I'+str(root/'inc'),str(root/'src/arcade_title_flow.c'),
               '-o',str(out/'flow.dylib')],check=True)
lib=C.CDLL(str(out/'flow.dylib'))
class State(C.Structure):
    _fields_=[('timer',C.c_uint16),('demo_timer',C.c_uint16),('shown',C.c_uint16),
              ('state',C.c_uint8),('phase',C.c_uint8)]
class Input(C.Structure):
    _fields_=[('credits',C.c_uint16),('clock',C.c_uint8),('busy',C.c_uint8),('free',C.c_uint8)]
class Event(C.Structure):
    _fields_=[('flags',C.c_uint16),('commands',C.c_uint16*8),('count',C.c_uint8)]
lib.arcade_title_step.argtypes=[C.POINTER(State),C.POINTER(Input),C.POINTER(Event)]
checks=0
def step(s,i):
    global checks
    e=Event();lib.arcade_title_step(C.byref(s),C.byref(i),C.byref(e))
    assert e.count<=8
    checks+=1
    return e,list(e.commands[:e.count])

# All 16-bit countdown values, including zero wrap; byte countdown preserves06.
for timer in range(65536):
    s=State(timer,0,1,5,2)
    e,q=step(s,Input(1,7,1,0))
    assert s.timer==(timer-1)&65535 and not q and e.flags==0
    assert (s.state,s.phase)==((0,0) if timer==1 else (5,2))
    for phase in (2,4):
        s=State(timer,0,0,4,phase)
        e,q=step(s,Input())
        assert s.timer==(timer&0xff00)|((timer-1)&255)
        if timer&255==1:
            assert (s.state,s.phase)==((4,3) if phase==2 else (0,0))
            assert q==([0x119] if phase==2 else [])
        else:
            assert (s.state,s.phase)==(4,phase) and not q

# All clock phases: prelude/demo counters tick only at multiples of64.
for clock in range(256):
    for state,phase,flag in ((1,4,16),(3,0,64)):
        s=State(1,1,0,state,phase)
        e,q=step(s,Input(0,clock,0,0))
        assert e.flags&flag and not q
        assert s.state==(state+1 if clock%64==0 else state)

# Busy waits must not hide a coin interruption. No duplicate refresh commands
# on unchanged credit. Simultaneous screen commands survive the interruption.
commands={
    (1,1):[0x1000,0x20a,0x338,0x700,0x1000,0x20b],
    (4,1):[0x1000,0x900,0x20a,0x338,0x700],
    (4,3):[0x1000,0xa00,0x20a,0x338,0x700],
    (5,1):[0x20c,0x20a,0x1000,0x700,0x324,0xe00],
}
for (state,phase),expected in commands.items():
    for busy in (0,1):
        for credit in (0,1,255,256,65535):
            s=State(0,0,0,state,phase)
            e,q=step(s,Input(credit,0,busy,0))
            assert q==([] if busy else expected)+([0x338,0x700] if credit else [])
            if credit:assert (s.state,s.phase,s.shown)==(5,0,credit)
            elif busy:assert (s.state,s.phase)==(state,phase)

# Credited state expires after240 calls despite credits; free play adds state5
# only after the second ranking and does not reset the sequence every update.
for credits,free in ((0,0),(1,0),(65535,0),(0,1)):
    s=State(1,0,credits,4,4)
    e,q=step(s,Input(credits,0,0,free))
    assert s.state==(5 if credits or free else 0)
    if s.state==5:
        step(s,Input(credits,0,0,free));step(s,Input(credits,0,0,free))
        assert s.timer==240
        for n in range(240):
            step(s,Input(credits,n%256,0,free))
            assert s.state==(0 if n==239 else 5)
        e,q=step(s,Input(credits,0,0,free))
        assert s.state==1 and e.flags==1

report={'calls_checked':checks,'passed':True,
        'scope':'Host source-contract tests: word/byte countdown boundaries, all clock phases, queue order, busy credit interruptions and credited/freeplay restart. Renderer, prelude actors, live source clock and original-CPU differential execution pending.'}
(out/'flow-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

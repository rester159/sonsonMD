#!/usr/bin/env python3
"""Native conversion-pickup collection versus recorded original E7D6/E7DF."""
from pathlib import Path
import csv
import ctypes as C
import hashlib
import json
import subprocess

r = Path(__file__).resolve().parents[1]
out = r/'out/parity-audit/powerup-collection'
out.mkdir(parents=True, exist_ok=True)
source = r/'src/arcade_powerup.c'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib',
                '-I'+str(r/'inc'),str(source),'-o',str(out/'test.dylib')], check=True)
names = ['y','active','state','phase','kind','placement','timer','anim_timer',
         'cursor','code','attr','exit_latched','owner']
class Actor(C.Structure):
    _fields_ = [('x',C.c_uint16)]+[(name,C.c_uint8) for name in names]
lib = C.CDLL(str(out/'test.dylib'))
collect = lib.arcade_powerup_collect_conversion
collect.argtypes = [C.POINTER(Actor),C.c_uint16,C.c_uint8,C.c_uint8,
                    C.POINTER(C.c_uint8),C.POINTER(C.c_uint8),C.POINTER(C.c_uint8)]
collect.restype = C.c_uint8
trace = r/'out/parity-audit/conversion-trigger.csv'
assert hashlib.sha256(trace.read_bytes()).hexdigest() == 'f8568925852235e1e0462afcf247c069b1083a15ef8e8e08c4f04b96ec2772a4'
rows = list(csv.DictReader(trace.open()))
assert len(rows) == 900
for raw in rows:
    row = {k:int(v) for k,v in raw.items()}
    a = Actor(x=2,y=3,active=row['active'],state=2,phase=7,kind=255,
              owner=1-row['owner'],placement=13,timer=32,anim_timer=4,
              cursor=2,code=0x9d,attr=0xcb,exit_latched=1)
    before = {name:getattr(a,name) for name,_ in Actor._fields_}
    limit,next_index,conversion = C.c_uint8(row['limit']),C.c_uint8(9),C.c_uint8(0)
    hit = collect(C.byref(a),(2+row['dx']) & 65535,(3+row['dy']) & 255,
                  row['owner'],C.byref(limit),C.byref(next_index),C.byref(conversion))
    assert (hit,limit.value,next_index.value,conversion.value) == (
        row['collected'],row['after_limit'],row['next_index'],row['purge']), row
    # These writes are checked by the original CPU trace producer as well.
    expected = dict(before)
    if row['collected']:
        expected.update(active=2,state=3,phase=0,owner=row['owner'])
    assert {name:getattr(a,name) for name,_ in Actor._fields_} == expected, row
report = dict(cases=len(rows),native_bytes=C.sizeof(Actor),
              trace_sha256=hashlib.sha256(trace.read_bytes()).hexdigest(),
              source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
              scope='Host native FF collection against original CPU boundary fixtures, both owners and byte/word coordinate wrap. Caller eligibility, natural spawning, target execution and live conversion binding pending.')
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

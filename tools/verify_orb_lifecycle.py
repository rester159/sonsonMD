#!/usr/bin/env python3
"""Compare native carrier state and parent writes with independent E1AF traces."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import subprocess

r=Path(__file__).resolve().parents[1]
o=r/'out/parity-audit/orb-lifecycle';o.mkdir(exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_orb.c'),'-o',str(o/'test.dylib')],check=True)
class Orb(C.Structure):
    _fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ('subx','y','suby','active','state','phase','variant','direction','timer','anim_timer','cursor','code','attr')]
class Parent(C.Structure):
    _fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ('y','active','state','phase','kind','placement','timer','anim_timer','cursor','code','attr','exit_latched','owner')]
lib=C.CDLL(str(o/'test.dylib'))
lib.arcade_orb_step.argtypes=[C.POINTER(Orb),C.POINTER(Parent),C.c_uint16]
def orb(b):
    ptr=int.from_bytes(b[14:16],'big');root=0xe24d if b[3] else 0xe26f
    cursor=(ptr-root)//3 if ptr else 0
    return [int.from_bytes(b[5:7],'big')]+[b[k]for k in (7,8,9,0,1,2,3,10,29,13)]+[cursor,b[16],b[17]]
def parent(b):
    return [int.from_bytes(b[5:7],'big')]+[b[k]for k in (8,0,1,2,3,28,29,13)]+[0,b[16],b[17],b[19],b[4]]
def values(a):return [getattr(a,n)for n,_ in a._fields_]
trace=r/'out/parity-audit/orb-lifecycle.jsonl'
rows=[json.loads(s)for s in trace.read_text().splitlines()]
assert len(rows)==12288,len(rows)
returns=0
for row in rows:
    before=bytes.fromhex(row['before']);after=bytes.fromhex(row['after'])
    pb=bytes.fromhex(row['parent_before']);pa=bytes.fromhex(row['parent_after'])
    if row['tick']==0:a=Orb(*orb(before));p=Parent(*parent(pb))
    assert values(a)==orb(before),('continuity',row)
    assert values(p)==parent(pb),('parent continuity',row)
    lib.arcade_orb_step(C.byref(a),C.byref(p),row['camera'])
    assert values(a)==orb(after),(row,values(a),orb(after))
    assert values(p)==parent(pa),(row,values(p),parent(pa))
    if before[0] and not after[0]:returns+=1
    assert all(pb[k]==pa[k]for k in range(32)if k not in (0,1,2,5,6)),row
assert returns==24,returns
report={'updates':len(rows),'trajectories':24,'parent_returns':returns,'error':None,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'scope':'Isolated original E1AF versus native host: two variants, four Y boundaries, stopped/moving/wrapped camera. Live owner, rendering, collision and target integration pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

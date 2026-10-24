#!/usr/bin/env python3
"""Compiled native placements/pool against original54E7 copy transactions."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];out=r/'out/parity-audit/world-objects'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_world_objects.c'),str(r/'src/arcade_world_object_pool.c'),'-o',str(out/'test.dylib')],check=True)
class Object(C.Structure):
    _fields_=[('x',C.c_uint16),('source_map',C.c_uint16)]+[(n,C.c_uint8)for n in ['active','phase','y','food','timer','kind']]
class Pool(C.Structure):
    _fields_=[('objects',Object*20),('block',C.c_uint8),('quarter',C.c_uint8)]
lib=C.CDLL(str(out/'test.dylib'));lib.arcade_world_object_load.argtypes=[C.POINTER(Pool)]
lib.arcade_world_object_begin.argtypes=[C.POINTER(Pool)]
def source_bytes(pool):
    actual=bytearray()
    for obj in pool.objects:
        actual+=bytes([obj.active,obj.phase])+obj.source_map.to_bytes(2,'big')+obj.x.to_bytes(2,'big')+bytes([obj.y,obj.food,obj.timer,obj.kind])
    return actual
trace=out/'loader.jsonl';rows=[json.loads(s)for s in trace.read_text().splitlines()];assert len(rows)==440
for row in rows:
    p=Pool();C.memset(C.addressof(p),0xa5,C.sizeof(p));p.block=row['block'];p.quarter=row['quarter']
    result=lib.arcade_world_object_load(C.byref(p))
    assert bool(result)==(row['block']<87)
    assert 0x5587+p.block*40==row['source'] and 0x560+p.quarter*40==row['destination'],row
    assert source_bytes(p).hex()==row['pool'],row
rom=b''.join((r/'arcade'/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e'])
offsets=(C.c_uint16*27).in_dll(lib,'arcade_world_object_offsets')
for i,offset in enumerate(offsets):
    assert 0x5587+offset*10==int.from_bytes(rom[0x5551-0x4000+i*2:0x5553-0x4000+i*2],'big')
objects=(Object*348).in_dll(lib,'arcade_world_objects')
report=dict(cases=len(rows),pool_bytes_checked=len(rows)*200,segment_offsets=27,
            kinds={str(k):sum(o.kind==k for o in objects)for k in range(3)},
            trace_sha256=hashlib.sha256(trace.read_bytes()).hexdigest(),
            source_sha256=hashlib.sha256((r/'src/arcade_world_object_pool.c').read_bytes()).hexdigest(),
            scope='Compiled full placement table and four-record rotating pool against original54E7. Source end and all five destination quarters. Live scroll calls, object update/render/contact binding and target CPU validation pending.')
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

# Keep this separate from the unmodified 54E7 evidence above: this fixture
# overrides ANDCC EF to ANDCC FF to suppress board interrupts during 54B0.
# It proves the copy result only, not normal startup/interrupt sequencing.
boot_trace=out/'boot-masked.jsonl'
boot_rows=[json.loads(s)for s in boot_trace.read_text().splitlines()]
assert len(boot_rows)==5
assert {row['quarter']for row in boot_rows}==set(range(5))
for row in boot_rows:
    assert row['block']==-1
    p=Pool();C.memset(C.addressof(p),0xa5,C.sizeof(p))
    p.quarter=row['quarter']
    lib.arcade_world_object_begin(C.byref(p))
    assert (p.block,p.quarter)==(4,4)
    assert 0x5587+p.block*40==row['source']
    assert 0x560+p.quarter*40==row['destination']
    assert source_bytes(p).hex()==row['pool'],row
boot_report=dict(cases=len(boot_rows),pool_bytes_checked=len(boot_rows)*200,
    trace_sha256=hashlib.sha256(boot_trace.read_bytes()).hexdigest(),
    source_sha256=report['source_sha256'],
    scope='Isolated segment0 preload copy with IRQ unmask operand overridden. Tail active flags cleared, other tail fields preserved. Does not prove uninterrupted arcade startup or live Genesis integration.')
(out/'boot-copy-results.json').write_text(json.dumps(boot_report,indent=2)+'\n')
print(json.dumps(boot_report,indent=2))

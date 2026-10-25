#!/usr/bin/env python3
"""Native barrier lifecycle and source background writes versus B36F traces."""
from pathlib import Path
import ctypes as C,json,hashlib,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/barrier-lifecycle';o.mkdir(exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_barrier_step.c'),str(r/'src/arcade_barrier_data.c'),'-o',str(o/'test.dylib')],check=True)
class Object(C.Structure):
    _fields_=[('x',C.c_uint16),('source_map',C.c_uint16)]+[(n,C.c_uint8)for n in ('active','phase','y','food','timer','kind')]
class Draw(C.Structure):
    _fields_=[('write',C.c_uint8),('row',C.c_uint8),('pattern',C.POINTER(C.c_uint8))]
lib=C.CDLL(str(o/'test.dylib'));lib.arcade_barrier_step.argtypes=[C.POINTER(Object),C.POINTER(C.c_uint8),C.c_uint16,C.c_uint8,C.c_uint8,C.POINTER(Draw),C.POINTER(C.c_uint8)]
trace=r/'out/parity-audit/barrier-lifecycle.jsonl';rows=[json.loads(s)for s in trace.read_text().splitlines()];assert len(rows)==6144,len(rows)
roots=[0xb7e3,0xb80b,0xb833,0xb85b]
rom=b"".join((r/"arcade"/n).read_bytes()for n in ("ss.01e","ss.02e","ss.03e"))
lib.arcade_barrier_next_phase.argtypes=[C.c_uint8]
lib.arcade_barrier_next_phase.restype=C.c_uint8
def values(b):return [int.from_bytes(b[4:6],'big'),0]+[b[k]for k in (0,1,6,7,8,9)]
for row in rows:
    before=bytes.fromhex(row['before']);after=bytes.fromhex(row['after'])
    if row['tick']==0:a=Object(*values(before));cursor=C.c_uint8(0)
    a.phase=before[1] # Only scripted source hits alter phase between updates.
    assert [getattr(a,n)for n,_ in a._fields_]==values(before),row
    draw=Draw();flash=C.c_uint8()
    lib.arcade_barrier_step(C.byref(a),C.byref(cursor),row['camera'],row['tick']%3==0,row['tick']&255,C.byref(draw),C.byref(flash))
    assert [getattr(a,n)for n,_ in a._fields_]==values(after),(row,values(after),a.active,a.phase,a.food,a.timer)
    ptr=int.from_bytes(after[2:4],'big')
    expected_cursor=row['variant']*10+(ptr-roots[row['variant']])//4 if ptr else 0
    assert cursor.value==expected_cursor,(row,cursor.value,expected_cursor)
    if a.active&2:assert lib.arcade_barrier_next_phase(cursor.value)==rom[ptr+4-0x4000],row
    strip=bytes([85]*4);patch=bytes([85]*8)
    if draw.write in (1,2,3):
        pattern=bytes(draw.pattern[:8])
        if draw.write==1:strip=pattern[:2]+pattern[4:6]
        elif draw.write==2:strip=pattern[2:4]+pattern[6:8]
        else:patch=pattern
    elif draw.write==4:patch=bytes([85]*4+[flash.value]*4)
    assert strip.hex()==row['strip'],(row,draw.write,strip.hex())
    assert patch.hex()==row['patch'],(row,draw.write,patch.hex())
report={'updates':len(rows),'trajectories':12,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'scope':'Original B36F kind2: four variants, stopped/moving/wrapped camera, scripted repeated hits, exact actor state/cursor and strip/plane writes. Live rendering/contact and target integration pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

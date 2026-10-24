#!/usr/bin/env python3
"""Initial food redraw versus original B922 CPU background writes."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/food-redraw';o.mkdir(exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_food_step.c'),str(r/'src/arcade_food_patterns.c'),'-o',str(o/'test.dylib')],check=True)
class Object(C.Structure):
    _fields_=[('x',C.c_uint16),('source_map',C.c_uint16)]+[(n,C.c_uint8)for n in ['active','phase','y','food','timer','kind']]
class Draw(C.Structure):
    _fields_=[('write',C.c_uint8),('row',C.c_uint8),('pattern',C.POINTER(C.c_uint8))]
lib=C.CDLL(str(o/'test.dylib'));lib.arcade_food_redraw.argtypes=[C.POINTER(Object),C.c_uint16,C.POINTER(Draw)]
trace=r/'out/parity-audit/food-redraw.jsonl';rows=[json.loads(s)for s in trace.read_text().splitlines()];assert len(rows)==2880
for row in rows:
    a=Object((row['camera']+row['delta'])&65535,0x1100,row['active'],5,row['y'],row['food'],0,0);d=Draw()
    lib.arcade_food_redraw(C.byref(a),row['camera'],C.byref(d))
    raw=bytes([a.active,a.phase])+a.source_map.to_bytes(2,'big')+a.x.to_bytes(2,'big')+bytes([a.y,a.food,a.timer,a.kind])
    assert raw.hex()==row['object'],(row,raw.hex())
    assert d.write in (0,3)
    patch=bytes(d.pattern[:8]) if d.write==3 else bytes([165]*8)
    assert patch.hex()==row['patch'],(row,patch.hex())
report=dict(cases=len(rows),actor_bytes_checked=len(rows)*10,background_bytes_checked=len(rows)*8,
            trace_sha256=hashlib.sha256(trace.read_bytes()).hexdigest(),
            source_sha256=hashlib.sha256((r/'src/arcade_food_step.c').read_bytes()).hexdigest(),
            scope='Initial kind0 redraw against B922 CPU writes. All20 foods, active flags0..7, entry thresholds255/256/257, normal and wrapping cameras, three rows. Live rendering and target execution pending.')
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

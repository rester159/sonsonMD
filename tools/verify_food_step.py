#!/usr/bin/env python3
"""Native food draw events versus original CPU background writes."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/food-step';o.mkdir(exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_food_step.c'),str(r/'src/arcade_food_patterns.c'),'-o',str(o/'test.dylib')],check=True)
class Object(C.Structure):
    _fields_=[('x',C.c_uint16),('source_map',C.c_uint16)]+[(n,C.c_uint8)for n in ['active','phase','y','food','timer','kind']]
class Draw(C.Structure):
    _fields_=[('write',C.c_uint8),('row',C.c_uint8),('pattern',C.POINTER(C.c_uint8))]
lib=C.CDLL(str(o/'test.dylib'));lib.arcade_food_step.argtypes=[C.POINTER(Object),C.c_uint16,C.c_uint8,C.POINTER(Draw)]
trace=r/'out/parity-audit/food-step.jsonl';rows=[json.loads(s)for s in trace.read_text().splitlines()];assert len(rows)==2160
for row in rows:
    a=Object(1000+row['delta'],0x1100,4,row['phase'],row['y'],row['food'],0,0);d=Draw()
    lib.arcade_food_step(C.byref(a),1000,row['advance'],C.byref(d))
    raw=bytes([a.active,a.phase])+a.source_map.to_bytes(2,'big')+a.x.to_bytes(2,'big')+bytes([a.y,a.food,a.timer,a.kind])
    assert raw.hex()==row['object'],(row,raw.hex())
    strip=bytearray([165]*65);patch=bytes([165]*8)
    if d.write in (1,2):
        k=0 if d.write==1 else 2
        strip[d.row:d.row+2]=bytes(d.pattern[k:k+2])
        strip[32+d.row:34+d.row]=bytes(d.pattern[4+k:6+k])
    elif d.write==3:patch=bytes(d.pattern[:8])
    else:assert d.write==0
    assert strip.hex()==row['strip'] and patch.hex()==row['patch'],(row,strip.hex(),patch.hex())
report=dict(cases=len(rows),actor_bytes_checked=len(rows)*10,background_bytes_checked=len(rows)*73,
            trace_sha256=hashlib.sha256(trace.read_bytes()).hexdigest(),
            source_sha256=hashlib.sha256((r/'src/arcade_food_step.c').read_bytes()).hexdigest(),
            scope='Native kind0 phases0..5 and generated patterns against actual source CPU background writes. All20 food codes, advance gates, camera entry boundaries, y0/104/255. Genesis tile translation, live rendering and target execution pending.')
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

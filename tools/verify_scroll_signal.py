#!/usr/bin/env python3
"""Source-observed scroll arithmetic and column pulse, exhaustive fractions."""
from pathlib import Path
import ctypes as C,csv,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/scroll-signal';o.mkdir(exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_scroll.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'));fn=lib.arcade_scroll_step
fn.argtypes=[C.POINTER(C.c_uint16),C.POINTER(C.c_uint16),C.POINTER(C.c_uint8)];fn.restype=C.c_uint8
trace=r/'out/parity-audit/scroll-signal.csv';rows=list(csv.DictReader(trace.open()));assert len(rows)==3584
for row in rows:
 cam=C.c_uint16(int(row['camera']));sub=C.c_uint16(int(row['sub']));latch=C.c_uint8(int(row['latch']))
 pulse=fn(C.byref(cam),C.byref(sub),C.byref(latch))
 assert [cam.value,sub.value,latch.value,pulse]==[int(row[k])for k in ['after_camera','after_sub','after_latch','advance']],row
report={'cases':len(rows),'fields':len(rows)*4,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'source_sha256':hashlib.sha256((r/'src/arcade_scroll.c').read_bytes()).hexdigest(),'scope':'Host A697 arithmetic and column-pulse outputs against original CPU including actual column expansion. Stop-state dispatch and native map-event application excluded.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

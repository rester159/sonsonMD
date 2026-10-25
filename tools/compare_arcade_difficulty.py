#!/usr/bin/env python3
"""Native difficulty boundary contracts versus original MC6809 execution."""
import csv,ctypes as C,hashlib,json,pathlib,subprocess
r=pathlib.Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_difficulty.c'),'-o',str(o/'arcade_difficulty.dylib')],check=True)
lib=C.CDLL(str(o/'arcade_difficulty.dylib'))
lib.arcade_difficulty_add.argtypes=[C.c_uint16,C.c_uint16];lib.arcade_difficulty_add.restype=C.c_uint16
lib.arcade_difficulty_seam.argtypes=[C.POINTER(C.c_uint8),C.c_uint16];lib.arcade_difficulty_seam.restype=C.c_uint16
program=b''.join((r/'arcade'/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e'])
program_hash=hashlib.sha256(program).hexdigest()
assert program_hash=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
trace=o/'difficulty-oracle.csv';rows=list(csv.DictReader(trace.open()))
assert len(rows)==9216,len(rows)
for row in rows:
 d={k:int(v)for k,v in row.items()};base=C.c_uint8(d['base'])
 result=lib.arcade_difficulty_add(d['combat'],d['amount'])if d['operation']==0 else lib.arcade_difficulty_seam(C.byref(base),d['combat'])
 assert (base.value,result)==(d['result_base'],d['result_combat']),d
report={'program_sha256':program_hash,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'cases':len(rows),'field_checks':len(rows)*2,'differences':0,'scope':'isolated original CPU difficulty add and scenery seam routines; not full gameplay'}
(o/'difficulty-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

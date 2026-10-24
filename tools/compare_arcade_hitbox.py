#!/usr/bin/env python3
"""Compare native G/H hit geometry with actual original CPU collision leaves."""
import csv,ctypes as C,hashlib,json,pathlib,subprocess
r=pathlib.Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_collision.c'),'-o',str(o/'arcade_collision.dylib')],check=True)
lib=C.CDLL(str(o/'arcade_collision.dylib'));fn=lib.arcade_gh_shot_overlap
fn.argtypes=[C.c_uint16,C.c_uint8,C.c_uint16,C.c_uint8];fn.restype=C.c_uint8
program=b''.join((r/'arcade'/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e']);ph=hashlib.sha256(program).hexdigest()
assert ph=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
trace=o/'hitbox-oracle.csv';rows=list(csv.DictReader(trace.open()));assert len(rows)==7748,len(rows)
hits=0
for row in rows:
 d={k:int(v)for k,v in row.items()};actual=fn(d['enemy_x'],d['enemy_y'],d['shot_x'],d['shot_y'])
 assert actual==d['hit'],d;hits+=actual
report={'program_sha256':ph,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'cases':len(rows),'hits':hits,'differences':0,'scope':'isolated original CPU G/H collision leaf geometry, not family order or full kill transactions'}
(o/'hitbox-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

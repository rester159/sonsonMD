#!/usr/bin/env python3
"""Compare ordinary contact eligibility and wrapped geometry with original CPU."""
import csv,ctypes as C,hashlib,json,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_collision.c'),'-o',str(o/'arcade_contact.dylib')],check=True)
fn=C.CDLL(str(o/'arcade_contact.dylib')).arcade_body_contact;fn.argtypes=[C.c_uint16,C.c_uint8,C.c_uint16,C.c_uint8,C.c_uint8,C.c_uint8];fn.restype=C.c_uint8
sha=lambda b:hashlib.sha256(b).hexdigest()
program=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'));assert sha(program)=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
trace=o/'contact-oracle.csv';rows=list(csv.DictReader(trace.open()));assert len(rows)==6060,len(rows)
hits=0
for row in rows:
 d={k:int(v)for k,v in row.items()};actual=fn(*[d[n]for n in ('player_x','player_y','enemy_x','enemy_y','player_active','enemy_active')]);assert actual==d['hit'],(d,actual);hits+=actual
report={'program_sha256':sha(program),'trace_sha256':sha(trace.read_bytes()),'source_sha256':sha((r/'src/arcade_collision.c').read_bytes()),'cases':len(rows),'hits':hits,'differences':0,'scope':'original G/H/I ordinary contact scans for both players; bonus bit64 excluded; page/lifecycle gates separate'}
(o/'contact-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

#!/usr/bin/env python3
"""Original collision and score-handler transactions versus native rewards."""
import csv,ctypes as C,hashlib,json,pathlib,subprocess
r=pathlib.Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_i_combat.c'),str(r/'src/arcade_collision.c'),str(r/'src/arcade_difficulty.c'),'-o',str(o/'arcade_i_rewards.dylib')],check=True)
class Kill(C.Structure):_fields_=[(n,C.c_uint16)for n in ['combat','points','event','bonus']]+[(n,C.c_uint8)for n in ['remaining','phase','owner']]
lib=C.CDLL(str(o/'arcade_i_rewards.dylib'));fn=lib.arcade_i_kill
fn.argtypes=[C.POINTER(Kill),C.c_uint16,C.c_uint8,C.c_uint8]
program=b''.join((r/'arcade'/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e'])
program_hash=hashlib.sha256(program).hexdigest()
assert program_hash=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
trace=o/'i-reward-oracle.csv';rows=list(csv.DictReader(trace.open()));assert len(rows)==4096,len(rows)
checks=0
for row in rows:
 d={k:int(v)for k,v in row.items()};a=Kill();fn(C.byref(a),d['combat'],d['remaining'],d['owner'])
 for actual,expected in [('combat','after_combat'),('points','score'),('event','event'),('bonus','bonus'),('remaining','after_remaining'),('phase','phase'),('owner','style')]:
  checks+=1;assert getattr(a,actual)==d[expected],(actual,d,getattr(a,actual))
 assert (d['enemy_active'],d['shot_active'],d['shot_state'])==(2,2,512),d
report={'program_sha256':program_hash,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'cases':len(rows),'field_checks':checks,'differences':0,'scope':'original class-I kill leaf plus original score event handler, isolated successful hits; family scan/reachability separate'}
(o/'i-reward-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

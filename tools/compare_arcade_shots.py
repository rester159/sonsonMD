#!/usr/bin/env python3
"""Check original player-shot motion, animation, release and availability."""
import ctypes as C,hashlib,json,pathlib,subprocess
r=pathlib.Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_shots.c'),'-o',str(o/'arcade_shots.dylib')],check=True)
class Shot(C.Structure):_fields_=[('x',C.c_uint16),('origin',C.c_uint16)]+[(n,C.c_uint8)for n in ['xf','y','yf','active','state','phase','facing','timer','anim_timer','cursor','pose']]
lib=C.CDLL(str(o/'arcade_shots.dylib'));lib.arcade_player_shot_step.argtypes=[C.POINTER(Shot),C.c_uint16,C.POINTER(C.c_uint8)]
cursors={0xafec:0,0xafef:1,0xaff4:2,0xaff7:3};poses={0x46:0,0x47:1,0x4e:2,0x4f:3}
def decode(s):
 b=bytes.fromhex(s);return Shot(int.from_bytes(b[5:7],'big'),int.from_bytes(b[24:26],'big'),b[7],b[8],b[9],b[0],b[1],b[2],b[10],b[26],b[13],cursors[int.from_bytes(b[14:16],'big')],poses[b[16]])
program=b''.join((r/'arcade'/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e'])
program_hash=hashlib.sha256(program).hexdigest()
assert program_hash=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
trace=o/'shot-oracle.jsonl';rows=[json.loads(x)for x in trace.read_text().splitlines()];assert len(rows)==1920,len(rows)
checks=0;bound=[]
for row in rows:
 a=decode(row['before']);expected=decode(row['after']);available=C.c_uint8(row['available'])
 bound.append((bytes(a),bytes(expected),row['camera'],row['available'],row['after_available']))
 lib.arcade_player_shot_step(C.byref(a),row['camera'],C.byref(available))
 for field,_ in Shot._fields_:
  checks+=1;assert getattr(a,field)==getattr(expected,field),(field,row,getattr(a,field),getattr(expected,field))
 checks+=1;assert available.value==row['after_available'],row
target=o/'shot-target.csv'
with target.open('w')as f:
 f.write('before,after,camera,available,after_available\n')
 for before,after,camera,available,after_available in bound:
  fields=[]
  for raw in [before,after]:
   b=bytearray(raw)
   b[0:2]=b[0:2][::-1];b[2:4]=b[2:4][::-1];fields.append(b[:15].hex())
  f.write(f'{fields[0]},{fields[1]},{camera},{available},{after_available}\n')
report={'program_sha256':program_hash,'target_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'cases':len(rows),'field_checks':checks,'differences':0,'scope':'48 isolated original shot trajectories: birth, range, hit, offscreen release, animation and availability; spawn input timing separate'}
(o/'shot-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

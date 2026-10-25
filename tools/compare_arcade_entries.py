#!/usr/bin/env python3
"""Check G/H entry arithmetic and delay phases against original CPU calls."""
import ctypes as C,hashlib,json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1];out=root/'out/parity-audit'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(root/'inc'),str(root/'src/arcade_waves.c'),str(root/'src/arcade_formations.c'),'-o',str(out/'arcade_entries.dylib')],check=True)
class Actor(C.Structure):_fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ['xf','y','yf','active','state','phase','delay','facing']]
lib=C.CDLL(str(out/'arcade_entries.dylib'));lib.arcade_formation_enter.argtypes=[C.POINTER(Actor),C.c_uint8,C.c_uint16,C.c_uint8]
def decode(s):
 b=bytes.fromhex(s)
 return Actor(int.from_bytes(b[5:7],'big'),b[7],b[8],b[9],b[0],b[1],b[2],b[29],b[10])
checks=0;frames=0;cases=0
trace=out/'entry-oracle-v2.jsonl';data=trace.read_bytes()
for line in data.splitlines():
 row=json.loads(line);a=decode(row['before']);expected=decode(row['after'])
 lib.arcade_formation_enter(C.byref(a),row['family'],row['camera'],row['stopped'])
 for name,_ in Actor._fields_:
  checks+=1
  assert getattr(a,name)==getattr(expected,name),{'field':name,'actual':getattr(a,name),'expected':getattr(expected,name),'row':row}
 frames+=1;cases+=row['tick']==0
assert cases==80 and frames>1000,(cases,frames)
result={'source':'Original MC6809 G/H entry routines, isolated JSR fixture','trace_sha256':hashlib.sha256(data).hexdigest(),'cases':cases,'frames':frames,'field_checks':checks,'differences':0}
(out/'entry-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

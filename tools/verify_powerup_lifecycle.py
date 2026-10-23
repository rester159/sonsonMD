#!/usr/bin/env python3
"""Native FE/FF animation and lifecycle versus continuous original CPU traces."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/powerup-lifecycle';o.mkdir(exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_powerup.c'),'-o',str(o/'test.dylib')],check=True)
fields=['y','active','state','phase','kind','placement','timer','anim_timer','cursor','code','attr','exit_latched','owner']
class Actor(C.Structure):_fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in fields]
lib=C.CDLL(str(o/'test.dylib'));lib.arcade_powerup_step.argtypes=[C.POINTER(Actor),C.c_uint16]
trace=r/'out/parity-audit/powerup-lifecycle.jsonl';rows=[json.loads(s)for s in trace.read_text().splitlines()];assert len(rows)==12288
def convert(b,kind):
 ptr=int.from_bytes(b[14:16],'big');root=0xb2a7 if kind==254 else 0xb2c1
 cursor=(ptr-root)//3 if ptr else 0
 assert ptr==0 or ptr==root+cursor*3 and 0<=cursor<(8 if kind==254 else 4)
 return [int.from_bytes(b[5:7],'big')]+[b[k]for k in [8,0,1,2,3,28,29,13]]+[cursor,b[16],b[17],b[19],b[4]]
def values(a):return [a.x]+[getattr(a,n)for n in fields]
for row in rows:
 before=bytes.fromhex(row['before']);after=bytes.fromhex(row['after']);expected=convert(after,row['kind'])
 if row['tick']==0:a=Actor(*convert(before,row['kind']))
 elif row['tick']==64 and row['scenario']==2:a.state=3;a.phase=0
 assert values(a)==convert(before,row['kind']),('continuity',row)
 lib.arcade_powerup_step(C.byref(a),row['camera'])
 assert values(a)==expected,(row,values(a),expected)
 allowed={0,1,2,5,6,8,13,14,15,16,17,19,29}
 assert all(before[k]==after[k]for k in range(32)if k not in allowed),row
report={'cases':len(rows),'trajectories':24,'field_checks':len(rows)*14,'native_bytes':C.sizeof(Actor),'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'native_sha256':hashlib.sha256((r/'src/arcade_powerup.c').read_bytes()).hexdigest(),'scope':'Host FE/FF special-object lifetime and animation versus original B19A. Moving/stopped cameras with16-bit wrap; four placements; scripted retirement. Spawn/contact/render integration pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

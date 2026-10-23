#!/usr/bin/env python3
"""Native FE/FF animation and lifecycle versus continuous original CPU traces."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/ordinary-pickup-lifecycle';o.mkdir(exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_pickup.c'),str(r/'src/arcade_bonus_art.c'),'-o',str(o/'test.dylib')],check=True)
fields=['y','active','state','phase','kind','placement','timer','anim_timer','cursor','code','attr','exit_latched','owner']
class Actor(C.Structure):_fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in fields]
lib=C.CDLL(str(o/'test.dylib'));lib.arcade_pickup_step.argtypes=[C.POINTER(Actor),C.c_uint16]
trace=r/'out/parity-audit/ordinary-pickup-lifecycle.jsonl';rows=[json.loads(s)for s in trace.read_text().splitlines()];assert len(rows)==30720
addresses={f['address']:i for i,f in enumerate(json.loads((r/'out/parity-audit/bonus-art-map.json').read_text())['frames'])}
def convert(b,kind):
 ptr=int.from_bytes(b[14:16],'big');cursor=addresses[ptr] if ptr else 0
 return [int.from_bytes(b[5:7],'big')]+[b[k]for k in [8,0,1,2,3,28,29,13]]+[cursor,b[16],b[17],b[19],b[4]]
def values(a):return [a.x]+[getattr(a,n)for n in fields]
for row in rows:
 before=bytes.fromhex(row['before']);after=bytes.fromhex(row['after']);expected=convert(after,row['kind'])
 if row['tick']==0:a=Actor(*convert(before,row['kind']))
 elif row['tick']==64 and row['scenario']==2:a.active=2;a.state=1;a.phase=0
 assert values(a)==convert(before,row['kind']),('continuity',row)
 orb=bytes.fromhex(row['orb'])
 event=lib.arcade_pickup_step(C.byref(a),row['camera'])
 assert event==(orb[3]+1 if orb[0] else 0),(row,event)
 if event:assert orb[0]==1 and orb[1:3]==bytes(2) and int.from_bytes(orb[30:32],'big')==0x708,row
 assert values(a)==expected,(row,values(a),expected)
 allowed={0,1,2,3,5,6,8,13,14,15,16,17,19,29}
 assert all(before[k]==after[k]for k in range(32)if k not in allowed),row
report={'cases':len(rows),'trajectories':60,'field_checks':len(rows)*14,'native_bytes':C.sizeof(Actor),'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'native_sha256':hashlib.sha256((r/'src/arcade_pickup.c').read_bytes()).hexdigest(),'scope':'Host ordinary kinds0..9 versus original B19A: both owners, moving/stopped wrapped camera, scripted collection and missed-fruit orb requests. Kind10 and live binding pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

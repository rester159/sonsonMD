#!/usr/bin/env python3
"""Original death dispatcher states0..6, excluding input-sensitive reentry."""
from pathlib import Path
import ctypes as C,hashlib,json,struct,subprocess,collections
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
class Life(C.Structure):
 _fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ('xf','y','yf','active','state','phase','kind','owner','facing','timer','reentry_lock','cursor','anim_timer','code','attr')]
assert C.sizeof(Life)==18
addresses=[base+3*k for base in (0xaffc,0xb00a,0xafac,0xafd0)for k in range(4)]+[0xaf9a,0xaf9f]
def decode(raw):
 b=bytes.fromhex(raw);p=int.from_bytes(b[14:16],'big')
 return Life(int.from_bytes(b[5:7],'big'),b[7],b[8],b[9],b[0],b[1],b[2],b[3],b[4],b[10],b[26],b[27],addresses.index(p)if p else 255,b[13],b[16],b[17])
def pack(a):return struct.pack('>H15B',a.x,*[getattr(a,k)for k,_ in Life._fields_[1:]])
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_life.c'),'-o',str(o/'arcade_life.dylib')],check=True)
fn=C.CDLL(str(o/'arcade_life.dylib')).arcade_life_step;fn.argtypes=[C.POINTER(Life),C.c_uint16,C.c_uint8,C.POINTER(C.c_uint8),C.POINTER(C.c_uint8),C.POINTER(C.c_uint16),C.POINTER(C.c_uint8)];fn.restype=C.c_uint8
sha=lambda b:hashlib.sha256(b).hexdigest();program=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'));assert sha(program)=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
trace=o/'life-oracle.jsonl';rows=[json.loads(x)for x in trace.read_text().splitlines()];assert 'LIFE_DONE'in(o/'life-source.log').read_text()
assert sum(d['tick']==0 for d in rows)==224
states=collections.Counter();target=o/'life-target.csv'
with target.open('w')as f:
 f.write('life,camera,stopped,reserves,gate,after_life,after_reserves,after_gate,event,sound\n')
 for d in rows:
  a=decode(d['before']);before=pack(a);expected=decode(d['after']);res=C.c_uint8(d['reserves']);gate=C.c_uint8(d['gate']);event=C.c_uint16();sound=C.c_uint8();states[a.state]+=1
  assert fn(C.byref(a),d['camera'],d['stopped'],C.byref(res),C.byref(gate),C.byref(event),C.byref(sound))==1
  assert pack(a)==pack(expected),(d,pack(a).hex(),pack(expected).hex())
  assert(res.value,gate.value,event.value)==(d['after_reserves'],d['after_gate'],d['event']),d
  expected_sound=bytes.fromhex(d['sound']);assert expected_sound==(bytes([128,sound.value])+bytes(6)if sound.value else bytes(8)),(d,sound.value)
  f.write(f'{before.hex()},{d["camera"]},{d["stopped"]},{d["reserves"]},{d["gate"]},{pack(expected).hex()},{d["after_reserves"]},{d["after_gate"]},{d["event"]},{expected_sound[1]}\n')
report={'scope':'Original AD9B death states0..6 only; reentry state7 and lifecycle integration pending','program_sha256':sha(program),'trace_sha256':sha(trace.read_bytes()),'target_sha256':sha(target.read_bytes()),'native_sha256':sha((r/'src/arcade_life.c').read_bytes()),'trajectories':224,'updates':len(rows),'states':dict(states),'byte_checks':len(rows)*22,'differences':0}
(o/'life-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

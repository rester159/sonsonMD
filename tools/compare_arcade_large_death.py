#!/usr/bin/env python3
"""Continuous native death/exit timelines vs original D55B records."""
from pathlib import Path
import csv,ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit';trace=o/'large-death-oracle.csv';(o/'large-death-comparison.json').unlink(missing_ok=True)
assert 'LARGE_DEATH_DONE'in(o/'large-death-source/mame.log').read_text()
sources=[r/'src'/n for n in('arcade_large_death.c','arcade_large_animation.c','arcade_large_animation_data.c')]
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),*map(str,sources),'-o',str(o/'large-death.dylib')],check=True)
class Animation(C.Structure):_fields_=[('cursor',C.c_uint16),('timer',C.c_uint8),('code',C.c_uint8*4),('attr',C.c_uint8*4)]
class Body(C.Structure):_fields_=[('animation',Animation)]+[(k,C.c_uint8)for k in('active','state','phase','subtype','variant','owner','hp','timer')]
lib=C.CDLL(str(o/'large-death.dylib'));lib.arcade_large_death_step.argtypes=[C.POINTER(Body)]
meta=json.loads((o/'large-animation-map.json').read_text());ids={a:k for k,a in enumerate(meta['addresses'])}
def convert(b):
 ptr=int.from_bytes(b[14:16],'big')
 if ptr in ids:cursor=ids[ptr]
 else:
  # D80A's terminal score-frame next pointer is never followed: lifetime12<127.
  assert b[2]in(2,3)or not b[0]
  cursor=ids[meta['score_roots'][b[27]*3+b[3]]]
 return cursor.to_bytes(2,'big')+b[13:14]+b[16:24]+bytes(b[k]for k in(0,1,2,3,27,4,24,29))
def load(v):return Body(Animation(int.from_bytes(v[:2],'big'),v[2],(C.c_uint8*4)(*v[3:7]),(C.c_uint8*4)(*v[7:11])),*v[11:])
def dump(b):
 a=b.animation
 return a.cursor.to_bytes(2,'big')+bytes([a.timer])+bytes(a.code)+bytes(a.attr)+bytes(getattr(b,k)for k in('active','state','phase','subtype','variant','owner','hp','timer'))
rows=list(csv.DictReader(trace.open()));assert len(rows)==3072
fixtures=[];body=None;deletions={}
for d in rows:
 before=bytes.fromhex(d['before']);after=bytes.fromhex(d['after']);v=convert(before);expected=convert(after)
 if int(d['tick'])==0:body=load(v)
 else:assert dump(body)==v,('continuity',d)
 lib.arcade_large_death_step(C.byref(body));assert dump(body)==expected,(d,dump(body).hex(),expected.hex())
 allowed={0,1,2,13,14,15,*range(16,24),24,29}
 assert all(before[k]==after[k]for k in range(48)if k not in allowed)
 if before[0]and not after[0]:deletions[d['case']]=int(d['tick']);assert int(d['tick'])==(17 if int(d['exit'])else 45)
 fixtures.append(v.hex()+','+expected.hex())
assert len(deletions)==48
(o/'large-death-target.csv').write_text('\n'.join(fixtures)+'\n')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
program=hashlib.sha256(b''.join((r/'arcade'/n).read_bytes()for n in('ss.01e','ss.02e','ss.03e'))).hexdigest();assert program=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
d={'program_sha256':program,'trace_sha256':sha(trace),'source_sha256':{p.name:sha(p)for p in sources},'target_sha256':sha(o/'large-death-target.csv'),'cases':len(rows),'trajectories':48,'field_bytes_checked':len(rows)*19,'scope':'Continuous native D7B4 vs original D55B with9D=0. Three subtypes,four variants,both owners,kill/exit. Terminal score cursor normalized; position and other record bytes unchanged. No gameplay integration.'}
(o/'large-death-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))

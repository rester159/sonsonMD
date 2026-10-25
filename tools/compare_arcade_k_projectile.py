#!/usr/bin/env python3
"""Continuous kind2 projectile trajectories versus original CBC8 execution."""
from pathlib import Path
import csv,ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit';trace=o/'k-projectile-oracle.csv';(o/'k-projectile-comparison.json').unlink(missing_ok=True)
assert 'K_PROJECTILE_DONE'in(o/'k-projectile-source/mame.log').read_text()
source=r/'src/arcade_k.c';subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(source),'-o',str(o/'k-projectile.dylib')],check=True)
fields=('xf','y','yf','active','state','phase','facing','death_timer','cursor','anim_timer','code','attr')
class Shot(C.Structure):_fields_=[('x',C.c_uint16),('origin',C.c_uint16)]+[(k,C.c_uint8)for k in fields]
lib=C.CDLL(str(o/'k-projectile.dylib'));lib.arcade_k_projectile_step.argtypes=[C.POINTER(Shot),C.c_uint16,C.c_uint8,C.c_uint8]
addresses=[0xcec7,0xceca,0xcecd,0xced0,0xced5,0xced8,0xcedb,0xcede,0xcee1];ids={a:k for k,a in enumerate(addresses)}
def convert(b):return b[5:7]+b[28:30]+bytes(b[k]for k in(7,8,9,0,1,2,10,24))+bytes([ids[int.from_bytes(b[14:16],'big')],b[13],b[16],b[17]])
def load(v):return Shot(int.from_bytes(v[:2],'big'),int.from_bytes(v[2:4],'big'),*v[4:])
def dump(a):return a.x.to_bytes(2,'big')+a.origin.to_bytes(2,'big')+bytes(getattr(a,k)for k in fields)
rows=list(csv.DictReader(trace.open()));assert len(rows)==23040
fixtures=[];deletions=0
for d in rows:
 case=int(d['case']);tick=int(d['tick']);b=bytes.fromhex(d['before']);e=bytes.fromhex(d['after']);before=convert(b);expected=convert(e)
 if tick==0:a=load(before)
 else:
  if case%3==1 and tick==24:a.state=3;a.phase=0
  assert dump(a)==before,('continuity',case,tick)
 lib.arcade_k_projectile_step(C.byref(a),int(d['camera']),int(d['stopped']),int(d['purge']))
 assert dump(a)==expected,(case,tick,dump(a).hex(),expected.hex())
 allowed={0,1,2,5,6,7,8,9,11,12,13,14,15,16,17,24,28,29}
 assert all(b[k]==e[k]for k in range(32)if k not in allowed),(case,tick,'unowned write')
 if b[0]and not e[0]:deletions+=1
 inp=before+int(d['camera']).to_bytes(2,'big')+bytes((int(d['stopped']),int(d['purge'])));fixtures.append(inp.hex()+','+expected.hex())
(o/'k-projectile-target.csv').write_text('\n'.join(fixtures)+'\n')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
program=hashlib.sha256(b''.join((r/'arcade'/n).read_bytes()for n in('ss.01e','ss.02e','ss.03e'))).hexdigest();assert program=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
d={'program_sha256':program,'trace_sha256':sha(trace),'native_sha256':sha(source),'target_sha256':sha(o/'k-projectile-target.csv'),'cases':len(rows),'trajectories':144,'deletions':deletions,'field_bytes_compared':len(rows)*16,'scope':'Continuous CBC8 kind2 projectile: startup,motion,art,offscreen cleanup,impact and purge. Two facings,subpixels,screen edges,camera wrap,scroll modes. Scripted impact at24; not natural collision or gameplay pool integration.'}
(o/'k-projectile-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))

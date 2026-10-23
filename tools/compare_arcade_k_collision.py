#!/usr/bin/env python3
"""Native shared-child scan order and shot cancellation vs original CPU."""
from pathlib import Path
import csv,ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit';trace=o/'k-collision-oracle.csv';(o/'k-collision-comparison.json').unlink(missing_ok=True)
assert 'K_COLLISION_DONE'in(o/'k-collision-source/mame.log').read_text()
source=r/'src/arcade_k_collision.c';subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(source),'-o',str(o/'k-collision.dylib')],check=True)
class Child(C.Structure):_fields_=[('x',C.c_uint16),('origin',C.c_uint16)]+[(n,C.c_uint8)for n in('xf','y','yf','active','state','phase','facing','death_timer','cursor','anim_timer','code','attr')]
class Shot(C.Structure):_fields_=[('x',C.c_uint16),('origin',C.c_uint16)]+[(n,C.c_uint8)for n in('xf','y','yf','active','state','phase','facing','timer','anim_timer','cursor','pose')]
lib=C.CDLL(str(o/'k-collision.dylib'))
for name in('arcade_k_shot_target','arcade_k_contact_target'):fn=getattr(lib,name);fn.argtypes=[C.POINTER(Child),C.c_uint16,C.c_uint8,C.c_uint8];fn.restype=C.c_uint8
lib.arcade_k_cancel.argtypes=[C.POINTER(Child),C.POINTER(Shot),C.c_uint8];lib.arcade_k_cancel.restype=C.c_uint16
rows=list(csv.DictReader(trace.open()));assert len(rows)==8192
fixtures=[];hits=[0,0]
for d in rows:
 op=int(d['operation']);owner=int(d['owner']);b=bytearray.fromhex(d['actor_before']);poolraw=bytearray.fromhex(d['pool_before']);pool=(Child*4)()
 x=int.from_bytes(b[5:7],'big');y=b[8];active=b[0];inp=bytes((op,owner))+b[5:7]+bytes((y,active,b[1],b[2]))
 for k in range(4):
  a=poolraw[k*32:(k+1)*32];pool[k].x=int.from_bytes(a[5:7],'big');pool[k].y=a[8];pool[k].active=a[0];pool[k].state=a[1];pool[k].phase=a[2];inp+=a[5:7]+bytes((a[8],a[0],a[1],a[2]))
 fn=lib.arcade_k_shot_target if op==0 else lib.arcade_k_contact_target
 selected=fn(pool,x,y,active);assert selected==int(d['selected']),d
 event=0
 if selected!=255:
  hits[op]+=1;k=selected*32
  if op==0:
   shot=Shot();shot.active=b[0];shot.state=b[1];shot.phase=b[2];event=lib.arcade_k_cancel(C.byref(pool[selected]),C.byref(shot),owner)
   b[0:3]=bytes((shot.active,shot.state,shot.phase));poolraw[k:k+3]=bytes((pool[selected].active,pool[selected].state,pool[selected].phase))
  else:
   # Source effects recorded for eventual player-life binding; selector only.
   b[1:4]=bytes((0,0,1));poolraw[k+1:k+3]=bytes((3,2))
 assert event==int(d['event'])and b==bytes.fromhex(d['actor_after'])and poolraw==bytes.fromhex(d['pool_after']),d
 # Target output checks selectors plus real cancel routine, not contact adapter.
 out=bytes([selected])+event.to_bytes(2,'big')
 if op==0:out+=bytes((b[0],b[1],b[2]))
 else:
  original=bytes.fromhex(d['actor_before']);out+=original[:3]
 out+=b''.join(bytes((p.active,p.state,p.phase))for p in pool)
 fixtures.append(inp.hex()+','+out.hex())
(o/'k-collision-target.csv').write_text('\n'.join(fixtures)+'\n')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
program=hashlib.sha256(b''.join((r/'arcade'/n).read_bytes()for n in('ss.01e','ss.02e','ss.03e'))).hexdigest();assert program=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
d={'program_sha256':program,'trace_sha256':sha(trace),'native_sha256':sha(source),'target_sha256':sha(o/'k-collision-target.csv'),'cases':len(rows),'shot_hits':hits[0],'contact_hits':hits[1],'record_bytes_compared':len(rows)*160,'scope':'Original EE10 and ED9F: first-hit selection, wrapped bounds, flags, shot cancellation and event. Contact write adapter records source effects; player-life binding/global collision order and bit6 bonus path excluded.'}
(o/'k-collision-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))

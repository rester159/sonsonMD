#!/usr/bin/env python3
"""Original large scans vs native slot selection and accepted-hit composition."""
import compare_arcade_large_actor as a
import csv,ctypes as C,hashlib,json,subprocess
r=a.r;o=a.o;trace=o/'large-scan-oracle.csv';(o/'large-scan-comparison.json').unlink(missing_ok=True)
assert 'LARGE_SCAN_DONE'in(o/'large-scan-source/mame.log').read_text()
source=r/'src/arcade_large_collision.c';difficulty=r/'src/arcade_difficulty.c'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(source),str(difficulty),'-o',str(o/'large-scans.dylib')],check=True)
lib=C.CDLL(str(o/'large-scans.dylib'))
for name in('arcade_large_shot_target','arcade_large_contact_target'):fn=getattr(lib,name);fn.argtypes=[C.POINTER(a.Actor),C.c_uint16,C.c_uint8,C.c_uint8];fn.restype=C.c_uint8
class Shot(C.Structure):_fields_=[('x',C.c_uint16),('origin',C.c_uint16)]+[(k,C.c_uint8)for k in('xf','y','yf','active','state','phase','facing','timer','anim_timer','cursor','pose')]
class Result(C.Structure):_fields_=[('combat',C.c_uint16),('event',C.c_uint16),('points',C.c_uint16),('sound',C.c_uint8)]
lib.arcade_large_hit.argtypes=[C.POINTER(a.Actor),C.POINTER(Shot),C.c_uint8,C.c_uint16,C.POINTER(Result)]
rows=list(csv.DictReader(trace.open()));assert len(rows)==8192
fixtures=[];hits=[0,0]
for d in rows:
 op=int(d['operation']);b=bytearray.fromhex(d['actor_before']);raw=bytearray.fromhex(d['pool_before']);pool=(a.Actor*3)();inp=bytes([op])+b[5:7]+bytes((b[8],b[0]))
 for k in range(3):
  p=raw[k*48:(k+1)*48];q=pool[k];q.x=int.from_bytes(p[5:7],'big');q.y=p[8];q.body.active=p[0];q.body.state=p[1];q.body.phase=p[2];q.body.subtype=p[3];q.body.owner=p[4];q.body.hp=p[24];q.hit=p[25];q.body.variant=p[27]
  inp+=p[5:7]+bytes((p[8],p[0],p[1]))
 fn=lib.arcade_large_shot_target if op==0 else lib.arcade_large_contact_target
 chosen=fn(pool,int.from_bytes(b[5:7],'big'),b[8],b[0]);assert chosen==int(d['selected']),d
 if chosen!=255:
  hits[op]+=1
  if op==0:
   s=Shot();s.active=b[0];s.state=b[1];s.phase=b[2];result=Result();lib.arcade_large_hit(C.byref(pool[chosen]),C.byref(s),int(d['owner']),0,C.byref(result))
   b[:3]=bytes((s.active,s.state,s.phase));k=chosen*48;q=pool[chosen];raw[k:k+3]=bytes((q.body.active,q.body.state,q.body.phase));raw[k+4]=q.body.owner;raw[k+24]=q.body.hp;raw[k+25]=q.hit;assert result.event==0 and result.sound==7
  else:b[1:4]=bytes((0,0,1)) # Source effects recorded for future player-life binding.
 assert b==bytes.fromhex(d['actor_after'])and raw==bytes.fromhex(d['pool_after']),d
 fixtures.append(inp.hex()+','+bytes([chosen]).hex())
(o/'large-scan-target.csv').write_text('\n'.join(fixtures)+'\n')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
d={'program_sha256':a.program,'trace_sha256':sha(trace),'native_sha256':sha(source),'difficulty_sha256':sha(difficulty),'target_sha256':sha(o/'large-scan-target.csv'),'cases':len(rows),'shot_hits':hits[0],'contact_hits':hits[1],'record_bytes_compared':len(rows)*176,'scope':'EE71/EF09 three-slot order, flags, wrapped/asymmetric geometry. Host composes accepted nonlethal hits; contact effects recorded by adapter. Target tests selector; global page/life integration separate.'}
(o/'large-scan-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))

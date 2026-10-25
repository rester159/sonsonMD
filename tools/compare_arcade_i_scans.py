#!/usr/bin/env python3
"""Compare family/shot/actor priority with original class-I scanner calls."""
import ctypes as C,hashlib,json,pathlib,subprocess
r=pathlib.Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_collision.c'),str(r/'src/arcade_i_combat.c'),str(r/'src/arcade_difficulty.c'),'-o',str(o/'arcade_i_scan.dylib')],check=True)
class Actor(C.Structure):_fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ['xf','y','yf','active','state','phase','delay','facing']]
class Foe(C.Structure):_fields_=[('x_speed',C.c_uint32),('y_speed',C.c_uint32),('body',Actor)]+[(n,C.c_uint8)for n in ['cursor','code','attr','anim_timer','hop_timer','hit_style']]
lib=C.CDLL(str(o/'arcade_i_scan.dylib'));fn=lib.arcade_i_shot_target;fn.argtypes=[C.POINTER(Foe),C.c_uint16,C.c_uint8];fn.restype=C.c_uint8
program=b''.join((r/'arcade'/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e'])
program_hash=hashlib.sha256(program).hexdigest()
assert program_hash=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
trace=o/'i-scan-oracle.jsonl';rows=[json.loads(x)for x in trace.read_text().splitlines()];assert len(rows)==1024,len(rows)
hits=0;calls=0;bound=[]
for row in rows:
 foes=(Foe*4)();raw=bytes.fromhex(row['actors']);shots=bytearray.fromhex(row['shots'])
 for k,a in enumerate(foes):
  b=raw[k*32:(k+1)*32];a.body.x=int.from_bytes(b[5:7],'big');a.body.y=b[8];a.body.active=b[0];a.body.state=b[1]
 got=[]
 for family in [0]:
  for shot in range(2):
   b=shots[shot*32:(shot+1)*32]
   if b[0]!=1:continue
   before=bytes(foes).hex();sx=int.from_bytes(b[5:7],'big');sy=b[8]
   slot=fn(foes,sx,sy);calls+=1
   source_hit=[v for v in row['hits']if (v-1)%2==shot]
   expected=((source_hit[0]-1)//2) if source_hit else 255
   assert slot==expected,(row,slot,expected)
   bound.append({'foes':before,'family':family,'x':sx,'y':sy,'slot':expected})
   if slot!=255:
    index=slot;got.append(index*2+shot+1);foes[index].body.active=2;shots[shot*32]=2
 assert got==row['hits'],{'case':row['case'],'got':got,'expected':row['hits']};hits+=len(got)
target=o/'i-scan-target.csv'
with target.open('w')as f:
 f.write('foes,family,x,y,slot\n')
 for row in bound:
  raw=bytearray.fromhex(row['foes'])
  for k in range(4):raw[k*24+8:k*24+10]=raw[k*24+8:k*24+10][::-1]
  f.write(f"{raw.hex()},{row['family']},{row['x']},{row['y']},{row['slot']}\n")
report={'program_sha256':program_hash,'target_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'cases':len(rows),'selector_calls':calls,'hits':hits,'differences':0,'scope':'original class-I scanner with two fixed shot slots; other families/page gating are separate'}
(o/'i-scan-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

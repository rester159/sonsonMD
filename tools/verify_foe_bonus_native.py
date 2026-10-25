#!/usr/bin/env python3
"""Native shared bonus lifecycle against original dispatcher snapshots."""
from pathlib import Path
import ctypes as C,json,subprocess,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/foe-bonus-native';o.mkdir(exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_foe_bonus.c'),str(r/'src/arcade_bonus_art.c'),'-o',str(o/'test.dylib')],check=True)
class Body(C.Structure):_fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ['xf','y','yf','active','state','phase','delay','facing']]
class Foe(C.Structure):_fields_=[('body',Body),('cursor',C.c_uint16),('pose',C.c_uint16)]+[(n,C.c_uint8)for n in ['kind','anim_timer','stamp','target','hit_style','death_timer']]
lib=C.CDLL(str(o/'test.dylib'));lib.arcade_foe_bonus_step.argtypes=[C.POINTER(Foe),C.c_uint16,C.c_uint8,C.POINTER(C.c_uint8),C.c_uint8]
data=json.loads((r/'out/parity-audit/bonus-art-map.json').read_text());frames=data['frames'];ids={v['address']:i for i,v in enumerate(frames)}
rows=[json.loads(x)for x in (r/'out/parity-audit/foe-bonus-oracle.jsonl').read_text().splitlines()];checks=0
for row in rows:
 b=bytes.fromhex(row['before']);ex=bytes.fromhex(row['after'])
 a=Foe();a.body=Body(int.from_bytes(b[5:7],'big'),b[7],b[8],b[9],b[0],b[1],b[2],b[29],b[10]);a.cursor=ids[int.from_bytes(b[14:16],'big')];a.kind=b[3];a.anim_timer=b[13];a.hit_style=b[4];n=C.c_uint8(row['index'])
 lib.arcade_foe_bonus_step(C.byref(a),0x2345,0,C.byref(n),10)
 got=[a.body.active,a.body.state,a.body.phase,a.kind,a.hit_style,a.body.x,a.body.xf,a.body.y,a.body.yf,a.body.delay,a.body.facing,a.anim_timer,frames[a.cursor]['address'],n.value]
 expected=[ex[0],ex[1],ex[2],ex[3],ex[4],int.from_bytes(ex[5:7],'big'),ex[7],ex[8],ex[9],ex[29],ex[10],ex[13],int.from_bytes(ex[14:16],'big'),row['next_index']]
 assert got==expected,row;checks+=len(got)
 if row['phase']in(0,2):
  pose=frames[a.pose&0x3fff];assert [pose['code'],0xc3 if a.pose&0x4000 else pose['attr']]==list(ex[16:18]),row;checks+=2
assert len(rows)==1408
result={'cases':len(rows),'checks':checks,'source_sha256':hashlib.sha256((r/'src/arcade_foe_bonus.c').read_bytes()).hexdigest(),'scope':'Host native shared FF lifecycle vs source snapshots; valid captured phases, no live powerup trigger claim'}
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

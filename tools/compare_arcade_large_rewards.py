#!/usr/bin/env python3
"""Native large hit effects versus original hit and BCD score consumption."""
import compare_arcade_large_actor as a
import csv,ctypes as C,hashlib,json,subprocess
r=a.r;o=a.o;trace=o/'large-reward-oracle.csv';(o/'large-reward-comparison.json').unlink(missing_ok=True)
assert 'LARGE_REWARD_DONE'in(o/'large-reward-source/mame.log').read_text()
source=r/'src/arcade_large_collision.c';difficulty=r/'src/arcade_difficulty.c'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(source),str(difficulty),'-o',str(o/'large-rewards.dylib')],check=True)
class Shot(C.Structure):_fields_=[('x',C.c_uint16),('origin',C.c_uint16)]+[(k,C.c_uint8)for k in('xf','y','yf','active','state','phase','facing','timer','anim_timer','cursor','pose')]
class Result(C.Structure):_fields_=[('combat',C.c_uint16),('event',C.c_uint16),('points',C.c_uint16),('sound',C.c_uint8)]
lib=C.CDLL(str(o/'large-rewards.dylib'));lib.arcade_large_hit.argtypes=[C.POINTER(a.Actor),C.POINTER(Shot),C.c_uint8,C.c_uint16,C.POINTER(Result)];lib.arcade_large_shot_overlap.argtypes=[C.c_uint16,C.c_uint8,C.c_uint16,C.c_uint8];lib.arcade_large_shot_overlap.restype=C.c_uint8
rows=[{k:int(v)for k,v in d.items()}for d in csv.DictReader(trace.open())];assert len(rows)==768
fixtures=[]
for d in rows:
 actor=a.Actor();actor.body.subtype=d['subtype'];actor.body.variant=d['variant'];actor.body.owner=1-d['owner'];actor.body.hp=d['hp'];actor.body.active=1;actor.body.state=1;actor.body.phase=1
 shot=Shot();shot.active=1;shot.state=1;shot.phase=1;out=Result()
 lib.arcade_large_hit(C.byref(actor),C.byref(shot),d['owner'],d['combat'],C.byref(out))
 got=bytes((actor.body.hp,actor.body.active,actor.body.state,actor.body.phase,actor.body.owner,actor.hit,shot.active,shot.state,shot.phase))+out.event.to_bytes(2,'big')+bytes([out.sound])+out.combat.to_bytes(2,'big')+out.points.to_bytes(2,'big')
 expected=bytes(d[k]for k in('after_hp','active','state','phase','credit','flash','shot_active','shot_state','shot_phase'))+d['event'].to_bytes(2,'big')+bytes([d['sound']])+d['after_combat'].to_bytes(2,'big')+d['points'].to_bytes(2,'big')
 assert got==expected,d
 inp=bytes(d[k]for k in('subtype','variant','owner','hp'))+d['combat'].to_bytes(2,'big');fixtures.append(inp.hex()+','+expected.hex())
geometry=o/'large-damage-oracle.csv';checks=0
assert 'LARGE_DAMAGE_DONE'in(o/'large-damage-source/mame.log').read_text()
for row in csv.DictReader(geometry.open()):
 d={k:int(v)for k,v in row.items()}
 if d['kind']!=1:continue
 assert lib.arcade_large_shot_overlap((100+d['dx'])&65535,(128+d['dy'])&255,100,128)==d['carry'],d;checks+=1
assert checks==4608
(o/'large-reward-target.csv').write_text('\n'.join(fixtures)+'\n')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
d={'program_sha256':a.program,'trace_sha256':sha(trace),'native_sha256':sha(source),'difficulty_sha256':sha(difficulty),'target_sha256':sha(o/'large-reward-target.csv'),'cases':len(rows),'geometry_cases':checks,'geometry_trace_sha256':sha(geometry),'scope':'Accepted-hit HP,shot consumption,owner,sound,score event,BCD points and combat difficulty. All3 subtypes/4variants/2owners;HP0/1/2/7;8 difficulty edges. Full scan order/contact/render integration separate.'}
(o/'large-reward-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))

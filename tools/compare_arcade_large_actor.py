#!/usr/bin/env python3
"""Continuous complete native large actor vs original D55B trajectories."""
from pathlib import Path
import csv,ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit';trace=o/'large-motion-oracle.csv';(o/'large-actor-comparison.json').unlink(missing_ok=True)
assert 'LARGE_MOTION_DONE'in(o/'large-motion-source/mame.log').read_text()
sources=[r/'src'/n for n in('arcade_large_actor.c','arcade_large.c','arcade_large_fire.c','arcade_large_death.c','arcade_large_animation.c','arcade_large_animation_data.c','arcade_large_motion_data.c')]
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),*map(str,sources),'-o',str(o/'large-actor.dylib')],check=True)
class Animation(C.Structure):_fields_=[('cursor',C.c_uint16),('timer',C.c_uint8),('code',C.c_uint8*4),('attr',C.c_uint8*4)]
body_fields=('active','state','phase','subtype','variant','owner','hp','timer')
class Body(C.Structure):_fields_=[('animation',Animation)]+[(k,C.c_uint8)for k in body_fields]
actor_fields=('xf','y','yf','direction','facing','hit','lifetime','fire_timer','fire_period','target','retreat')
class Actor(C.Structure):_fields_=[('body',Body),('x',C.c_uint16)]+[(k,C.c_uint8)for k in actor_fields]
class Player(C.Structure):_fields_=[('x',C.c_uint16),('y',C.c_uint8),('active',C.c_uint8),('kind',C.c_uint8)]
class World(C.Structure):_fields_=[('camera',C.c_uint16),('clock',C.c_uint8),('stage',C.c_uint8),('tracking',C.c_uint8),('stopped',C.c_uint8),('budget',C.c_uint8),('purge',C.c_uint8),('players',Player*2)]
class Birth(C.Structure):_fields_=[('x',C.c_uint16),('y',C.c_uint8),('facing',C.c_uint8),('slot',C.c_uint8)]
lib=C.CDLL(str(o/'large-actor.dylib'));lib.arcade_large_step.argtypes=[C.POINTER(Actor),C.POINTER(World),C.c_uint8,C.POINTER(C.c_uint16),C.POINTER(C.c_uint8),C.POINTER(C.c_uint8),C.POINTER(Birth)];lib.arcade_large_step.restype=C.c_uint8
meta=json.loads((o/'large-animation-map.json').read_text());ids={a:k for k,a in enumerate(meta['addresses'])}
def convert(b):
 ptr=int.from_bytes(b[14:16],'big');assert ptr==0 or ptr in ids,hex(ptr)
 target=int.from_bytes(b[30:32],'big');assert target in(0,0x490,0x4c0)
 return (ids.get(ptr,0).to_bytes(2,'big')+b[13:14]+b[16:24]+bytes(b[k]for k in(0,1,2,3,27,4,24,29))+b[5:7]+bytes(b[k]for k in(7,8,9,10,26,25,28,32,33))+bytes((0 if target==0 else 1 if target==0x490 else 2,int(int.from_bytes(b[11:13],'big')in(0xdba2,0xdbbb,0xdbd4)))))
def load(v):return Actor(Body(Animation(int.from_bytes(v[:2],'big'),v[2],(C.c_uint8*4)(*v[3:7]),(C.c_uint8*4)(*v[7:11])),*v[11:19]),int.from_bytes(v[19:21],'big'),*v[21:])
def dump(a):
 b=a.body;n=b.animation
 return n.cursor.to_bytes(2,'big')+bytes([n.timer])+bytes(n.code)+bytes(n.attr)+bytes(getattr(b,k)for k in body_fields)+a.x.to_bytes(2,'big')+bytes(getattr(a,k)for k in actor_fields)
rows=list(csv.DictReader(trace.open()));assert len(rows)==36864
actor=None;fixtures=[];birth_count=0;seed=C.c_uint16(0);sample=C.c_uint8(0)
for row in rows:
 d={k:(v if k in('before','after','children','rng')else int(v))for k,v in row.items()}
 before=convert(bytes.fromhex(d['before']));expected=convert(bytes.fromhex(d['after']))
 if d['tick']==0:actor=load(before);seed.value=sample.value=0
 else:assert dump(actor)==before,('continuity',d['case'],d['tick'])
 w=World();w.camera=d['camera'];w.clock=d['clock'];w.tracking=d['players']==2;w.stopped=d['scroll'];w.players[0]=Player(d['p1x'],d['p1y'],1,0);w.players[1]=Player(d['p2x'],d['p2y'],int(d['players']==2),0)
 birth=Birth(0x1234,0x56,0x78,0x9a);occupied=(C.c_uint8*4)(0,0,0,0)
 # Canonical fixture input: actor32,camera2,clock,tracking,stopped,difficulty,
 # P1 x2/y/active/kind, P2 same, RNG2/sample, four occupied flags.
 inp=before+w.camera.to_bytes(2,'big')+bytes((w.clock,w.tracking,w.stopped,d['difficulty']))
 for p in w.players:inp+=p.x.to_bytes(2,'big')+bytes((p.y,p.active,p.kind))
 inp+=seed.value.to_bytes(2,'big')+bytes((sample.value,0,0,0,0))
 fired=lib.arcade_large_step(C.byref(actor),C.byref(w),d['difficulty'],C.byref(seed),C.byref(sample),occupied,C.byref(birth))
 got=dump(actor);assert got==expected,(d['case'],d['tick'],[(k,x,y)for k,(x,y)in enumerate(zip(got,expected))if x!=y])
 children=bytes.fromhex(d['children']);slots=[k for k in range(4)if children[k*32]];assert len(slots)<=1
 if slots:
  k=slots[0];rec=children[k*32:(k+1)*32];event=bytes([1])+rec[5:7]+bytes((rec[8],rec[10],k));birth_count+=1
 else:event=bytes((0,0x12,0x34,0x56,0x78,0x9a))
 assert bytes([fired])+birth.x.to_bytes(2,'big')+bytes((birth.y,birth.facing,birth.slot))==event,(d['case'],d['tick'],'birth')
 random=bytes.fromhex(d['rng'])[:3];assert seed.value.to_bytes(2,'big')+bytes([sample.value])==random
 fixtures.append(inp.hex()+','+(expected+event+random).hex())
(o/'large-actor-target.csv').write_text('\n'.join(fixtures)+'\n')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
program=hashlib.sha256(b''.join((r/'arcade'/n).read_bytes()for n in('ss.01e','ss.02e','ss.03e'))).hexdigest();assert program=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
d={'program_sha256':program,'trace_sha256':sha(trace),'source_sha256':{p.name:sha(p)for p in sources},'target_sha256':sha(o/'large-actor-target.csv'),'cases':len(rows),'trajectories':48,'semantic_bytes_checked':len(rows)*41,'births':birth_count,'actor_size_host':C.sizeof(Actor),'scope':'Continuous full ordinary actor: source D55B positions, states, four-cell art, timers, target and projectile births. Scripted camera/players and child slots cleared each tick. No low-HP collision injection in these trajectories; component contracts separate. Not gameplay integration.'}
(o/'large-actor-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))

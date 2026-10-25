#!/usr/bin/env python3
"""Source-pinned title speeds and differential gameplay regression; no frontend."""
from pathlib import Path
import ctypes as C
import hashlib,json,random,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/prelude-motion';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert rom[0x540a:0x541a]==bytes.fromhex('01000200000002c00100000000000240')
class Player(C.Structure):
 _fields_=[('x',C.c_uint16)]+[(n,C.c_uint8) for n in ('xf','y','yf','state','phase','timer','stamp','facing','previous','active','attached')]
class Anim(C.Structure):_fields_=[(n,C.c_uint8)for n in ('owner','cursor','timer','code','attr')]
class Fire(C.Structure):_fields_=[('x',C.c_uint16),('y',C.c_uint8),('facing',C.c_uint8)]
Floor=C.CFUNCTYPE(C.c_int16,C.c_uint16,C.c_uint8,C.c_void_p)
@Floor
def floor(x,y,ctx):return 1 if y%32>=24 else -32767
libs=[]
baseline=r/'out/parity-audit/postgame-runtime/build/src/arcade_player.c'
for name,src in [('before',baseline),('after',r/'src/arcade_player.c')]:
 subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(src),'-o',str(o/(name+'.dylib'))],check=True)
 lib=C.CDLL(str(o/(name+'.dylib')))
 lib.arcade_player_step_stopped.argtypes=[C.POINTER(Player),C.POINTER(Player),C.c_uint8,C.c_uint16,C.c_uint8,Floor,C.c_void_p,C.POINTER(Fire),C.POINTER(Anim)]
 libs.append(lib)
new=libs[1];new.arcade_player_step_prelude.argtypes=[C.POINTER(Player),C.POINTER(Player),C.c_uint8,Floor,C.c_void_p,C.POINTER(Fire),C.POINTER(Anim)]
checks=0
for xf in range(256):
 for button,dx in [(0,0),(4,-1),(8,1)]:
  p=Player(x=100,xf=xf,y=136,state=1,active=1,facing=1);a=Anim(owner=0);f=Fire()
  new.arcade_player_step_prelude(C.byref(p),None,button,floor,None,C.byref(f),C.byref(a))
  assert p.x==100+dx and p.xf==xf,(xf,button,p.x,p.xf)
  checks+=1
rng=random.Random(94010)
for case in range(20000):
 p=Player(x=rng.randrange(65536),**{n:rng.randrange(256) for n,_ in Player._fields_[1:]})
 p.state=rng.randrange(6);p.phase=rng.randrange(4);p.active=rng.choice([0,1,255]);p.facing=rng.choice([1,3])
 q=Player.from_buffer_copy(p);q.x=(p.x+rng.randrange(-24,25))&65535
 a=Anim(rng.randrange(2),rng.randrange(4),rng.randrange(8),0,0)
 button=rng.randrange(64);camera=rng.randrange(65536);stopped=rng.randrange(2);outputs=[]
 for lib in libs:
  pp=Player.from_buffer_copy(p);qq=Player.from_buffer_copy(q);aa=Anim.from_buffer_copy(a);ff=Fire()
  lib.arcade_player_step_stopped(C.byref(pp),C.byref(qq),button,camera,stopped,floor,None,C.byref(ff),C.byref(aa))
  outputs.append((bytes(pp),bytes(qq),bytes(aa),bytes(ff)))
 assert outputs[0]==outputs[1],case
report={'passed':True,'prelude_speed_cases':checks,'gameplay_regression_cases':20000,
 'baseline_sha256':hashlib.sha256(baseline.read_bytes()).hexdigest(),
 'native_sha256':hashlib.sha256((r/'src/arcade_player.c').read_bytes()).hexdigest(),
 'scope':'ROM-pinned 940A horizontal magnitudes, all fraction bytes/no idle drift; gameplay differential against archived implementation across 20000 deterministic inputs/states. Not original-CPU prelude actor comparison or live integration.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

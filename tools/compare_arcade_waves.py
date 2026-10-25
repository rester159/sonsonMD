#!/usr/bin/env python3
"""Compare typed native wave/RNG contracts with unmodified original CPU calls."""
import ctypes as C,hashlib,json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1];out=root/'out/parity-audit'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(root/'inc'),str(root/'src/arcade_waves.c'),str(root/'src/arcade_formations.c'),'-o',str(out/'arcade_waves.dylib')],check=True)
class Control(C.Structure):
 _fields_=[('auxiliary',C.c_uint16)]+[(n,C.c_uint8) for n in ['active','state','phase','wait','parameter']]
class Waves(C.Structure):
 _fields_=[('mask',C.c_uint16),('placement',C.c_uint16)]+[(n,C.c_uint8)for n in ['cached_coarse','state','cursor','remaining','delay']]+[('controls',Control*12)]
class Rng(C.Structure):_fields_=[('seed',C.c_uint16),('last',C.c_uint8)]
class Birth(C.Structure):_fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ['y','delay','facing']]
lib=C.CDLL(str(out/'arcade_waves.dylib'))
lib.arcade_rng_next.argtypes=[C.POINTER(Rng)];lib.arcade_rng_next.restype=C.c_uint8
lib.arcade_waves_reset.argtypes=[C.POINTER(Waves),C.c_uint8]
lib.arcade_waves_step.argtypes=[C.POINTER(Waves),C.c_uint8,C.c_uint8];lib.arcade_waves_step.restype=C.c_int16
# family,live,clock,stage are four bytes, followed by two words.
lib.arcade_formation_step.argtypes=[C.POINTER(Control)]+[C.c_uint8]*4+[C.c_uint16]*2+[C.POINTER(Rng),C.POINTER(Birth),C.POINTER(C.c_uint8)]
lib.arcade_formation_step.restype=C.c_uint8
checks=0;counts={};births=0;last=0
trace=out/'wave-oracle.jsonl';data=trace.read_bytes()
def check(actual,expected,field,row):
 global checks
 checks+=1
 assert actual==expected,{'field':field,'actual':actual,'expected':expected,'source_row':row}
def compare_control(c,b,row):
 for name,expected in [('active',b[0]),('wait',b[1]),('state',b[2]),('phase',b[3]),('auxiliary',int.from_bytes(b[10:12],'big')),('parameter',b[14])]:check(getattr(c,name),expected,name,row)
for line in data.splitlines():
 row=json.loads(line);kind=row['kind'];counts[kind]=counts.get(kind,0)+1
 if kind=='rng':
  rng=Rng(row['seed'],0);result=lib.arcade_rng_next(C.byref(rng))
  check(rng.seed,row['next'],'rng_seed',row);check(result,row['last'],'rng_result',row);check(rng.last,row['last'],'rng_last',row);last=row['last']
 elif kind=='wave':
  if row['tick']==0:
   w=Waves();lib.arcade_waves_reset(C.byref(w),255)
   for k,c in enumerate(w.controls):c.active=k%2;c.wait=7;c.state=2;c.phase=3;c.auxiliary=0x1234;c.parameter=0xa5
  lib.arcade_waves_step(C.byref(w),row['wave'],0)
  b=bytes.fromhex(row['producer']);ctrl=bytes.fromhex(row['controls'])
  for name,expected in [('cached_coarse',b[0]),('state',b[2]),('mask',int.from_bytes(b[6:8],'big')),('remaining',b[8])]:check(getattr(w,name),expected,name,row)
  if row['tick']>0:check(w.cursor,(int.from_bytes(b[10:12],'big')-0x638)//16,'cursor',row)
  for k,c in enumerate(w.controls):compare_control(c,ctrl[k*16:(k+1)*16],row)
 elif kind=='delay':
  w=Waves();lib.arcade_waves_reset(C.byref(w),0);w.delay=row['delay'];lib.arcade_waves_step(C.byref(w),0,row['clock']);check(w.delay,row['after'],'delay',row)
 else:
  sp=row['statephase'];c=Control(0x1234,1,1 if sp==3 else 0,0 if sp==3 else sp,1,row['parameter']);rng=Rng(row['seed'],last);born=(Birth*6)();reservation=C.c_uint8(9)
  count=lib.arcade_formation_step(C.byref(c),row['family'],row['live'],row['clock'],row['group']*16,0x2345,0x23f5,C.byref(rng),born,C.byref(reservation))
  compare_control(c,bytes.fromhex(row['control']),row)
  check(reservation.value,row['reservation'],'reservation',row);check(rng.seed,row['next'],'seed',row);check(rng.last,row['last'],'last',row);last=row['last']
  actors=bytes.fromhex(row['actors']);expected_count=6 if actors[0] else 0;check(count,expected_count,'birth_count',row)
  for k in range(count):
   a=actors[k*32:(k+1)*32];b=born[k];births+=1
   for name,expected in [('x',int.from_bytes(a[5:7],'big')),('y',a[8]),('delay',a[29]),('facing',a[10])]:check(getattr(b,name),expected,name,row)
assert counts=={'rng':65536,'wave':1131,'delay':15,'formation':7680},counts
result={'source':'Unmodified original SonSon MC6809 routines invoked in a RAM JSR fixture; local contracts, not natural reachability','trace_sha256':hashlib.sha256(data).hexdigest(),'source_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()for p in [root/'src/arcade_waves.c',root/'src/arcade_formations.c',root/'inc/ss_wave_sched.h']},'cases':counts,'formation_births_compared':births,'field_checks':checks,'differences':0}
(out/'wave-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

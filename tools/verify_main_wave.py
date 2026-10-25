#!/usr/bin/env python3
"""Offline EFED controller contract, using original ROM placements as oracle."""
from pathlib import Path
import ctypes as C,subprocess,json,hashlib,random
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/main-wave';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
for addr,hexbytes in [(0xf000,'d6 a6 26 03 6c 02 39 cc 01 01 ed 02 39'),(0xf019,'d6 a4 c1 0c 25 45'),(0xf080,'d6 a4 27 44 96 a6 81 10 27 3e')]:
 assert rom[addr-0x4000:addr-0x4000+len(bytes.fromhex(hexbytes))]==bytes.fromhex(hexbytes)
sources=('arcade_main_wave.c','arcade_main_wave_data.c','arcade_waves.c','arcade_formations.c')
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),*[str(r/'src'/n)for n in sources],'-o',str(o/'test.dylib')],check=True)
class Control(C.Structure):_fields_=[('auxiliary',C.c_uint16)]+[(n,C.c_uint8)for n in ('active','state','phase','wait','parameter')]
class Wave(C.Structure):_fields_=[('control',Control)]+[(n,C.c_uint8)for n in ('sequence','cursor','remaining')]
class Rng(C.Structure):_fields_=[('seed',C.c_uint16),('last',C.c_uint8)]
class Birth(C.Structure):_fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ('slot','y','variant','delay')]
lib=C.CDLL(str(o/'test.dylib'));lib.arcade_main_wave_step.argtypes=[C.POINTER(Wave)]+[C.c_uint8]*5+[C.c_uint16,C.POINTER(C.c_uint8),C.POINTER(Rng),C.POINTER(Birth)];lib.arcade_main_wave_step.restype=C.c_uint8
rand=random.Random(0xEFED);checks=births=0
for case in range(4096):
 w=Wave(Control(0x1234,1,case%2,case%3,77,99),case%16,0,12);rng=Rng(case*13,17)
 state,phase,sequence,cursor,remaining=w.control.state,w.control.phase,w.sequence,0,12;seed,last=rng.seed,rng.last
 for tick in range(64):
  active=0 if tick%17==0 else 1;w.control.active=active
  live=rand.choice((0,1,12,15,16,17));ready=rand.choice((0,1,11,12,24,255));vsync=rand.randrange(256);clock=rand.randrange(256);difficulty=rand.randrange(256);camera=rand.randrange(65536)
  occupied=[rand.choice((0,1,2,128))for _ in range(16)];expected=[]
  def emit(slot,seq,pos):
   global seed,last
   ptr=int.from_bytes(rom[0xf0c9-0x4000+seq*2:0xf0cb-0x4000+seq*2],'big')
   y,variant=rom[ptr-0x4000+pos*2:ptr-0x4000+pos*2+2]
   feedback=(~((seed&255)^(seed>>8))>>6)&1;seed=((seed<<1)|feedback)&65535;last=seed&255
   expected.append(((camera+248)&65535,slot,y,variant,last&7))
  if active:
   if state==0:
    if live:state,phase=1,1
    else:state=1
   elif state==1:
    if phase==0 and ready>=12:
     seq=((difficulty&48)>>2)+((vsync&6)>>1)
     for k in range(12):emit(k,seq,k)
     phase=1
    elif phase==1:
     sequence=((difficulty&48)>>2)+(((vsync^clock)&6)>>1);cursor=0;remaining=12;phase=2
    elif phase==2 and ready and live!=16 and 0 in occupied:
     emit(occupied.index(0),sequence,cursor);cursor+=1;remaining-=1
     if not remaining:phase=1
  output=(Birth*12)();n=lib.arcade_main_wave_step(C.byref(w),live,ready,vsync,clock,difficulty,camera,(C.c_uint8*16)(*occupied),C.byref(rng),output)
  actual=[tuple(getattr(output[k],name)for name in ('x','slot','y','variant','delay'))for k in range(n)]
  assert actual==expected,(case,tick,actual,expected)
  assert (w.control.state,w.control.phase,w.sequence,w.cursor,w.remaining,rng.seed,rng.last)==(state,phase,sequence,cursor,remaining,seed,last)
  assert (w.control.auxiliary,w.control.wait,w.control.parameter)==(0x1234,77,99)
  checks+=1;births+=n
report={'updates':checks,'births':births,'scope':'Offline source-derived EFED controller model with independently read ROM placements; guards key branch bytes, RNG calls, occupancy gates and field retention. Not an original execution trace or live game binding.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

#!/usr/bin/env python3
"""Check results decoration timing and exact baked cells without an emulator."""
from pathlib import Path
import ctypes as C
import json,subprocess
import gen_assets as G
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/results-decor'
o.mkdir(parents=True,exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_results_decor.c'),str(r/'src/arcade_results_decor_data.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'))
class State(C.Structure):
 _fields_=[(n,C.c_uint8)for n in ('next','timer','pose')]
class Pose(C.Structure):
 _fields_=[('tile',C.c_uint16)]+[(n,C.c_uint8)for n in ('code','attr','timer')]
class Position(C.Structure):
 _fields_=[('x',C.c_uint16),('y',C.c_uint8)]
for fn in ('begin','step'):
 getattr(lib,'arcade_results_decor_'+fn).argtypes=[C.POINTER(State),C.c_uint8]
poses=(Pose*6).in_dll(lib,'arcade_results_poses')
pixels=(C.c_uint32*(24*8)).in_dll(lib,'arcade_results_decor_pixels')
positions=(Position*3).in_dll(lib,'arcade_results_positions')
rom=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
g=G.Roms(str(r/'arcade'));checks=0
for index in range(3):
 descriptor=rom[0x5962+index*5:0x5967+index*5]
 root=int.from_bytes(descriptor[:2],'big')
 assert (positions[index].x,positions[index].y)==(int.from_bytes(descriptor[2:4],'big'),descriptor[4])
 for frame in range(2):
  p=poses[index*2+frame];a=root-0x4000+frame*3
  code,attr,timer=rom[a:a+3]
  assert (p.code,p.attr,p.timer)==(code,attr,timer&127)
  expected=G.bake_sprite_tiles(g,code+((attr&32)<<3),attr&31)
  assert list(pixels)[p.tile*8:p.tile*8+32]==list(expected)
 s=State(255,255,255);lib.arcade_results_decor_begin(C.byref(s),index)
 # Source9617 reads a frame on zero, otherwise only decrements. Thus the
 # extracted10/16 timers produce11/17-update spans, a28-update cycle.
 for tick in range(1024):
  phase=tick%28
  expected=(1,10-phase,0)if phase<11 else (0,27-phase,1)
  assert (s.next,s.timer,s.pose)==expected,(index,tick,bytes(s))
  lib.arcade_results_decor_step(C.byref(s),index);checks+=1
 lib.arcade_results_decor_begin(C.byref(s),index)
 assert bytes(s)==bytes((1,10,0))
report={'animation_updates':checks,'exact_sprite_cells':6,'positions':3,
 'scope':'Offline decoration data and timing checks. Original-player animation retention and live SAT/VDP integration remain unverified.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

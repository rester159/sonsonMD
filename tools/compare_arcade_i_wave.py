#!/usr/bin/env python3
"""Compare typed class-I controller to original F97A CPU observations."""
import csv, ctypes as C, hashlib, json
from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
class Control(C.Structure):
 _fields_=[('auxiliary',C.c_uint16)]+[(n,C.c_uint8)for n in ('active','state','phase','wait','parameter')]
class Wave(C.Structure):
 _fields_=[('control',Control),('cursor',C.c_uint8),('remaining',C.c_uint8)]
class Birth(C.Structure):
 _fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ('y','delay','facing')]
assert C.sizeof(Wave)==10 and C.sizeof(Birth)==6
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_i.c'),'-o',str(o/'arcade_i.dylib')],check=True)
fn=C.CDLL(str(o/'arcade_i.dylib')).arcade_i_wave_step
fn.argtypes=[C.POINTER(Wave),C.c_uint8,C.c_uint8,C.c_uint16,C.POINTER(C.c_uint8),C.POINTER(Birth)];fn.restype=C.c_uint8
program=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
sha=lambda b:hashlib.sha256(b).hexdigest()
assert sha(program)=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
trace=o/'i-wave-oracle.csv';rows=list(csv.DictReader(trace.open()));assert len(rows)==8640
checks=births=0;target=o/'i-wave-target.csv'
with target.open('w')as f:
 f.write('wave,live,clock,camera,reservation,after_wave,after_reservation,born,birth\n')
 for line in rows:
  d={k:int(v)for k,v in line.items()}
  w=Wave(Control(0xabcd,*[d[k]for k in ('active','state','phase','wait','parameter')]),d['cursor'],d['remaining'])
  # Canonical target bytes omit the control padding at offset 7.
  before=bytearray(bytes(w));before[:2]=before[:2][::-1];before[7]=0
  reserve=C.c_uint8(d['reservation']);b=Birth(0x5678,0xab,0xcd,0xef)
  born=fn(C.byref(w),d['live'],d['clock'],d['camera'],C.byref(reserve),C.byref(b))
  expected=bytearray(before)
  for offset,key in ((3,'after_state'),(4,'after_phase'),(5,'after_wait'),(8,'after_cursor'),(9,'after_remaining')):expected[offset]=d[key]
  actual=bytearray(bytes(w));actual[:2]=actual[:2][::-1];actual[7]=0
  assert actual==expected,(d,actual.hex(),expected.hex())
  assert reserve.value==d['after_reservation'],d
  assert born==int(d['birth']>=0),d
  wanted=bytes.fromhex('5678abcdef')
  if born:
   assert d['birth']==d['cursor'],d
   wanted=d['birth_x'].to_bytes(2,'big')+bytes([d['birth_y'],0,d['birth_facing']]);births+=1
  actual_birth=b.x.to_bytes(2,'big')+bytes([b.y,b.delay,b.facing])
  assert actual_birth==wanted,(d,actual_birth,wanted)
  checks+=16 # 9 meaningful controller bytes + reservation + return + 5 birth bytes
  f.write(f'{before.hex()},{d["live"]},{d["clock"]},{d["camera"]},{d["reservation"]},{expected.hex()},{d["after_reservation"]},{born},{wanted.hex()}\n')
report={'scope':'isolated F97A class-I controller; not integrated into gameplay','program_sha256':sha(program),'trace_sha256':sha(trace.read_bytes()),'target_sha256':sha(target.read_bytes()),'native_sha256':sha((r/'src/arcade_i.c').read_bytes()),'cases':len(rows),'births':births,'field_checks':checks,'differences':0}
(o/'i-wave-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

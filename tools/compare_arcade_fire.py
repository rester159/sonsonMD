#!/usr/bin/env python3
"""Original AD10 edge/counter/slot/position contract, including preserved bytes."""
import ctypes as C,hashlib,json,pathlib,subprocess
r=pathlib.Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_shots.c'),'-o',str(o/'arcade_fire.dylib')],check=True)
class Input(C.Structure):_fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ['y','facing','state','kind','available','input','previous']]
class Birth(C.Structure):_fields_=[('x',C.c_uint16),('y',C.c_uint8),('facing',C.c_uint8)]
lib=C.CDLL(str(o/'arcade_fire.dylib'));fn=lib.arcade_player_shot_fire;fn.argtypes=[C.POINTER(Input),C.c_uint8,C.POINTER(Birth)];fn.restype=C.c_uint8
program=b''.join((r/'arcade'/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e'])
program_hash=hashlib.sha256(program).hexdigest()
assert program_hash=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
trace=o/'fire-oracle.jsonl';rows=[json.loads(x)for x in trace.read_text().splitlines()];assert len(rows)==4096,len(rows)
target=o/'fire-target.csv';born=0
with target.open('w')as f:
 f.write('input,first_active,slot,birth\n')
 for row in rows:
  p=Input(*[row[n]for n,_ in Input._fields_]);birth=Birth(0x5678,0xab,0xcd);slot=fn(C.byref(p),row['active'],C.byref(birth))
  before=bytearray.fromhex(row['before']);expected=bytes.fromhex(row['after']);actual=bytearray(before)
  if slot!=255:
   assert slot in [0,1];offset=slot*32;actual[offset:offset+3]=bytes([1,0,0]);actual[offset+5:offset+7]=birth.x.to_bytes(2,'big');actual[offset+8]=birth.y;actual[offset+10]=birth.facing;born+=1
  assert bytes(actual)==expected,row
  assert row['after_available']==row['available'],row
  # Expected selection/pose comes from the changed original records, not C output.
  changed=[k for k in range(2)if before[k*32:(k+1)*32]!=expected[k*32:(k+1)*32]]
  assert len(changed)<=1
  selected=changed[0]if changed else 255
  wanted=expected[selected*32+5:selected*32+7]+bytes([expected[selected*32+8],expected[selected*32+10]])if changed else bytes.fromhex('5678abcd')
  raw=bytearray(bytes(p));raw[:2]=raw[:2][::-1]
  f.write(f'{raw[:9].hex()},{row["active"]},{selected},{wanted.hex()}\n')
report={'program_sha256':program_hash,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'target_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'cases':len(rows),'births':born,'byte_checks':len(rows)*64,'differences':0,'scope':'isolated original AD10 with two initialized records; source parent initialization/respawn separately audited'}
(o/'fire-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

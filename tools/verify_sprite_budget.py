#!/usr/bin/env python3
"""Source FED8 output order, atomic large admission, and remaining budget."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/sprite-budget';o.mkdir(exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_sprite_budget.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'));fn=lib.arcade_sprite_budget
fn.argtypes=[C.POINTER(C.c_uint8),C.c_uint8,C.POINTER(C.c_uint8)];fn.restype=C.c_uint8
remaining_fn=lib.arcade_sprite_remaining
remaining_fn.argtypes=[C.c_uint8,C.POINTER(C.c_uint8),C.POINTER(C.c_uint8)];remaining_fn.restype=C.c_uint8
trace=r/'out/parity-audit/sprite-budget.jsonl';rows=[json.loads(s)for s in trace.read_text().splitlines()];assert len(rows)==512
large_cases=0
for row in rows:
 active=(C.c_uint8*71).from_buffer_copy(bytes.fromhex(row['active']));mask=(C.c_uint8*9)()
 left=fn(active,row['solo'],mask);assert left==row['remaining'],row
 singles=sum(bool(active[i] and not(active[i]&128) and not(i==8 and row['solo']))for i in range(64))
 large=(C.c_uint8*3)(*active[64:67]);children=(C.c_uint8*4)(*active[67:71])
 assert remaining_fn(singles,large,children)==row['remaining'],row
 cells=bytearray()
 for i in range(71):
  if not(mask[i>>3]>>(i&7)&1):continue
  n=i+1;x=n*3&255;y=(-n*7-16)&255
  if 64<=i<67:
   large_cases+=1
   for k in range(4):cells+=bytes([(y-(16 if k&1 else 0))&255,0xc0+k,n,(x+(16 if k&2 else 0))&255])
  else:cells+=bytes([y,0xc3,n,x])
 cells+=bytes([0xf8])*(96-len(cells))
 assert cells.hex()==row['cells'],row
assert large_cases>0
report={'cases':len(rows),'sprite_bytes':len(rows)*96,'large_admissions':large_cases,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'source_sha256':hashlib.sha256((r/'src/arcade_sprite_budget.c').read_bytes()).hexdigest(),'scope':'Host full FED8 logical admission against original CPU output: normal gameplay, both P2 suppression states. Attract overlay, gameplay reservation ordering, target CPU and live mapper pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

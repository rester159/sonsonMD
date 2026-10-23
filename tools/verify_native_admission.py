#!/usr/bin/env python3
"""Map archived FED8 source states to native pools and compare every admitted bit."""
from pathlib import Path
import ctypes as C,subprocess,json,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/native-admission';o.mkdir(parents=True,exist_ok=True)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_native_admission.c'),str(r/'src/arcade_sprite_budget.c'),'-o',str(o/'test.dylib')],check=True)
l=C.CDLL(str(o/'test.dylib'))
class Out(C.Structure):_fields_=[('large',C.c_uint8),('children',C.c_uint8),('remaining',C.c_uint8),('order',C.c_uint8*24),('count',C.c_uint8)]
byte=C.POINTER(C.c_uint8);fn=l.arcade_native_admission;fn.argtypes=[byte,byte,byte,C.c_uint8,byte,byte,C.POINTER(Out)]
budget=l.arcade_sprite_budget;budget.argtypes=[byte,C.c_uint8,byte];budget.restype=C.c_uint8
trace=r/'out/parity-audit/sprite-budget.jsonl';rows=[json.loads(x)for x in trace.read_text().splitlines()]
# Exact source record positions, preserving family-local slots.
mapping={**{3+k:16+k for k in range(4)},**{9+k:24+k for k in range(6)},**{31+k:30+k for k in range(6)},**{37+k:36+k for k in range(4)},**{41+k:40+k for k in range(2)},**{43+k:42+k for k in range(4)},**{47+k:k for k in range(12)},**{59+k:20+k for k in range(4)},63:46}
ranks=(C.c_uint8*24)(*([0]*6+[1]*6+[2]*4+[3]*2+[4]*4+[5,6]))
for row in rows:
 active=(C.c_uint8*71)(*bytes.fromhex(row['active']));mask=(C.c_uint8*9)();left=budget(active,row['solo'],mask);assert left==row['remaining']
 entities=(C.c_uint8*48)();main=(C.c_uint8*16)(*active[15:31]);large=(C.c_uint8*3)(*active[64:67]);children=(C.c_uint8*4)(*active[67:71])
 for src,dst in mapping.items():entities[dst]=active[src]
 reserved=sum(bool(active[i] and active[i]<128 and not(i==8 and row['solo']))for i in [0,1,2,7,8])
 out=Out();fn(entities,ranks,main,reserved,large,children,C.byref(out));assert out.remaining==left
 admitted=lambda i:bool(mask[i>>3]&(1<<(i&7)))
 for src,dst in mapping.items():assert (dst in out.order[:out.count])==admitted(src),(row['scenario'],src)
 expected_order=[mapping[i] if i in mapping else 48+i-15 if 15<=i<31 else 12+i-67 for i in range(71) if (i in mapping or 15<=i<31 or i>=67) and admitted(i)]
 assert out.count<=24 and list(out.order[:out.count])==expected_order
 for k in range(16):assert (48+k in out.order[:out.count])==admitted(15+k)
 for k in range(3):assert bool(out.large&(1<<k))==admitted(64+k)
 for k in range(4):assert bool(out.children&(1<<k))==admitted(67+k)
# A physically offscreen main actor still owns a cell; capacity admits only first19
# with five reserved players/specials, regardless of where drawing later culls.
entities=(C.c_uint8*48)();main=(C.c_uint8*16)(*[1]*16);large=(C.c_uint8*3)(1,1,1);children=(C.c_uint8*4)(1,1,1,1)
for k in range(12):entities[k]=1
out=Out();fn(entities,ranks,main,5,large,children,C.byref(out));assert list(out.order[:out.count])==list(range(48,64))+[0,1,2] and out.large==0 and out.children==0 and out.remaining==0
report={'original_trace_cases':len(rows),'admitted_bits_checked':len(rows)*66,'ordered_stream_cases':len(rows),'overload_cases':1,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'scope':'Native pool identity mapping and every main/entity/large/K admission plus ordered single-cell stream against archived original FED8 states. Provisional live family populations, actual sprite priority and target rendering are separate.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

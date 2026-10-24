#!/usr/bin/env python3
"""Native four-cell animation vs original D52A/D924 CPU results."""
from pathlib import Path
import csv,ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit';trace=o/'large-animation-oracle.csv'
(o/'large-animation-comparison.json').unlink(missing_ok=True)
assert 'LARGE_ANIMATION_DONE'in(o/'large-animation-source/mame.log').read_text()
source=r/'src/arcade_large_animation.c';data=r/'src/arcade_large_animation_data.c'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(source),str(data),'-o',str(o/'large-animation.dylib')],check=True)
class Animation(C.Structure):_fields_=[('cursor',C.c_uint16),('timer',C.c_uint8),('code',C.c_uint8*4),('attr',C.c_uint8*4)]
lib=C.CDLL(str(o/'large-animation.dylib'));lib.arcade_large_animate.argtypes=[C.POINTER(Animation)]
lib.arcade_large_damage_pose.argtypes=[C.POINTER(Animation),C.c_uint8,C.c_uint8,C.c_uint8,C.POINTER(C.c_uint8)]
addresses=json.loads((o/'large-animation-map.json').read_text())['addresses'];ids={a:k for k,a in enumerate(addresses)}
def convert(b):return ids[int.from_bytes(b[14:16],'big')].to_bytes(2,'big')+b[13:14]+b[16:24]
rows=list(csv.DictReader(trace.open()));assert len(rows)==5248
fixtures=[]
for d in rows:
 b=bytes.fromhex(d['before']);a=bytes.fromhex(d['after']);v=Animation(ids[int.from_bytes(b[14:16],'big')],b[13],(C.c_uint8*4)(*b[16:20]),(C.c_uint8*4)(*b[20:24]));pending=C.c_uint8(b[25]);op=int(d['operation'])
 if op:lib.arcade_large_damage_pose(C.byref(v),b[24],b[26],b[3],C.byref(pending))
 else:lib.arcade_large_animate(C.byref(v))
 got=v.cursor.to_bytes(2,'big')+bytes([v.timer])+bytes(v.code)+bytes(v.attr)+bytes([pending.value]);expected=convert(a)+a[25:26]
 assert got==expected,(d,got.hex(),expected.hex())
 allowed={13,14,15,*range(16,24),25}
 assert all(b[k]==a[k]for k in range(48)if k not in allowed)
 fixtures.append((convert(b)+bytes([b[24],b[26],b[3],b[25],op])).hex()+','+expected.hex())
(o/'large-animation-target.csv').write_text('\n'.join(fixtures)+'\n')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
program=hashlib.sha256(b''.join((r/'arcade'/n).read_bytes()for n in('ss.01e','ss.02e','ss.03e'))).hexdigest();assert program=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
d={'program_sha256':program,'trace_sha256':sha(trace),'native_sha256':sha(source),'data_sha256':sha(data),'target_sha256':sha(o/'large-animation-target.csv'),'cases':len(rows),'output_bytes_checked':len(rows)*12,'frame_count':len(addresses),'scope':'Original D52A/D924 vs native C. Entry, exit, death and HP walk scripts; all valid HP/facing/subtype refresh selections. Not renderer or actor integration.'}
(o/'large-animation-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))

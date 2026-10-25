#!/usr/bin/env python3
"""Compare typed course reset with source-address writes, offline."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/course-waves';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
clears={0x9048:0x628,0x96cc:0x638,0x96d9:0x648,0x96e6:0x658,0x96f3:0x668,0x9700:0x678,0x971c:0x688,0x9729:0x6a8,0x9736:0x698,0x9743:0x6b8,0x976f:0x6d8}
for pc,addr in clears.items():assert rom[pc-0x4000:pc-0x4000+3]==bytes([0x7f,addr>>8,addr&255])
assert rom[0x904b-0x4000:0x9067-0x4000]==bytes.fromhex('8e 06 38 5f'+' e7 80'*12)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_waves.c'),str(r/'src/arcade_formations.c'),'-o',str(o/'test.dylib')],check=True)
class Control(C.Structure):_fields_=[('auxiliary',C.c_uint16)]+[(n,C.c_uint8)for n in ('active','state','phase','wait','parameter')]
class Waves(C.Structure):_fields_=[('mask',C.c_uint16),('placement',C.c_uint16)]+[(n,C.c_uint8)for n in ('cached_coarse','state','cursor','remaining','delay')]+[('controls',Control*12)]
lib=C.CDLL(str(o/'test.dylib'));lib.arcade_waves_course_begin.argtypes=[C.POINTER(Waves)]
for seed in range(256):
 mem=bytearray(((i*13+seed)&255)for i in range(0x700));w=Waves()
 def word(a):return int.from_bytes(mem[a:a+2],'big')
 w.mask=word(0x62e);w.placement=word(0x62c);w.cached_coarse=mem[0x628];w.state=mem[0x62a];w.cursor=seed%12;w.remaining=mem[0x630];w.delay=mem[0x9c]
 cursor=w.cursor
 for i,c in enumerate(w.controls):
  base=0x638+16*i
  for n,off in [('active',0),('wait',1),('state',2),('phase',3),('parameter',14)]:setattr(c,n,mem[base+off])
  c.auxiliary=word(base+10)
 for addr in clears.values():mem[addr]=0
 mem[0x638:0x644]=bytes(12)
 lib.arcade_waves_course_begin(C.byref(w))
 assert (w.mask,w.placement,w.cached_coarse,w.state,w.cursor,w.remaining,w.delay)==(word(0x62e),word(0x62c),mem[0x628],mem[0x62a],cursor,mem[0x630],mem[0x9c])
 for i,c in enumerate(w.controls):
  base=0x638+16*i
  for n,off in [('active',0),('wait',1),('state',2),('phase',3),('parameter',14)]:assert getattr(c,n)==mem[base+off],(seed,i,n)
  assert c.auxiliary==word(base+10)
report={'cases':256,'scope':'Typed course-reset compared to source-address writes guarded by source revision and opcode bytes. Preserves producer and controller residual fields; no runtime timing or natural repeated-course proof.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

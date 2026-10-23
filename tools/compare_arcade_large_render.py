#!/usr/bin/env python3
"""Compare native cell emission with original FF6A output and admission."""
import csv,ctypes as C,hashlib,json,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
program=hashlib.sha256(b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))).hexdigest()
assert program=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
assert 'LARGE_RENDER_DONE' in (o/'large-render-source/mame.log').read_text()
class Animation(C.Structure):_fields_=[('cursor',C.c_uint16),('timer',C.c_uint8),('code',C.c_uint8*4),('attr',C.c_uint8*4)]
class Body(C.Structure):_fields_=[('animation',Animation)]+[(k,C.c_uint8)for k in ('active','state','phase','subtype','variant','owner','hp','timer')]
class Actor(C.Structure):_fields_=[('body',Body),('x',C.c_uint16)]+[(k,C.c_uint8)for k in ('xf','y','yf','direction','facing','hit','lifetime','fire_timer','fire_period','target','retreat')]
assert C.sizeof(Actor)==34
source=r/'src/arcade_large_render.c'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(source),'-o',str(o/'large-render.dylib')],check=True)
lib=C.CDLL(str(o/'large-render.dylib'));fn=lib.arcade_large_cells;fn.argtypes=[C.POINTER(Actor),C.c_uint16,C.POINTER(C.c_uint8),C.POINTER(C.c_uint8)];fn.restype=C.c_uint8
trace=o/'large-render-oracle.csv';rows=list(csv.DictReader(trace.open()));assert len(rows)==4096
fixtures=[];emitted=0
for d in rows:
 p=bytes.fromhex(d['record']);a=Actor();a.body.active=p[0];a.x=int.from_bytes(p[5:7],'big');a.y=p[8];a.body.animation.code[:]=p[16:20];a.body.animation.attr[:]=p[20:24]
 budget=C.c_uint8(int(d['budget']));cells=(C.c_uint8*16)(*([165]*16));count=fn(C.byref(a),int(d['camera']),C.byref(budget),cells)
 assert bytes(cells)==bytes.fromhex(d['output']) and count==int(d['count']) and budget.value==int(d['remaining']),d
 inp=bytes([p[0]])+p[5:7]+bytes([p[8]])+p[16:24]+int(d['camera']).to_bytes(2,'big')+bytes([int(d['budget'])])
 out=bytes([count,budget.value])+bytes(cells);fixtures.append(inp.hex()+','+out.hex());emitted+=count
(o/'large-render-target.csv').write_text('\n'.join(fixtures)+'\n')
d={'program_sha256':program,'cases':len(rows),'emitted_cells':emitted,'trace_sha256':sha(trace),'native_sha256':sha(source),'target_sha256':sha(o/'large-render-target.csv'),'scope':'FF6A raw hardware cells and budget admission; Genesis tile/palette mapping and gameplay renderer not yet bound'}
(o/'large-render-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))

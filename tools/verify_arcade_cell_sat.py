#!/usr/bin/env python3
"""Check SAT geometry/flip against source three-copy sprite raster semantics."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit';src=r/'src/arcade_cell_sat.c'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(src),'-o',str(o/'cell-sat.dylib')],check=True)
class Cell(C.Structure):_fields_=[(k,C.c_uint8)for k in ('y','attr','code','x')]
class View(C.Structure):_fields_=[('x',C.c_int16),('y',C.c_int16),('width',C.c_uint16),('height',C.c_uint16)]
lib=C.CDLL(str(o/'cell-sat.dylib'));fn=lib.arcade_cell_sat;fn.argtypes=[C.POINTER(Cell),C.c_uint16,C.POINTER(View),C.POINTER(C.c_uint16),C.c_uint16,C.c_uint16];fn.restype=C.c_uint16
checks=0;cases=0
# Include original visible rectangle and two Genesis crops. No viewport choice
# is asserted as the integrated game's policy by this conversion test.
for v in (View(8,8,240,240),View(0,16,256,224),View(0,0,256,224)):
 for x in (0,1,7,8,15,16,223,224,239,240,241,247,248,255):
  for y in (0,1,7,8,15,16,223,224,239,240,241,247,248,255):
   for attr in (0,64,128,192,250):
    c=Cell(y,attr,17,x);sat=(C.c_uint16*256)(*([0xa5a5]*256));n=fn(C.byref(c),1308,C.byref(v),sat,2,64)
    expected=[]
    for sx,sy in ((x,y),(x-256,y),(x,y-256)):
     left,top=sx-v.x,sy-v.y
     if max(left,0)<min(left+16,v.width) and max(top,0)<min(top+16,v.height):expected.append((left,top))
    assert n==len(expected)
    for i,(left,top)in enumerate(expected):
     p=list(sat[(2+i)*4:(3+i)*4]);assert p==[(128+top)&1023,0x500|3+i,0x2000|1308|(0 if attr&64 else 0x800)|(0 if attr&128 else 0x1000),(128+left)&511]
     # Unique source pixel identity proves both axes flip at visible pixels.
     for py in range(max(0,top),min(v.height,top+16)):
      for px in range(max(0,left),min(v.width,left+16)):
       gx=px-left;gy=py-top
       actual=(15-gy if p[2]&0x1000 else gy)*16+(15-gx if p[2]&0x800 else gx)
       expected_pixel=(gy if attr&128 else 15-gy)*16+(gx if attr&64 else 15-gx)
       assert actual==expected_pixel;checks+=1
    assert list(sat[:8])==[0xa5a5]*8 and all(z==0xa5a5 for z in sat[(2+n)*4:])
    if n:
     before=bytes(sat);assert fn(C.byref(c),1308,C.byref(v),sat,2,2+n-1)==65535 and bytes(sat)==before
    cases+=1
report={'cases':cases,'visible_pixel_checks':checks,'native_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'scope':'Host compiled SAT cell conversion, source independent wrap copies, clipping, inverted flip bits, links and capacity. Full renderer priority, viewport policy, target SAT/display and integration pending.'}
(o/'cell-sat-results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

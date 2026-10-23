#!/usr/bin/env python3
"""Independent raw-ROM pixel decode and compiled generated lookup checks."""
from pathlib import Path
import ctypes as C,csv,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit';d=json.loads((o/'large-art-map.json').read_text())
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_large_art.c'),'-o',str(o/'large-art.dylib')],check=True)
lib=C.CDLL(str(o/'large-art.dylib'));fn=lib.arcade_large_art_frame;fn.argtypes=[C.c_uint8,C.c_uint8];fn.restype=C.c_uint16
pixels=(C.c_uint32*(d['distinct_frames']*32)).in_dll(lib,'arcade_large_art_pixels')
planes=[(r/'arcade'/a).read_bytes()+(r/'arcade'/b).read_bytes()for a,b in [('ss_9.m5','ss_10.m6'),('ss_11.m3','ss_12.m4'),('ss_13.m1','ss_14.m2')]]
lut=(r/'arcade/ssb3.h7').read_bytes();mapping={(p['code'],p['bank']):p['frame']for p in d['mapping']}
checks=0
for (code,bank),frame in mapping.items():
 for y in range(16):
  for x in range(16):
   pos=code*32+y+(16 if x<8 else 0);bit=x%8 # MAME layout xoffset 7..0, MSB-first bit reader.
   pen=sum(((plane[pos]>>bit)&1)<<i for i,plane in enumerate(planes))
   color=(lut[bank*8+pen]&15)if pen else 0
   if pen and not color:color=15
   tile=(x//8)*2+y//8;row=pixels[frame*32+tile*8+y%8]
   actual=(row>>(28-(x%8)*4))&15
   assert actual==color,(code,bank,x,y,actual,color);checks+=1
for code in range(256):
 for attr in range(256):
  expected=mapping.get((code+((attr&32)<<3),attr&31),65535)
  assert fn(code,attr)==expected,(code,attr)
reachable=0
for name,large in [('large-motion-oracle.csv',True),('large-death-oracle.csv',True),('k-projectile-oracle.csv',False)]:
 for row in csv.DictReader((o/name).open()):
  p=bytes.fromhex(row['after'])
  if not p[0]:continue
  for k in range(4 if large else 1):
   code=p[16+k];attr=p[20+k]if large else p[17]
   assert fn(code,attr)!=65535,(name,row['case'],row['tick'],code,attr)
   reachable+=1
report={'pixel_checks':checks,'lookup_checks':65536,'source_trajectory_cell_checks':reachable,'distinct_frames':d['distinct_frames'],'native_sha256':hashlib.sha256((r/'src/arcade_large_art.c').read_bytes()).hexdigest(),'scope':'Raw ROM palette-index pixels and host compiled frame lookup; not VDP output or RGB palette accuracy'}
(o/'large-art-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

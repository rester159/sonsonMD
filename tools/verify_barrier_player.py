#!/usr/bin/env python3
"""Compare native player blocking with independent E55F/E6FF observations."""
from pathlib import Path
import ctypes as C
import csv
import hashlib
import json
import subprocess
r=Path(__file__).resolve().parents[1]
o=r/'out/parity-audit/barrier-player';o.mkdir(exist_ok=True)
(o/'wrapper.c').write_text('#include "arcade_barrier.h"\nvoid probe(unsigned short camera,short dx,short dy,unsigned char active,unsigned char pa,unsigned char twice,unsigned short offset,unsigned short *v){\n ArcadeWorldObjectPool pool={0};ArcadePlayer p={0};u16 i,ox=(u16)(camera+offset);\n p.x=(u16)(ox+dx);p.y=(u8)(250+dy);p.active=pa;p.state=2;p.phase=1;\n for(i=0;i<20;i+=19){pool.objects[i].active=(i==0||twice)?active:0;pool.objects[i].kind=2;pool.objects[i].x=ox;pool.objects[i].y=250;}\n v[3]=arcade_barrier_player(&pool,&p,camera);v[0]=p.x;v[1]=p.state;v[2]=p.phase;\n}\n')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(o/'wrapper.c'),str(r/'src/arcade_barrier.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'))
lib.probe.argtypes=[C.c_uint16]+[C.c_int16]*2+[C.c_uint8]*3+[C.c_uint16]+[C.POINTER(C.c_uint16)]
trace=r/'out/parity-audit/barrier-player.csv'
rows=list(csv.DictReader(trace.open()));assert len(rows)==5040,len(rows)
fields=('x','state','phase','kind')
for row in rows:
    v=(C.c_uint16*4)()
    lib.probe(*[int(row[k])for k in ('camera','dx','dy','active','pa','double','offset')],v)
    assert list(v)==[int(float(row[k]))for k in fields],(row,list(v))
report={'cases':len(rows),'field_checks':len(rows)*4,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'scope':'Original E55F/E6FF player/barrier scan, wrapping geometry, visibility, active flags, repeated pushes and left-edge crush. Target/live life-state binding pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

#!/usr/bin/env python3
"""Compare native barrier shots with independent EB11 observations."""
from pathlib import Path
import ctypes as C
import csv
import hashlib
import json
import subprocess
r=Path(__file__).resolve().parents[1]
o=r/'out/parity-audit/barrier-shots';o.mkdir(exist_ok=True)
(o/'wrapper.c').write_text('#include "arcade_barrier.h"\nvoid probe(short dx,short dy,unsigned char active,unsigned char shot,unsigned char twice,unsigned char next,unsigned short *v){\n ArcadeWorldObjectPool p={0};ArcadePlayerShot s={0};u8 phases[20]={0};u16 i;\n s.active=shot;s.x=65530;s.y=250;\n for(i=0;i<20;i+=19){p.objects[i].active=(i==0||twice)?active:0;p.objects[i].phase=3;p.objects[i].kind=2;p.objects[i].x=(u16)(65530+dx);p.objects[i].y=(u8)(250+dy);phases[i]=next;}\n v[5]=arcade_barrier_shots(&p,phases,&s);\n v[0]=p.objects[0].phase;v[1]=p.objects[19].phase;v[2]=s.active;v[3]=s.state;v[4]=s.phase;\n}\n')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(o/'wrapper.c'),str(r/'src/arcade_barrier.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'))
lib.probe.argtypes=[C.c_int16]*2+[C.c_uint8]*4+[C.POINTER(C.c_uint16)]
trace=r/'out/parity-audit/barrier-shots.csv'
rows=list(csv.DictReader(trace.open()));assert len(rows)==1680,len(rows)
fields=('phase0','phase19','shot_active','shot_state','shot_phase','events')
for row in rows:
    v=(C.c_uint16*6)()
    lib.probe(*[int(row[k])for k in ('dx','dy','active','shot','double','nextphase')],v)
    assert list(v)==[int(float(row[k]))for k in fields],(row,list(v))
report={'cases':len(rows),'field_checks':len(rows)*6,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'scope':'Original EB11 shot/background scan: wrapped geometry boundaries, active bits, two overlapping objects including final slot, hit retirement and repeated event count. Lifecycle and live binding pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

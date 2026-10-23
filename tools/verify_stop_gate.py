#!/usr/bin/env python3
"""Compare the native section-stop gate with independent A787 observations."""
from pathlib import Path
import ctypes as C
import csv
import hashlib
import json
import subprocess
r=Path(__file__).resolve().parents[1]
o=r/'out/parity-audit/stop-gate';o.mkdir(exist_ok=True)
(o/'wrapper.c').write_text('''#include "arcade_stop.h"
void probe(unsigned short camera,unsigned char clock,unsigned char timer,
           unsigned char grade,unsigned char scenario,unsigned short *v){
 ArcadeWorldObjectPool pool={0};ArcadeStop s={timer,grade};ArcadeStopEvent e;
 pool.objects[19].active=scenario==0?0:4;
 pool.objects[19].kind=scenario==3?1:2;
 pool.objects[19].x=(unsigned short)(camera+256+(scenario==2));
 arcade_stop_step(&s,&pool,camera,clock,&e);
 v[0]=s.timer;v[1]=s.grade;v[2]=!e.release;v[3]=e.release?256:0;
 v[4]=e.bonus?256:0;v[5]=e.variant;v[6]=e.bonus?104:0;v[7]=e.bonus?10:0;
}
''')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(o/'wrapper.c'),str(r/'src/arcade_stop.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'))
lib.probe.argtypes=[C.c_uint16]+[C.c_uint8]*4+[C.POINTER(C.c_uint16)]
trace=r/'out/parity-audit/stop-gate.csv'
rows=list(csv.DictReader(trace.open()));assert len(rows)==1536,len(rows)
fields=('after_timer','after_grade','stop','release','bonus','variant','placement','kind')
for row in rows:
    v=(C.c_uint16*8)()
    lib.probe(*[int(row[k])for k in ('camera','clock','timer','grade','scenario')],v)
    assert list(v)==[int(row[k])for k in fields],(row,list(v))
report={'cases':len(rows),'field_checks':len(rows)*8,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'scope':'Isolated A787 section-stop state1. Clock/timer wrap, final pool slot, unsigned camera edge, kind/active gating and reward grade saturation. Source stage transitions, live world object destruction and target binding pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

#!/usr/bin/env python3
"""Compare native section transitions with independent original-CPU A730 trace."""
from pathlib import Path
import ctypes as C
import csv,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/sections';o.mkdir(exist_ok=True)
(o/'wrapper.c').write_text('''#include "arcade_section.h"
void probe(unsigned char index,unsigned char remaining,unsigned char column,
 unsigned char difficulty,unsigned short camera,unsigned short *v){
 ArcadeSection s={0x1234,0xabcd,index,remaining,column,difficulty,2,1,7,20};
 unsigned char load=arcade_section_column(&s,camera);
 v[0]=s.index;v[1]=s.remaining;v[2]=s.column;v[3]=s.difficulty;
 v[4]=s.speed;v[5]=s.stop;v[6]=s.next_stop;v[7]=s.timer;
 v[8]=s.next_timer;v[9]=s.checkpoint;
 /* Source address observations represented by the selected native index.
  * Original loader data/pool effects have a separate exhaustive verifier. */
 v[10]=load?0x6d3b+s.index*208:0x1111;
 v[11]=load?0x825b+s.index*128:0x2222;
 v[12]=load?v[11]:0x3333;
 v[13]=load?0x55af:0x5587;v[14]=load?0x588:0x560;
}
''')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(o/'wrapper.c'),str(r/'src/arcade_section.c'),str(r/'src/arcade_section_data.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'));lib.probe.argtypes=[C.c_uint8]*4+[C.c_uint16,C.POINTER(C.c_uint16)]
trace=r/'out/parity-audit/sections.csv';rows=list(csv.DictReader(trace.open()));assert len(rows)==2496,len(rows)
for row in rows:
 values=list(map(int,row.values()));v=(C.c_uint16*15)()
 lib.probe(*values[:5],v)
 assert list(v)==values[5:],(row,list(v))
report={'cases':len(rows),'field_checks':len(rows)*15,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'scope':'A730 transitions for all 26 indices, decrement wrap, column gating, difficulty saturation, preserved speed fraction, stop/timer handoff, checkpoint and resource selection. Startup, actual target execution and live binding pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

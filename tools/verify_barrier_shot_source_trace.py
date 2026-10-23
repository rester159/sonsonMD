#!/usr/bin/env python3
"""Check the live shot wrapper against saved independent EB11 observations."""
from pathlib import Path
import argparse,ctypes as C,csv,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output-dir',type=Path);a=p.parse_args()
s=a.source.read_text();body=s[s.index('u16 arcade_food_world_shot('):s.index('u16 arcade_food_world_contact(')]
o=a.output_dir or r/'out/parity-audit/barrier-shot-single-pass/source-trace';o.mkdir(parents=True,exist_ok=True)
c='''#include <string.h>
#include "arcade_barrier.h"
static ArcadeWorldObjectPool pool;
static u8 barrier_cursor[20];
/* Trace provides the independent source animation lookahead byte. */
static u8 arcade_barrier_next_phase(u8 cursor){return cursor;}
'''+(s[s.index('static u8 food_slots'):s.index('static void remember(')] if 'static u8 food_slots' in s else '')+body+'''
void probe(short dx,short dy,u8 active,u8 shot,u8 twice,u8 next,u16 *v){
 ArcadePlayerShot s={0};memset(&pool,0,sizeof(pool));
 s.active=shot;s.x=65530;s.y=250;
 for(u16 i=0;i<20;i+=19){ArcadeWorldObject *a=&pool.objects[i];
 a->active=(i==0||twice)?active:0;a->phase=3;a->kind=2;
 a->x=(u16)(65530+dx);a->y=(u8)(250+dy);barrier_cursor[i]=next;}
 v[5]=arcade_food_world_shot(&s)/1000;
 v[0]=pool.objects[0].phase;v[1]=pool.objects[19].phase;
 v[2]=s.active;v[3]=s.state;v[4]=s.phase;
}
'''
if 'static u8 food_slots' in s:c=c.replace(' v[5]=arcade_food_world_shot(&s)/1000;',' candidates_dirty=1;v[5]=arcade_food_world_shot(&s)/1000;')
(o/'test.c').write_text(c)
subprocess.run(['cc','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_barrier.c'),'-o',str(o/'test.dylib')],check=True)
l=C.CDLL(str(o/'test.dylib'));l.probe.argtypes=[C.c_int16]*2+[C.c_uint8]*4+[C.POINTER(C.c_uint16)]
trace=r/'out/parity-audit/barrier-shots.csv';rows=list(csv.DictReader(trace.open()));assert len(rows)==1680
for row in rows:
 v=(C.c_uint16*6)();l.probe(*[int(row[k])for k in ('dx','dy','active','shot','double','nextphase')],v)
 assert list(v)==[int(float(row[k])) for k in ('phase0','phase19','shot_active','shot_state','shot_phase','events')],(row,list(v))
report={'cases':len(rows),'source_sha256':hashlib.sha256(s.encode()).hexdigest(),'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'scope':'Actual live shot wrapper vs saved original-CPU EB11 observations. Lookahead byte supplied by trace; checks wrap boundaries, activation, two hits, retirement and awards. Host execution, not full game parity.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

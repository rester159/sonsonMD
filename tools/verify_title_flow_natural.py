#!/usr/bin/env python3
"""Compare persistent native title flow with archived natural arcade snapshots.

No emulator launch. No inferred queue-busy values or title states are injected
from the expected rows: this run explicitly models immediate screen completion.
The comparison covers state, front-end phase, and main-demo timer, not pixels.
"""
import csv
import hashlib
import json
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[1]
archive=root/'out/parity-audit/attract-natural'
trace=archive/'trace.csv'
assert 'ATTRACT_NATURAL_DONE 9000' in (archive/'mame.log').read_text()
rows=[{k:int(v) for k,v in row.items()} for row in csv.DictReader(trace.open())]
assert len(rows)==9000 and rows[-1]['frame']==9000
# Pin the archived observations, rather than silently accepting replacement data.
trace_hash=hashlib.sha256(trace.read_bytes()).hexdigest()
assert trace_hash=='e6857e84b47ff6a1c8b1c1897102fe42b48e5eb6373432c9bf13c556873af1cf'
first=next(i for i,r in enumerate(rows) if r['frame']>978 and r['mode']==1)
assert rows[first]['state']==rows[first]['substate']==rows[first]['timer']==0
checks=[]
states={}
for prev,row in zip(rows[first:],rows[first+1:]):
    assert row['mode']==1 and row['live']==0
    assert row['clock']==prev['clock']+1
    checks.append(f'in.clock={row["clock"]&255};arcade_title_step(&s,&in,&e);')
    checks.append(f'check(s.state,{row["state"]},{row["frame"]},"state");')
    checks.append(f'check(s.demo_timer,{row["timer"]},{row["frame"]},"demo timer");')
    # 9646 owns08 during demo; the title sequencer only owns it outside state3.
    if row['state']!=3:
        checks.append(f'check(s.phase,{row["substate"]},{row["frame"]},"phase");')
    states[row['state']]=states.get(row['state'],0)+1
out=root/'out/parity-audit/title-lifecycle'
out.mkdir(parents=True,exist_ok=True)
test=out/'natural.c'
test.write_text('''#include <stdio.h>
#include <stdlib.h>
#include "arcade_title_flow.h"
static void check(unsigned got,unsigned want,unsigned frame,const char *field){
 if(got!=want){fprintf(stderr,"frame %u %s: got %u expected %u\\n",frame,field,got,want);exit(1);}
}
int main(void){ArcadeTitleFlow s={0};ArcadeTitleInput in={0};ArcadeTitleEvent e;
'''+ '\n'.join(checks)+'\nreturn 0;}\n')
subprocess.run(['cc','-std=c99','-O0','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-I'+str(root/'inc'),str(test),
                str(root/'src/arcade_title_flow.c'),'-o',str(out/'natural')],check=True)
subprocess.run([str(out/'natural')],check=True)
report={'trace_sha256':trace_hash,'first_frame':rows[first]['frame'],
        'last_frame':rows[-1]['frame'],'updates':sum(states.values()),
        'state_observations':states,
        'field_checks':sum(2 if r['state']==3 else 3 for r in rows[first+1:]),
        'source_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in (root/'src/arcade_title_flow.c',root/'inc/arcade_title_flow.h')},
        'passed':True,
        'scope':'Persistent native flow vs archived original-CPU uncredited state/phase/demo-timer observations, two complete cycles and partial third; ASan/UBSan. Assumes immediate queue completion. Busy timing, credit interruptions, demo-internal substate, pixels, prelude actor state and target integration are not verified.'}
(out/'natural-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

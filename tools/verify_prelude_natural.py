#!/usr/bin/env python3
"""Prelude input comparison against archived, unmodified original CPU frames."""
import csv
import hashlib
import json
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[1]
trace=root/'out/parity-audit/attract-natural/trace.csv'
assert hashlib.sha256(trace.read_bytes()).hexdigest()=='e6857e84b47ff6a1c8b1c1897102fe42b48e5eb6373432c9bf13c556873af1cf'
rom=b''.join((root/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert rom[0x53eb:0x53f9]==bytes.fromhex('cc9bb6dd4acc9bc4dd4c0f480f49')
rows=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(trace.open())]
checks=[];cycles=steps=0
fields=[('cursor','ptr',0x9a6e),('input','input',0),('previous','previous',0),('delay','delay',0)]
for prev,row in zip(rows,rows[1:]):
    if (prev['mode'],prev['state'])!=(1,1):continue
    if prev['substate']==2:
        # Establish retained bytes from the incoming observed state once per
        # cycle; subsequent calls operate on persistent native state.
        for i in range(2):
            for field,suffix,base in fields:
                value=prev[f'p{i+1}{suffix}']-base
                if field=='cursor':value &=65535
                checks.append(f's.{field}[{i}]={value};')
        checks.append('arcade_attract_prelude_init(&s);')
        cycles+=1
    elif prev['substate']==4:
        assert row['clock']==prev['clock']+1
        checks.append(f'assert(arcade_attract_step(&s,{row["clock"]&255}));')
        steps+=1
    else:continue
    for i in range(2):
        for field,suffix,base in fields:
            checks.append(f'check(s.{field}[{i}],{row[f"p{i+1}{suffix}"]-base},{row["frame"]},"{field}{i}");')
assert cycles==3 and steps>1800,(cycles,steps)
out=root/'out/parity-audit/prelude-input';out.mkdir(parents=True,exist_ok=True)
test=out/'test.c'
test.write_text('''#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "arcade_attract.h"
static void check(unsigned got,unsigned want,unsigned frame,const char *field){
 if(got!=want){fprintf(stderr,"frame %u %s: %u != %u\\n",frame,field,got,want);exit(1);}
}
int main(void){ArcadeAttract s={0};
'''+ '\n'.join(checks)+'\nreturn 0;}\n')
subprocess.run(['cc','-std=c99','-O0','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'inc'),str(test),str(root/'src/arcade_attract.c'),str(root/'src/arcade_attract_data.c'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
report={'passed':True,'cycles':cycles,'source_updates':steps,'field_checks':(steps+cycles)*8,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),
        'scope':'Persistent native prelude input streams versus three natural original-CPU prelude cycles, including initialization retained bytes and exit ticks. Initial state seeded once per cycle; no actor, rendering, clock ownership or target integration claim.'}
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

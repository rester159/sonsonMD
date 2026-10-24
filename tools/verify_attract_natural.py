#!/usr/bin/env python3
"""Compare the native sequencer with read-only natural original-CPU snapshots."""
from pathlib import Path
import csv
import hashlib
import json
import subprocess

root = Path(__file__).resolve().parents[1]
trace = root/'out/parity-audit/attract-natural/trace.csv'
out = root/'out/parity-audit/attract-sequencer'
out.mkdir(parents=True, exist_ok=True)
rows = [{k: int(v) for k, v in row.items()} for row in csv.DictReader(trace.open())]
assert len(rows) == 9000 and rows[-1]['frame'] == 9000
tests = []
cycles = 0
steps = 0
for prev, row in zip(rows, rows[1:]):
    if (prev['mode'], prev['state'], prev['substate'], prev['playstate']) != (1, 3, 2, 0):
        continue
    initial = prev['p1ptr'] == 0x9a6e and prev['p2ptr'] == 0x9b14
    if initial:
        cycles += 1
        tests.append('arcade_attract_init(&s);')
    assert cycles
    assert row['clock'] == prev['clock']+1
    tests.append(f'assert(arcade_attract_step(&s, {row["clock"] & 255}));')
    for i in range(2):
        for field, suffix, adjustment in (('cursor', 'ptr', 0x9a6e), ('input', 'input', 0),
                                          ('previous', 'previous', 0), ('delay', 'delay', 0)):
            value = row[f'p{i+1}{suffix}']-adjustment
            tests.append(f'check(s.{field}[{i}], {value}, {row["frame"]}, "{field}{i}");')
    steps += 1
assert cycles == 3 and steps == 5179, 'Unexpected natural-demo coverage'
source = '''#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arcade_attract.h"
static void check(unsigned got,unsigned want,unsigned frame,const char *field) {
 if(got!=want){fprintf(stderr,"frame %u %s: got %u expected %u\\n",frame,field,got,want);exit(1);}
}
int main(void){ArcadeAttract s;
'''+ '\n'.join(tests) + '''
/* Out-of-range and unaligned reads must fail atomically, not loop silently. */
for(unsigned player=0;player<2;player++)for(unsigned bad=355;bad<358;bad++) {
 arcade_attract_init(&s);s.cursor[player]=bad;ArcadeAttract before=s;
 assert(!arcade_attract_step(&s,0));assert(!memcmp(&s,&before,sizeof(s)));
}
return 0;}
'''
(out/'test.c').write_text(source)
subprocess.run(['cc', '-std=c99', '-O0', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-I'+str(root/'inc'), str(out/'test.c'),
                str(root/'src/arcade_attract.c'), str(root/'src/arcade_attract_data.c'),
                '-o', str(out/'test')], check=True)
subprocess.run([str(out/'test')], check=True)
report = {'steps': steps, 'field_comparisons': steps*8, 'demo_entries': cycles,
          'trace_sha256': hashlib.sha256(trace.read_bytes()).hexdigest(),
          'source_sha256': {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                            for p in (root/'src/arcade_attract.c', root/'src/arcade_attract_data.c',
                                      root/'inc/arcade_attract.h')},
          'scope': 'Persistent native input sequencer against natural original-CPU main-demo snapshots, including exit ticks. Two complete demos and one partial. ASan/UBSan. No target integration or full attract presentation claim.'}
(out/'report.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))

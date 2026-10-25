#!/usr/bin/env python3
"""Source-rate cadence arithmetic against independent rational elapsed-time model."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/arcade-pacing';o.mkdir(parents=True,exist_ok=True)
c=r'''
#include <assert.h>
#include <stdint.h>
#include "arcade_pacing.h"
int main(void){unsigned pal,seed,i;uint64_t cases=0;
for(pal=0;pal<2;pal++)for(seed=0;seed<16;seed++){
 ArcadePacing p;uint64_t active=0,taken=0;u16 refresh=65520+seed;
 const uint64_t num=277ULL*3420*(pal?313:262),den=5ULL*(pal?53203424:53693175);
 arcade_pacing_init(&p,pal,refresh);
 for(i=0;i<10000;i++){
  unsigned gap=(i%97==0)?257:(i%31==0)?7:1;
  unsigned pause=(i%211)<23,limit=(i%59==0)?0:2;
  refresh=(u16)(refresh+gap);arcade_pacing_observe(&p,refresh,pause);
  if(!pause)active+=gap;
  assert(p.phase==(active*num)%den);
  assert(p.pending+taken==(active*num)/den);
  for(unsigned k=0;k<limit;k++)if(arcade_pacing_take(&p))taken++;
  assert(p.pending+taken==(active*num)/den);cases++;
 }
 while(arcade_pacing_take(&p))taken++;
 assert(taken==(active*num)/den && !p.pending);
 /* Pausing preserves fractional phase and pending work over host wrap. */
 p.pending=17;u32 phase=p.phase;
 arcade_pacing_observe(&p,(u16)(refresh+65535),1);
 assert(p.pending==17 && p.phase==phase);
}
assert(cases==320000);return 0;
}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_pacing.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'observations':320000,'scope':'Real cadence kernel versus64-bit rational elapsed-time oracle; NTSC/PAL, host wrap, irregular gaps, pause, bounded/zero work budgets and retained debt. Kernel-level test only; does not establish live game cadence, input or presentation correctness.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

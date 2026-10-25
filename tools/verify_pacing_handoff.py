#!/usr/bin/env python3
"""Shared scene cadence versus independent 64-bit active-refresh accounting."""
from pathlib import Path
import subprocess,json,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/pacing-handoff';o.mkdir(parents=True,exist_ok=True)
(o/'report.json').unlink(missing_ok=True)
c=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "arcade_pacing.h"
int main(int argc,char **argv){
 assert(argc==2);unsigned pal=atoi(argv[1]),i;u16 refresh=65530;
 uint64_t active=0,taken=0,num=277ULL*3420*(pal?313:262),den=5ULL*(pal?53203424:53693175);
 ArcadePacing *owner=arcade_pacing_enter(pal,refresh);
 for(i=0;i<100000;i++){
  unsigned gap=(i%37==0)?91:1,limit=i%4;
  refresh=(u16)(refresh+gap);active+=gap;
  arcade_pacing_observe(owner,refresh,0);
  for(unsigned k=0;k<limit;k++)taken+=arcade_pacing_take(owner);
  assert(owner->phase==active*num%den);
  assert(owner->pending+taken==active*num/den);
  /* Scene setup is explicitly excluded, preserving both phase and backlog.
   * Enter repeatedly at unchanged counters too (post-game -> title -> game). */
  if(i%11==0){
   u32 phase=owner->phase,pending=owner->pending;
   refresh=(u16)(refresh+17);
   for(unsigned k=0;k<3;k++){
    ArcadePacing *next=arcade_pacing_enter(pal,refresh);
    assert(next==owner && next->phase==phase && next->pending==pending);
    assert(next->last_refresh==refresh);
   }
   /* An independent Home session must not modify Arcade state. */
   ArcadePacing home;arcade_pacing_init(&home,pal,refresh);
   arcade_pacing_observe(&home,(u16)(refresh+100),0);
   assert(owner->phase==phase && owner->pending==pending);
  }
 }
 while(arcade_pacing_take(owner))taken++;
 assert(taken==active*num/den && owner->pending==0);
 return 0;
}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_pacing.c'),'-o',str(o/'test')],check=True)
for pal in (0,1):subprocess.run([str(o/'test'),str(pal)],check=True)
report={'passed':True,'observations':200000,'handoffs':54546,'source_sha256':{n:hashlib.sha256((r/n).read_bytes()).hexdigest() for n in ('src/arcade_pacing.c','inc/arcade_pacing.h')},'scope':'Shared scheduler phase/debt conservation across repeated scene entry, refresh wrap, partial draining and independent Home state, NTSC/PAL; ASan/UBSan. Setup refreshes excluded by explicit policy. Does not prove original boot/queue IRQ timing or target lifecycle integration.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

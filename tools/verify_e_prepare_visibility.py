#!/usr/bin/env python3
"""Differential test of actual E preparation blocks; upload transport is a shim."""
from pathlib import Path
import subprocess, json
r=Path(__file__).resolve().parents[1]
o=r/'out/parity-audit/e-prepare-visibility';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/entity.c').read_text()
a=s.index('        /* Hidden/inactive actors')
b=s.index('\n    }',a)
new=s[a:b]
old=(r/'tools/fixtures/e_prepare_before_visibility_gate.c').read_text()
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_e_pool.h"
#include "arcade_e_render.h"
#include "arcade_native_admission.h"
static ArcadeEPool arcade_e;
static ArcadeNativeAdmission arcade_admission;
static u16 arcade_e_ready[2], requested[2];
static unsigned calls, effective, fail;
u8 arcade_e_upload(const u16 cells[2],u16 bytes,u16 ops){
 assert(bytes==704 && ops==2);calls++;
 if(cells[0]==65535 && cells[1]==65535)return 1;
 effective++;memcpy(requested,cells,sizeof(requested));return !fail;
}
static void before(void){__OLD__}
static void after(void){__NEW__}
int main(void){
 unsigned f0,f1,mask,shape,cases=0,skipped=0;
 for(f0=0;f0<256;f0++)for(f1=0;f1<256;f1++)
 for(mask=0;mask<4;mask++)for(fail=0;fail<2;fail++)for(shape=0;shape<3;shape++){
  u16 expect_ready[2],expect_request[2];unsigned expect_effective,expect_calls;
  arcade_e.actors[0].active=f0;arcade_e.actors[1].active=f1;
  arcade_e.actors[0].code=shape==0?113:shape==1?0:255;
  arcade_e.actors[1].code=shape==0?114:shape==1?3:255;
  arcade_e.actors[0].attr=arcade_e.actors[1].attr=shape==1?7:4;
  arcade_admission.count=0;
  for(unsigned k=0;k<24;k++)if(k!=7 && k!=19)
   arcade_admission.order[arcade_admission.count++]=k;
  if(mask&1)arcade_admission.order[arcade_admission.count++]=74;
  if(mask&2)arcade_admission.order[arcade_admission.count++]=75;
  arcade_e_ready[0]=f0*257u;arcade_e_ready[1]=f1*257u;
  calls=effective=0;requested[0]=requested[1]=65535;before();
  memcpy(expect_ready,arcade_e_ready,sizeof(expect_ready));
  memcpy(expect_request,requested,sizeof(expect_request));
  expect_effective=effective;expect_calls=calls;
  arcade_e_ready[0]=f0*257u;arcade_e_ready[1]=f1*257u;
  calls=effective=0;requested[0]=requested[1]=65535;after();
  assert(!memcmp(expect_ready,arcade_e_ready,sizeof(expect_ready)));
  assert(!memcmp(expect_request,requested,sizeof(expect_request)));
  assert(effective==expect_effective && calls<=expect_calls);
  if(calls<expect_calls){assert((!f0 || f0>=128)&&(!f1 || f1>=128));skipped++;}
  cases++;
 }
 assert(cases==1572864 && skipped>0);return 0;
}
'''.replace('__OLD__',old).replace('__NEW__',new)
(o/'test.c').write_text(c)
files=['arcade_e_render','arcade_e_art','arcade_bonus_art','arcade_cell_sat']
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f'{f}.c') for f in files],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'cases':1572864,'scope':'Actual old/new E preparation blocks, real request/art lookup; all active-byte pairs, four E admission masks, dynamic/resident/invalid poses, upload success/refusal. Identical readiness and nonempty requests. Transport shim; not target timing or VRAM evidence.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

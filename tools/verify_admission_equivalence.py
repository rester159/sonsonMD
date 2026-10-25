#!/usr/bin/env python3
"""Differential check against the saved pre-optimization admission implementation."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/admission-unused-masks';before=o/'before'
assert (before/'arcade_native_admission.c').exists()
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DArcadeNativeAdmission=ReferenceAdmission','-Darcade_native_admission=reference_admission','-I'+str(before),'-I'+str(r/'inc'),'-c',str(before/'arcade_native_admission.c'),'-o',str(o/'reference.o')],check=True)
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_native_admission.h"
typedef struct {u8 entities[6];u16 main;u8 large,children,remaining;u8 order[24],count;} ReferenceAdmission;
void reference_admission(const u8*,const u8*,const u8*,u8,const u8*,const u8*,ReferenceAdmission*);
static u32 seed=7;
static u8 rnd(void){seed=seed*1664525u+1013904223u;return (u8)(seed>>24);}
int main(void){unsigned trial,k;u8 e[48],r[24],m[16],l[3],c[4],reserved;
 for(trial=0;trial<262144;trial++){
  ReferenceAdmission a;ArcadeNativeAdmission b;
  memset(&a,0xa5,sizeof(a));memset(&b,0x5a,sizeof(b));
  for(k=0;k<48;k++){e[k]=rnd();if(trial&1 && (rnd()&3))e[k]=0;}
  for(k=0;k<24;k++)r[k]=rnd()%10;
  for(k=0;k<16;k++){m[k]=rnd();if(trial&1 && (rnd()&3))m[k]=0;}
  for(k=0;k<3;k++)l[k]=(rnd()&1)?rnd():0;
  for(k=0;k<4;k++)c[k]=(rnd()&1)?rnd():0;
  reserved=rnd()%30;
  reference_admission(e,r,m,reserved,l,c,&a);arcade_native_admission(e,r,m,reserved,l,c,&b);
  assert(a.large==b.large && a.children==b.children && a.remaining==b.remaining && a.count==b.count);
  assert(b.count<=24 && !memcmp(a.order,b.order,b.count));
  for(k=0;k<48;k++){u8 found=0;for(unsigned j=0;j<b.count;j++)if(b.order[j]==k)found=1;assert(found==!!(a.entities[k>>3]&(1u<<(k&7))));}
  for(k=0;k<16;k++){u8 found=0;for(unsigned j=0;j<b.count;j++)if(b.order[j]==48+k)found=1;assert(found==!!(a.main&(1u<<k)));}
 }
 return 0;
}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_native_admission.c'),str(o/'reference.o'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'cases':262144,'scope':'Saved before/current native admission differential: ordered identities, all removed-mask memberships, remaining budget and large/K decisions under sanitizers. Includes arbitrary flags, sparse pools, unsupported ranks and exhausted reservations.'}
(o/'equivalence.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

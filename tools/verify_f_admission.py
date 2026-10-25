#!/usr/bin/env python3
"""Independent explicit source-family order and capacity checks for F admission."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/f-admission';o.mkdir(parents=True,exist_ok=True)
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_native_admission.h"
static const u8 prior[]={16,17,18,19,24,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,25,70,71,72,73,26,74,75,27};
static const u8 tail[]={28,0,6,20,29,30};
static void add(ArcadeNativeAdmission *a,u8 id,u8 active){if(active && active<128 && a->remaining){a->order[a->count++]=id;a->remaining--;}}
int main(void){unsigned prefix,reserved,f0,f1,f2,f3,i;const u8 flags[]={0,1,2,64,128,255};
for(prefix=0;prefix<=sizeof prior;prefix++)for(reserved=0;reserved<=25;reserved++)for(f0=0;f0<6;f0++)for(f1=0;f1<6;f1++)for(f2=0;f2<6;f2++)for(f3=0;f3<6;f3++){
 u8 entities[48]={0},ranks[24],main[16]={0},cs[6]={0},ds[4]={0},es[2]={0},fs[4]={flags[f0],flags[f1],flags[f2],flags[f3]},large[3]={1,0,1},children[4]={1,128,2,0};
 ArcadeNativeAdmission got,want;memset(ranks,255,sizeof ranks);memset(&want,0,sizeof want);
 ranks[0]=0;ranks[1]=1;ranks[2]=2;ranks[3]=3;ranks[4]=4;ranks[5]=5;ranks[6]=6;
 want.remaining=reserved<24?24-reserved:0;
 for(i=0;i<prefix;i++){
  u8 id=prior[i];
  if(id<48)entities[id]=1;else if(id<64)main[id-48]=1;else if(id<70)cs[id-64]=1;else if(id<74)ds[id-70]=1;else es[id-74]=1;
  add(&want,id,1);
 }
 for(i=0;i<4;i++)add(&want,76+i,fs[i]);
 for(i=0;i<sizeof tail;i++){entities[tail[i]]=1;add(&want,tail[i],1);}
 for(i=0;i<3;i++)if(large[i]&&want.remaining>=4){want.large|=1<<i;want.remaining-=4;}
 for(i=0;i<4;i++)if(children[i]&&children[i]<128&&want.remaining){want.children|=1<<i;add(&want,12+i,children[i]);}
 arcade_native_admission_cdef(entities,ranks,main,reserved,large,children,cs,ds,es,fs,&got);
 assert(got.count==want.count&&got.remaining==want.remaining&&got.large==want.large&&got.children==want.children);
 assert(!memcmp(got.order,want.order,want.count));
}
return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_native_admission.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'order_capacity_cases':37*26*1296,'scope':'Explicit independent family order, F slot order/flags, prior occupancy, reserved overcapacity, atomic large bodies and shared K tail. Live F pool binding pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

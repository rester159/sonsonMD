#!/usr/bin/env python3
"""Production collision scan prefixes against previous emitted candidate order."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/legacy-collision-scans';o.mkdir(parents=True,exist_ok=True)
old=(r/'tools/fixtures/collision_before_legacy_skip.c').read_text();new=(r/'src/entity.c').read_text()
def extract(s):
 a=s.index('for(i=');a=s.index('for(i=',s.index('f_shot_collisions();'))
 b=s.index('\n    }\n',s.index('next[i-24]=heads[rank]',a))
 heads=s[a:b]
 a=s.index('for (j = ',s.index('arcade_large_contact_target('));b=s.index('        switch (en->type)',a)
 contacts=s[a:b]+'        out[n++]=j;\n    }\n'
 return heads,contacts
c='''#include <assert.h>
#include <string.h>
#include "arcade_types.h"
#define ENTITY_MAX 48
typedef struct {u8 active,owner,type;} Entity;
static Entity g_entities[48];static u8 legacy_pool_used,g_survivalRun;
static const u8 family_rank[15]={5,5,5,5,4,0,5,5,1,3,5,5,2,5,5};
#define ET_POPUP 14
'''
for name,s in [('before',old),('after',new)]:
 heads,contacts=extract(s)
 c+='static unsigned '+name+'(u8*out){u16 i,j;unsigned n=0;u8 rank,heads[6]={48,48,48,48,48,48},next[24];(void)rank;\n'+heads+'\nfor(i=0;i<6;i++)for(j=heads[i];j<48;j=next[j-24])out[n++]=j;\n'+contacts+'return n;}\n'
c+='''int main(void){unsigned trial,k;u32 seed=43;
for(trial=0;trial<100000;trial++){
 u8 a[96],b[96];unsigned na,nb;legacy_pool_used=trial&1;g_survivalRun=(trial>>1)&1;
 for(k=0;k<48;k++){seed=seed*1664525u+1013904223u;g_entities[k].active=(seed>>24)%4;g_entities[k].owner=(seed&4)?128:0;g_entities[k].type=(seed>>16)%16;}
 if(!legacy_pool_used)for(k=24;k<48;k++)g_entities[k].active=0;
 memset(a,255,sizeof a);memset(b,255,sizeof b);na=before(a);nb=after(b);
 assert(na==nb&&!memcmp(a,b,sizeof a));
}return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'candidate_order_cases':100000,'scope':'Actual legacy family-list and contact-prefix source extraction, byte-identical candidate sequence across modes and valid latch states. Does not execute collision effects; only provably empty/excluded candidates are removed.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

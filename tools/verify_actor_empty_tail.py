#!/usr/bin/env python3
"""Production traversal prefix with active-order and same-update allocation observer."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/actor-empty-tail';o.mkdir(exist_ok=True)
prefix=(r/'tools/fixtures/actor_empty_tail_candidate.c').read_text()
source=(r/'src/entity.c').read_text();a=source.index('    /* Enemy AI tracks a LIVE player:');a=source.index('        for (i = 0; i < ENTITY_MAX; i++)',a);b=source.index('            Entity *e = &g_entities[i];',a)
assert source[a:b]==prefix, 'Production actor traversal changed; update differential coverage'
old=(r/'tools/fixtures/actor_empty_tail_baseline.c').read_text()
c='''#include <assert.h>
#include <string.h>
#include <stdint.h>
#define ENTITY_MAX 48
static unsigned g_survivalRun,legacy_pool_used,active[48],order[48],used,birth_at,birth_slot;
static void step(unsigned i){
 if(!active[i])return;
 order[used++]=i;active[i]++;
 if(i==birth_at){active[birth_slot]=1;if(birth_slot>=24)legacy_pool_used=1;}
}
'''
for name,body in [('before',old),('after',prefix)]:c+='static void '+name+'(void){unsigned i;'+body+'step(i);}}\n'
c+='''int main(void){unsigned mode,used_before,at,slot,k,expected[48],expected_order[48],initial[48],count,latch;uint32_t seed=1;
for(mode=0;mode<2;mode++){for(used_before=0;used_before<2;used_before++){for(at=0;at<49;at++){for(slot=0;slot<48;slot++){
 for(k=0;k<48;k++){seed=seed*1664525u+1013904223u;initial[k]=(k<24||used_before)?seed%3:0;}
 if(at<24)initial[at]=1;
 g_survivalRun=mode;birth_at=at;birth_slot=slot;
 memcpy(active,initial,sizeof active);memset(order,0,sizeof order);used=0;legacy_pool_used=used_before;before();
 memcpy(expected,active,sizeof active);memcpy(expected_order,order,sizeof order);count=used;latch=legacy_pool_used;
 memcpy(active,initial,sizeof active);memset(order,0,sizeof order);used=0;legacy_pool_used=used_before;after();
 assert(used==count && legacy_pool_used==latch && !memcmp(active,expected,sizeof active) && !memcmp(order,expected_order,sizeof order));
}}}}return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'cases':2*2*49*48,'scope':'Actual old/new traversal prefixes with modeled active updates and same-update births, all birth slots and trigger positions, both modes and prior allocation-latch values. Compares state, active update order and latch. Does not replace real actor behavior tests.'}
(o/'equivalence.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

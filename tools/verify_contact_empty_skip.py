#!/usr/bin/env python3
"""Compare actual contact-loop prefixes, including allocation during traversal."""
from pathlib import Path
import json
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'out/parity-audit/contact-empty-skip'
out.mkdir(parents=True, exist_ok=True)
reference = (root/'tools/fixtures/collision_before_contact_bound.c').read_text()
production = (root/'src/entity.c').read_text()
assert 'if(g_survivalRun || legacy_pool_used)\n    /* Ordinary' in production
start=production.index('    /* Ordinary slots0..15')
end=production.index('    }   /* end per-player loop */',start)
assert 'case ET_SHOT:' not in production[start:end]
assert 'if(i>=24)legacy_pool_used=1;' in production
sources = [reference, production]
c = '''#include <assert.h>
#include <string.h>
#include "arcade_types.h"
#define ENTITY_MAX 48
typedef struct {u8 active,owner,type;} Entity;
static Entity g_entities[48];
static u8 g_survivalRun,legacy_pool_used,allocate_at,allocate_slot;
'''
for name, source in zip(('before', 'after'), sources):
    start = source.index('for (j = ', source.index('arcade_large_contact_target('))
    end = source.index('        switch (en->type)', start)
    c += 'static unsigned '+name+'(u8 *sequence){u16 j;unsigned n=0;\n'
    if name=='after': c += 'if(g_survivalRun || legacy_pool_used){\n'
    c += source[start:end]
    c += '''if(en->type==99)continue; /* ET_SHOT: original switch has no case/effect. */
        sequence[n++]=(u8)j;
        /* Model a collection allocating a later or earlier legacy slot. */
        if(j==allocate_at){legacy_pool_used=1;g_entities[allocate_slot].active=1;g_entities[allocate_slot].owner=0;}
    }
'''
    if name=='after':c+='}\n'
    c+='return n;}\n'
c += '''int main(void){unsigned trial,k;u32 seed=71;
for(trial=0;trial<200000;trial++){
 Entity saved[48],expected[48];u8 a[48],b[48],latch,final_latch;unsigned na,nb;
 g_survivalRun=trial&1;legacy_pool_used=(trial>>1)&1;latch=legacy_pool_used;
 for(k=0;k<48;k++){seed=seed*1664525u+1013904223u;g_entities[k].active=(seed>>24)&3;g_entities[k].owner=(seed&8)?128:0;g_entities[k].type=0;}
 if(!g_survivalRun)for(k=16;k<20;k++)g_entities[k].type=99;
 if(!legacy_pool_used)for(k=24;k<48;k++)g_entities[k].active=0;
 allocate_at=(u8)(trial%48);allocate_slot=(u8)(24+(trial/48)%24);
 /* Exercise the latch changing inside a previously empty-range scan. */
 if(trial%3==0){allocate_at=16;g_entities[16].active=1;g_entities[16].owner=0;}
 memcpy(saved,g_entities,sizeof saved);memset(a,255,sizeof a);memset(b,255,sizeof b);
 na=before(a);memcpy(expected,g_entities,sizeof expected);final_latch=legacy_pool_used;
 memcpy(g_entities,saved,sizeof saved);legacy_pool_used=latch;nb=after(b);
 assert(na==nb&&!memcmp(a,b,sizeof a));
 assert(final_latch==legacy_pool_used&&!memcmp(expected,g_entities,sizeof expected));
}return 0;}
'''
(out/'test.c').write_text(c)
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                '-I'+str(root/'inc'),str(out/'test.c'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
report = {'cases':200000,'scope':'Actual old/new contact-loop prefixes, with reserved player-shot no-op cases; valid allocation-latch states, both modes, ordered candidates and state after in-loop allocations. Collision effects are modeled, not full arcade execution.'}
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))

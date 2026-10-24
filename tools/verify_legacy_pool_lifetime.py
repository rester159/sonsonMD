#!/usr/bin/env python3
"""Exercise production allocation prefix and reset clear for legacy scan ownership."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/legacy-pool-lifetime';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/entity.c').read_text();a=s.index('static Entity *entity_spawn_range(');b=s.index('        e->type',a)
allocation=s[a:b]+'        (void)type;(void)x;(void)y;return e;\n    }\n    return 0;\n}\n'
a=s.index('static void entity_reset(');b=s.index('    legacy_pool_used=0;',a)+len('    legacy_pool_used=0;')
reset=s[a:b]+'\n    (void)cold;\n}\n'
c='''#include <assert.h>
#include <string.h>
#include "arcade_types.h"
#define ENTITY_MAX 48
typedef struct {u8 active;} Entity;
static Entity g_entities[48];static u8 legacy_pool_used;
'''+allocation+reset+'''
int main(void){unsigned first,end,free_slot,used,k;
for(first=0;first<=48;first++)for(end=first;end<=48;end++)for(free_slot=0;free_slot<=48;free_slot++)for(used=0;used<2;used++){
 Entity *got;int expected=-1;
 for(k=0;k<48;k++)g_entities[k].active=(k==free_slot)?0:128;
 legacy_pool_used=used;
 if(free_slot>=first&&free_slot<end)expected=free_slot;
 got=entity_spawn_range(0,0,0,first,end);
 assert(got==(expected<0?0:&g_entities[expected]));
 assert(legacy_pool_used==(used||(expected>=24)));
 if(expected>=0)assert(got->active==1);
 entity_reset(0);assert(!legacy_pool_used);
 for(k=0;k<48;k++)assert(!g_entities[k].active);
}
return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'allocation_reset_cases':1225*49*2,'scope':'Actual production free-slot search/activation prefix and reset-clear prefix; all valid allocation ranges, free-slot positions and prior latch states. Excludes per-type initialization, whose later code does not change the monotonic latch.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

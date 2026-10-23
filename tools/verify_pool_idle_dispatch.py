#!/usr/bin/env python3
"""Production pool loop dispatch across all active bytes, preserving slot order."""
from pathlib import Path
import re,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/pool-idle-dispatch';o.mkdir(parents=True,exist_ok=True)
def function(s,name):
 a=s.index('void '+name+'(');b=s.index('{',a);depth=1;i=b+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
reports=[]
for family,n in [('c',6),('d',4),('e',2),('f',4)]:
 name=f'arcade_{family}_pool_update';old=function((r/f'tools/fixtures/{family}_pool_before_idle_guard.c').read_text(),name).replace(name,'before');new=function((r/f'src/arcade_{family}_pool.c').read_text(),name)
 actor=(r/f'inc/arcade_{family}_actor.h').read_text();proto=re.search(r'void arcade_'+family+r'_actor_step\([^;]+',actor).group()
 args=new.split('(',1)[1].split(')',1)[0].split(',');args=[re.findall(r'\w+',arg)[-1]for arg in args]
 actual={arg:('&p' if arg=='p' else '&next' if arg=='next' else '&available' if arg=='available' else '0')for arg in args}
 call=','.join(actual[arg]for arg in args)
 c=f'''#include <assert.h>
#include <string.h>
#include "arcade_{family}_pool.h"
static unsigned calls,used;static u8 order[6];static Arcade{family.upper()}Actor *base;
u8 arcade_{family}_contact_presence;
'''+proto+'''{
 calls++;if(!actor->active)return;
 order[used++]=(u8)(actor-base);actor->x++;
 *bonus_next=(u8)(*bonus_next+actor->active);
}
'''+old+'\n'+new+f'''
int main(void){{unsigned value,slot,k;u8 next,expected_next;
'''+('u8 available=0;\n'if family=='e' else '')+f'''
for(value=0;value<256;value++)for(slot=0;slot<{n};slot++){{
 Arcade{family.upper()}Pool p,initial,expected;u8 expected_order[6];unsigned expected_used;
 memset(&p,0x5a,sizeof p);
 for(k=0;k<{n};k++)p.actors[k].active=(k==slot)?value:(k&1?0:128);
 initial=p;base=p.actors;next=3;calls=used=0;memset(order,0,sizeof order);
 before({call});assert(calls=={n});expected=p;expected_next=next;expected_used=used;memcpy(expected_order,order,sizeof order);
 p=initial;next=3;calls=used=0;memset(order,0,sizeof order);
 {name}({call});assert(calls==expected_used&&used==expected_used);
 assert(!memcmp(&p,&expected,sizeof p)&&next==expected_next&&!memcmp(order,expected_order,sizeof order));
}}
return 0;
}}
'''
 (o/f'{family}.c').write_text(c)
 subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/f'{family}.c'),'-o',str(o/family)],check=True)
 subprocess.run([str(o/family)],check=True)
 reports.append({'family':family,'dispatch_cases':256*n})
report={'families':reports,'scope':'Actual old/new pool loops with an actor-call observer: every active byte and slot, hidden actors retained, identical active order and effects; zero calls removed. Real actor lifecycle fixtures run separately.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

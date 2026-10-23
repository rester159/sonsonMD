#!/usr/bin/env python3
"""Verify pool presence summaries across every active-byte transition."""
from pathlib import Path
import subprocess,re,json,argparse
r=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--source-dir',type=Path,default=r/'src');args=p.parse_args();o=r/'out/parity-audit/pool-contact-presence/host';o.mkdir(parents=True,exist_ok=True)
def function(s,name):
 a=s.index('void '+name+'(');b=s.index('{',a);depth=1;i=b+1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
reports=[]
for family,n,typ in [('main',16,'Main'),('c',6,'C'),('d',4,'D'),('e',2,'E'),('f',4,'F')]:
 name=f'arcade_{family}_pool_update';presence=f'arcade_{family}_contact_presence'
 new=function((args.source_dir/f'arcade_{family}_pool.c').read_text(),name)
 assert presence in new
 old=new.replace(name,'before').replace(presence+'=0;','').replace(presence+'|=a->active;','').replace(presence+'|=p->actors[k].active;','')
 header=(r/f'inc/arcade_{family}_actor.h').read_text();protos=[re.search(r'void arcade_'+family+r'_actor_step\([^;]+',header).group()]
 if family=='main':protos.append(re.search(r'void arcade_main_bonus_step\([^;]+',header).group())
 c=f'''#include <assert.h>
#include <string.h>
#include "arcade_{family}_pool.h"
static unsigned used;static u8 order[16],after;static Arcade{typ}Actor *base;
u8 {presence};
'''
 for proto in protos:
  actor='a' if family=='main' else 'actor'
  c+=proto+'{order[used++]=(u8)('+actor+'-base);'+actor+'->x++;'+actor+'->active=after;}\n'
 c+=old+'\n'+new
 params=new.split('(',1)[1].split(')',1)[0].split(',');names=[re.findall(r'\w+',x)[-1] for x in params]
 call=','.join('&p' if x=='p' else '&next' if x=='next' else '&available' if x=='available' else '0' for x in names)
 c+=f'''int main(void){{unsigned v,w,slot,k;u8 next=0;'''+('u8 available=0;' if family=='e' else '')+f'''
for(v=0;v<256;v++)for(w=0;w<256;w++)for(slot=0;slot<{n};slot++){{
 Arcade{typ}Pool p,initial,expected;u8 expected_order[16],any=0;unsigned expected_used;
 memset(&p,0,sizeof p);p.actors[slot].active=v;p.actors[slot].state=(w&1)?255:0;initial=p;base=p.actors;after=w;
 used=0;memset(order,0,sizeof order);before({call});expected=p;expected_used=used;memcpy(expected_order,order,sizeof order);
 p=initial;used=0;memset(order,0,sizeof order);{presence}=255;{name}({call});
 assert(!memcmp(&p,&expected,sizeof p)&&used==expected_used&&!memcmp(order,expected_order,sizeof order));
 for(k=0;k<{n};k++)any|=p.actors[k].active;assert({presence}==any);
}}
}}
'''
 (o/f'{family}.c').write_text(c);subprocess.run(['cc','-O2','-I'+str(r/'inc'),str(o/f'{family}.c'),'-o',str(o/family)],check=True);subprocess.run([str(o/family)],check=True)
 reports.append({'family':family,'cases':65536*n})
report={'passed':True,'families':reports,'scope':'Actual summary update loops, every byte active-before/after and each slot; observer callbacks establish dispatch/state equivalence and exact post-update presence. Full lifecycle and same-tick spawn/contact ordering require target replay.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

#!/usr/bin/env python3
"""Native sixteen-slot lifecycle and contact integration, without Genesis I/O."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/main-pool';o.mkdir(parents=True,exist_ok=True)
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_main_pool.h"
int main(void){unsigned pattern,k,owner;u8 next;
for(pattern=0;pattern<256;pattern++){
 ArcadeMainPool p,expected;ArcadeRng rng={0,0};
 memset(&p,pattern,sizeof(p));expected=p;
 arcade_main_pool_reset(&p,0);
 for(k=0;k<16;k++)expected.actors[k].active=0;
 expected.live=0;assert(!memcmp(&p,&expected,sizeof(p)));
 memset(&p.wave,0,sizeof(p.wave));p.wave.control.active=1;
 assert(!arcade_main_pool_schedule(&p,12,0,0,0,65500,&rng));
 assert(p.wave.control.state==1 && !p.wave.control.phase);
 expected=p;
 assert(arcade_main_pool_schedule(&p,12,0,0,0,65500,&rng)==12);
 for(k=0;k<16;k++){
  ArcadeMainActor a=expected.actors[k];
  if(k<12){a.x=212;a.y=arcade_main_placements[0][k][0];a.variant=arcade_main_placements[0][k][1];a.delay=p.actors[k].delay;a.direction=0;a.active=128;a.state=a.phase=0;}
  assert(!memcmp(&a,&p.actors[k],sizeof(a)));
 }
 assert(!p.live);next=0;arcade_main_pool_update(&p,65500,1,0,0,&next,10);
 assert(p.live==12);
 for(k=0;k<12;k++)assert(p.actors[k].hp==5 && p.actors[k].state==1 && p.actors[k].active==130);
 for(k=0;k<12;k++){p.actors[k].state=5;p.actors[k].phase=2;p.actors[k].hit=0;}
 arcade_main_pool_update(&p,65500,2,0,0,&next,10);assert(!p.live);
 for(k=0;k<16;k++)assert(!p.actors[k].active);
 arcade_main_pool_reset(&p,1);memset(&expected,0,sizeof(expected));assert(!memcmp(&p,&expected,sizeof(p)));
}
for(pattern=0;pattern<256;pattern++)for(owner=0;owner<2;owner++){
 ArcadeMainActor a[16]={0};ArcadeBonusAward award;u8 slot;
 a[3].active=pattern;a[3].x=65535;a[3].y=255;a[3].state=255;
 a[9].active=1;a[9].x=65535;a[9].y=255;
 slot=arcade_main_contact_target(a,0,0,1);
 assert(slot==((pattern && !(pattern&2))?3:9));
 assert(arcade_main_contact_target(a,0,0,2)==255);
 for(k=0;k<11;k++){
  a[3].hit=k;a[3].phase=1;
  assert(arcade_main_bonus_collect(&a[3],owner,0x3ee0,&award));
  assert(a[3].active==2 && a[3].phase==2 && a[3].owner==owner);
  assert(award.points==(k==10?20000:(k+1)*1000));assert(award.combat==0x3ee0 && award.sound==5);
  assert(award.event==(0x400|(k==10?0x2a:k+9)|(owner<<7)));
 }
}
return 0;}
'''
(o/'test.c').write_text(c)
files=('arcade_main_pool.c','arcade_main_actor.c','arcade_main_actor_data.c','arcade_main_bonus.c','arcade_main_collision.c','arcade_main_wave.c','arcade_main_wave_data.c','arcade_waves.c','arcade_formations.c','arcade_difficulty.c','arcade_bonus_art.c')
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f)for f in files],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'pool_sequences':256,'contact_cases':512,'bonus_awards':5632,'scope':'Native controller/pool/actor integration: reserve twelve, initialize/live count, death cleanup, warm/cold reset retention and first-contact/bonus awards. Host only; main game binding/rendering pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

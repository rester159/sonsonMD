#!/usr/bin/env python3
"""Four-slot D controller/actor/combat lifecycle integration (host only)."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/d-pool';o.mkdir(parents=True,exist_ok=True)
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_d_pool.h"
#include "arcade_d_combat.h"
int main(void){unsigned pattern,k,owner;u8 next;
for(pattern=0;pattern<256;pattern++)for(owner=0;owner<2;owner++){
 ArcadeDPool p,expected;unsigned total=0;u16 combat=0;
 memset(&p,pattern,sizeof p);expected=p;arcade_d_pool_reset(&p,0);
 for(k=0;k<4;k++)expected.actors[k].active=0;
 expected.live=0;assert(!memcmp(&p,&expected,sizeof p));
 memset(&p.wave,0,sizeof p.wave);p.wave.control.active=1;p.wave.control.parameter=1;
 for(k=0;k<30;k++)assert(!arcade_d_pool_schedule(&p,pattern,0,65500,(u16)(65500+64)));
 assert(p.wave.reservation==4&&p.wave.control.state==1&&!p.live);
 expected=p;assert(arcade_d_pool_schedule(&p,pattern,0,65500,(u16)(65500+64))==4);
 for(k=0;k<4;k++){
  ArcadeDActor a=expected.actors[k];
  a.x=212;a.y=arcade_d_placements[pattern>>5][k][0];a.delay=arcade_d_placements[pattern>>5][k][1];
  a.kind=0;a.direction=9;a.target_offset=144;a.active=128;a.state=a.phase=0;
  assert(!memcmp(&a,&p.actors[k],sizeof a));
 }
 assert(!p.live);next=0;arcade_d_pool_update(&p,65500,0,0,0,0,65500,100,0,&next,10);assert(p.live==4);
 /* Live actors freeze the producer countdown, including after all six kills. */
 expected=p;assert(!arcade_d_pool_schedule(&p,pattern,0,65500,(u16)(65500+64)));assert(!memcmp(&p,&expected,sizeof p));
 for(k=0;k<4;k++){
  ArcadePlayerShot shot={0};ArcadeDHit h;
  arcade_d_hit(&p.actors[k],&shot,owner,&p.wave.reservation,combat,&h);
  total+=h.points+h.bonus_points;combat=h.combat;
  assert(p.actors[k].phase==(k==3?4:0));assert(p.live==4);
 }
 assert(total==900&&combat==5*0x40&&!p.wave.reservation);
 expected=p;assert(!arcade_d_pool_schedule(&p,pattern,0,65500,(u16)(65500+64)));assert(!memcmp(&p,&expected,sizeof p));
 arcade_d_pool_update(&p,65500,0,0,0,0,65500,100,0,&next,10);
 for(k=0;k<4;k++)assert(p.actors[k].phase==1&&p.actors[k].timer==32);
 for(k=0;k<32;k++)arcade_d_pool_update(&p,65500,0,0,0,0,65500,100,0,&next,10);
 assert(p.live==4);for(k=0;k<4;k++)assert(p.actors[k].phase==2);
 arcade_d_pool_update(&p,65500,0,0,0,0,65500,100,0,&next,10);
 assert(p.live==4);for(k=0;k<4;k++)assert(p.actors[k].active==255);
 arcade_d_pool_update(&p,65500,0,0,0,0,65500,100,0,&next,10);
 assert(!p.live);for(k=0;k<4;k++)assert(!p.actors[k].active);
 assert(!arcade_d_pool_schedule(&p,pattern,0,65500,(u16)(65500+64)));assert(p.wave.timer==29);
 arcade_d_pool_reset(&p,1);memset(&expected,0,sizeof expected);assert(!memcmp(&p,&expected,sizeof p));
}
return 0;}
'''
(o/'test.c').write_text(c)
files=('arcade_d_pool.c','arcade_d_actor.c','arcade_d_actor_data.c','arcade_d_motion.c','arcade_d_motion_data.c','arcade_d_wave.c','arcade_d_wave_data.c','arcade_d_combat.c','arcade_difficulty.c','arcade_bonus_art.c')
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f)for f in files],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'pool_lifecycles':512,'scope':'Controller births, retained residuals, actor live counts, four combat awards, delayed cleanup, next wave and reset contracts. Host only; game binding and raw stop-selector domain pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

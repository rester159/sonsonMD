#!/usr/bin/env python3
"""F controller/actor/combat lifecycle integration; windowless host fixtures."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/f-pool';o.mkdir(parents=True,exist_ok=True)
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_f_pool.h"
#include "arcade_f_combat.h"
int main(void){unsigned pattern,owner,k,t;
for(pattern=0;pattern<256;pattern++)for(owner=0;owner<2;owner++){
 ArcadeFPool p,expected;u8 next=0;u16 combat=0;unsigned total=0;
 memset(&p,pattern,sizeof p);expected=p;arcade_f_pool_reset(&p,0);
 for(k=0;k<4;k++)expected.actors[k].active=0;
 expected.live=0;assert(!memcmp(&p,&expected,sizeof p));
 memset(&p.wave,0,sizeof p.wave);p.wave.control.active=1;p.wave.control.parameter=1;
 for(t=1;t<32;t++)assert(!arcade_f_pool_schedule(&p,t,pattern,65500));
 assert(p.wave.control.wait==1&&!p.wave.control.state);
 assert(!arcade_f_pool_schedule(&p,32,pattern,65500));
 assert(p.wave.control.state==1&&!p.live&&p.wave.reservation==4);
 expected=p;assert(arcade_f_pool_schedule(&p,33,pattern,65500)==4);
 for(k=0;k<4;k++){
  ArcadeFActor a=expected.actors[k];
  const u8 *b=arcade_f_placements[(pattern&48)>>4][(pattern&14)>>1][k];
  a.x=(u16)(65500+((pattern&32)?-8:248));a.y=a.origin_y=b[0];
  a.delay=b[1];a.direction=(pattern&32)?1:5;
  a.active=128;a.state=a.phase=0;assert(!memcmp(&a,&p.actors[k],sizeof a));
 }
 assert(!p.live);
 arcade_f_pool_update(&p,65500,1,0,&next,10);
 assert(p.live==4);
 for(k=0;k<4;k++){
  u8 delay=arcade_f_placements[(pattern&48)>>4][(pattern&14)>>1][k][1];
  assert(p.actors[k].delay==(delay?delay-1:0));
  assert(p.actors[k].state==(delay?0:2));
  assert(p.actors[k].phase==(delay?1:0));
 }
 expected=p;assert(!arcade_f_pool_schedule(&p,64,pattern,65500));assert(!memcmp(&p,&expected,sizeof p));
 /* Overlap all four to establish first-hit priority and independent reservation. */
 for(k=0;k<4;k++){p.actors[k].x=100;p.actors[k].y=128;p.actors[k].active=1;p.actors[k].state=2;}
 for(k=0;k<4;k++){
  ArcadePlayerShot shot={0};ArcadeFHit h;shot.active=1;
  assert(arcade_f_shot_target(p.actors,100,128,1)==k);
  arcade_f_hit(&p.actors[k],&shot,owner,&p.wave.reservation,combat,&h);
  total+=h.points+h.bonus_points;combat=h.combat;
  assert(h.points==50&&h.bonus_points==(k==3?500:0));
  assert(p.wave.reservation==3-k&&p.live==4);
 }
 assert(total==700&&combat==268);
 arcade_f_pool_update(&p,0,0,0,&next,10);
 for(k=0;k<4;k++)assert(p.actors[k].phase==1&&p.actors[k].origin_y==32);
 for(t=0;t<32;t++)arcade_f_pool_update(&p,0,0,0,&next,10);
 assert(p.live==4);for(k=0;k<4;k++)assert(p.actors[k].phase==2&&p.actors[k].active==2);
 arcade_f_pool_update(&p,0,0,0,&next,10);
 assert(p.live==4);for(k=0;k<4;k++)assert(p.actors[k].phase==3&&p.actors[k].active==255);
 arcade_f_pool_update(&p,0,0,0,&next,10);
 assert(!p.live);for(k=0;k<4;k++)assert(!p.actors[k].active);
 assert(!arcade_f_pool_schedule(&p,64,pattern,65500));assert(p.wave.reservation==4);
 assert(arcade_f_pool_schedule(&p,65,pattern,65500)==4);
 arcade_f_pool_reset(&p,1);memset(&expected,0,sizeof expected);assert(!memcmp(&p,&expected,sizeof p));
}
return 0;}
'''
(o/'test.c').write_text(c)
files=('arcade_f_pool.c','arcade_f_actor.c','arcade_f_actor_data.c','arcade_f_motion.c','arcade_f_motion_data.c','arcade_f_wave.c','arcade_f_wave_data.c','arcade_f_combat.c','arcade_difficulty.c','arcade_bonus_art.c')
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f)for f in files],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'pool_lifecycles':512,'scope':'Warm/cold reset, retained birth fields, phased countdown, initialization fallthrough, live/reservation ownership, four first-hit kills and 700-point clear, hidden cleanup and respawn. Sanitized host integration; live binding pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

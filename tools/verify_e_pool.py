#!/usr/bin/env python3
"""E controller/actor/combat integration; host lifecycle fixtures, no emulator."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/e-pool';o.mkdir(parents=True,exist_ok=True)
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_e_pool.h"
#include "arcade_e_combat.h"
int main(void){unsigned pattern,owner,k,t;
for(pattern=0;pattern<256;pattern++)for(owner=0;owner<2;owner++){
 ArcadeEPool p,expected;u8 available=pattern,next=0;u16 combat=0;unsigned total=0;
 memset(&p,pattern,sizeof p);expected=p;arcade_e_pool_reset(&p,0);
 for(k=0;k<2;k++)expected.actors[k].active=0;
 expected.live=0;assert(!memcmp(&p,&expected,sizeof p));
 memset(&p.wave,0,sizeof p.wave);p.wave.active=1;p.wave.parameter=1;
 /* Unaligned clock holds wait until phase zero; final decrement defers birth. */
 for(t=1;t<32;t++)assert(!arcade_e_pool_schedule(&p,t,pattern,65500));
 assert(p.wave.wait==1&&!p.wave.state);
 assert(!arcade_e_pool_schedule(&p,32,pattern,65500));assert(p.wave.state==1&&!p.live);
 expected=p;assert(arcade_e_pool_schedule(&p,33,pattern,65500)==2);
 for(k=0;k<2;k++){
  ArcadeEActor a=expected.actors[k];
  a.x=212;a.y=arcade_e_placements[(pattern&56)>>3][k][0];
  a.delay=arcade_e_placements[(pattern&56)>>3][k][1];a.direction=9;
  a.active=128;a.state=a.phase=0;assert(!memcmp(&a,&p.actors[k],sizeof a));
 }
 assert(!p.live);assert(available==pattern);
 arcade_e_pool_update(&p,65500,34,0,0,100,100,owner,1,&available,&next,10);
 assert(p.live==2&&available==(u8)(pattern-2));
 for(k=0;k<2;k++){
  assert(p.actors[k].phase==1&&p.actors[k].turns==9);
  assert(p.actors[k].delay==arcade_e_placements[(pattern&56)>>3][k][1]);
 }
 expected=p;assert(!arcade_e_pool_schedule(&p,64,pattern,65500));assert(!memcmp(&p,&expected,sizeof p));
 /* Inject an overlapping pair to exercise E's two hits from one shot. */
 {
  ArcadePlayerShot shot={0};u8 mask;
  p.actors[0].y=p.actors[1].y=128;shot.active=1;
  mask=arcade_e_shot_targets(p.actors,212,128,shot.active);assert(mask==3);
  for(k=0;k<2;k++)if(mask&(1<<k)){
   ArcadeEHit h;arcade_e_hit(&p.actors[k],&shot,owner,combat,&h);
   total+=h.points;combat=h.combat;
  }
 }
 assert(total==600&&combat==152&&p.live==2&&available==(u8)(pattern-2));
 arcade_e_pool_update(&p,65500,0,0,0,100,100,owner,1,&available,&next,10);
 for(k=0;k<2;k++)assert(p.actors[k].phase==1&&p.actors[k].timer==32);
 for(t=0;t<32;t++)arcade_e_pool_update(&p,65500,t,0,0,100,100,owner,1,&available,&next,10);
 assert(p.live==2);for(k=0;k<2;k++)assert(p.actors[k].phase==2&&p.actors[k].active==2);
 /* E clears immediately on the next update; no hidden FF phase. */
 arcade_e_pool_update(&p,65500,32,0,0,100,100,owner,1,&available,&next,10);
 assert(!p.live&&available==pattern);for(k=0;k<2;k++)assert(!p.actors[k].active);
 assert(!arcade_e_pool_schedule(&p,64,pattern,65500));assert(p.wave.state==1);
 assert(arcade_e_pool_schedule(&p,65,pattern,65500)==2);
 arcade_e_pool_reset(&p,1);memset(&expected,0,sizeof expected);assert(!memcmp(&p,&expected,sizeof p));
}
return 0;}
'''
(o/'test.c').write_text(c)
files=('arcade_e_pool.c','arcade_e_actor.c','arcade_e_actor_data.c','arcade_e_motion.c','arcade_e_motion_data.c','arcade_e_wave.c','arcade_e_wave_data.c','arcade_e_combat.c','arcade_difficulty.c','arcade_bonus_art.c')
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f)for f in files],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'pool_lifecycles':512,'scope':'Retained birth fields, clock-phased countdown, initialization, shared counter wrapping, double-hit awards, death cleanup, next wave and reset APIs. Host fixtures only; live source A4 ownership and graphics remain pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

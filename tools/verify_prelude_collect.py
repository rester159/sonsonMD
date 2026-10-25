#!/usr/bin/env python3
"""Prelude source-order contact and shared pickup lifetime regression."""
from pathlib import Path
import json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/prelude-collect';o.mkdir(parents=True,exist_ok=True)
(o/'test.c').write_text('''#include <assert.h>
#include "arcade_prelude.h"
#include "arcade_prelude_render.h"
static s16 floor_probe(u16 x,u8 y,void *ctx){(void)x;(void)y;(void)ctx;return 1;}
int main(void){
 for(int dx=-5;dx<=5;dx++)for(int dy=-9;dy<=9;dy++)for(unsigned owner=0;owner<2;owner++){
  ArcadePrelude s={0};ArcadePreludeRewards r={0};ArcadeBonusAward events[2];
  arcade_prelude_decor_begin(&s);
  s.players[owner].active=1;s.players[owner].x=92+dx;s.players[owner].y=180+dy;
  unsigned hit=dx>=-3&&dx<=3&&dy>=-7&&dy<=7;
  assert(arcade_prelude_collect(&s,&r,events)==hit);
  if(hit){assert(s.decor[0].active==2 && s.decor[0].owner==owner);assert(events[0].points==9000);assert(events[0].event==(0x411|(owner<<7)));assert(events[0].sound==5);assert(r.collected==1);}
  else assert(s.decor[0].active==1 && r.collected==0);
 }
 for(unsigned active=1;active<256;active++){
  ArcadePrelude s={0};ArcadePreludeRewards r={0};ArcadeBonusAward events[2];
  arcade_prelude_decor_begin(&s);s.decor[1].x=92;
  for(unsigned i=0;i<2;i++){s.players[i].active=active;s.players[i].x=92;s.players[i].y=180;}
  r.collected=7;
  assert(arcade_prelude_collect(&s,&r,events)==2);
  assert(s.decor[0].owner==0 && s.decor[1].owner==0);
  assert(events[0].points==9000 && events[1].points==10000);
  assert(r.collected==1 && r.spawn.active==1 && r.spawn.phase==0 && r.spawn.placement==104 && r.conversion.kind==255);
  assert(arcade_prelude_collect(&s,&r,events)==0);
 }
 for(unsigned owner=0;owner<2;owner++){
  ArcadePrelude s={0};ArcadePreludeRewards r={0};ArcadeBonusAward events[2];
  arcade_prelude_decor_begin(&s);s.players[owner].active=1;s.players[owner].x=92;s.players[owner].y=180;
  assert(arcade_prelude_collect(&s,&r,events)==1);
  arcade_attract_prelude_init(&s.input);
  /* Collection follows B19A; its score pose begins on the next source tick. */
  for(unsigned tick=0;tick<96;tick++){
   assert(arcade_prelude_step(&s,tick,floor_probe,0,0));
   assert(s.decor[0].active==2);
   assert(s.decor[0].attr==(owner?0xc3:0xc4));
   assert(s.decor[0].timer==95-tick);
   if(!tick){
    ArcadePrelude display=s;u16 sat[48];const ArcadeSpriteView view={0,1,256,224};
    display.players[0].active=display.players[1].active=display.decor[1].active=0;
    const ArcadeBonusFrame *pose=&arcade_bonus_frames[arcade_bonus_frames[arcade_bonus_roots[8]].next];
    assert(arcade_prelude_sat(&display,&view,sat,0,12)==1);
    assert((sat[2]&0x7ff)==(owner?pose->owner2_tile:pose->tile));
   }
  }
  assert(s.decor[0].state==3);
  assert(arcade_prelude_step(&s,96,floor_probe,0,0));assert(s.decor[0].active==0);
 }
 return 0;
}
''')
sources=['arcade_prelude.c','arcade_player.c','arcade_attract.c','arcade_attract_data.c','arcade_bonus_art.c','arcade_pickup.c','arcade_rewards.c','arcade_difficulty.c','arcade_prelude_render.c','arcade_cell_sat.c','arcade_life_art.c']
subprocess.run(['cc','-std=c99','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f)for f in sources],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'passed':True,'geometry_owner_cases':11*19*2,'active_priority_cases':255,'score_lifetime_cases':2,
 'scope':'Prelude contact boundaries and P1-first ownership, slot-ordered events, every nonzero player-active byte, collection-counter wrap/FB95 request, next-tick score pose and96-update retirement. Shared source-derived pickup/reward implementation reused; no live title/original full-scene differential.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

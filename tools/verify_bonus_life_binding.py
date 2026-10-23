#!/usr/bin/env python3
"""Test the actual game bonus-life bridge and byte-compatible reserves."""
from pathlib import Path
import subprocess,json,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/bonus-life-binding';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/game.c').read_text();a=s.index('static u8 bonus_life_dip(void)');b=s.index('/* FPS-readout',a);body=s[a:b]
assert '{game_score_add(&g_game.players[k],event.points[k]);}' in s
assert 'check_extra_life(&g_game.players[k])' not in s
assert 'u8 reserves=player_reserves(p),gate=0,sound;' in s
assert 'if(event==(u16)(0xc00|k))player_set_reserves(p,reserves);' in s
c='''#include <assert.h>
#include "arcade_bonus_life.h"
#define MAX_PLAYERS 2
#define FLOW_GAME_OVER 3
#define SND_CMD_1UP 4
#define SND_1UP_ENABLE 1
typedef struct{u32 score;u8 which,lives,bonusNextIdx,flow;}Player;
static u8 g_survivalRun,g_attract,clock;
static struct{u8 bonusLife;}g_config;
static unsigned sounds;
static u8 entity_arcade_clock(void){return clock;}
static void snd_play_1up(void){sounds++;}
'''+body+'''
int main(void){
 Player p={0};unsigned cases=0;
 for(unsigned reserves=0;reserves<256;reserves++)for(unsigned owner=0;owner<2;owner++){
  p.which=owner;player_set_reserves(&p,reserves);assert(player_reserves(&p)==reserves);
  assert(p.lives==(u8)(reserves+1));assert(lives_256[owner]==(reserves==255));cases++;
 }
 for(unsigned dip=0;dip<4;dip++)for(unsigned owner=0;owner<2;owner++){
  p.which=owner;p.flow=0;player_set_reserves(&p,2);g_config.bonusLife=dip;
  arcade_bonus_life_begin(&bonus_life[owner],dip);sounds=0;
  p.score=(dip&1)?30000:20000;
  for(unsigned tick=1;tick<8;tick++){clock=owner*8+tick;check_extra_life(&p);assert(sounds==0);}
  clock=(1-owner)*8;check_extra_life(&p);assert(sounds==0);
  clock=owner*8;g_attract=1;check_extra_life(&p);assert(sounds==0);g_attract=0;
  g_survivalRun=1;check_extra_life(&p);assert(sounds==0);g_survivalRun=0;
  p.flow=FLOW_GAME_OVER;check_extra_life(&p);assert(sounds==0);p.flow=0;
  check_extra_life(&p);assert(sounds==1&&p.lives==4&&p.bonusNextIdx==1);
  check_extra_life(&p);assert(sounds==1);
  p.score=(dip&1)?90000:80000;check_extra_life(&p);assert(sounds==(dip<2?1:2));
  p.score=(dip&1)?210000:180000;check_extra_life(&p);assert(sounds==(dip<2?1:3));
  p.score=(dip&1)?330000:280000;check_extra_life(&p);assert(sounds==(dip<2?1:4));cases++;
 }
 /* FCA1 reserve255 increments modulo256, retaining the current life. */
 p.which=0;p.flow=0;p.score=20000;g_config.bonusLife=0;clock=0;
 arcade_bonus_life_begin(&bonus_life[0],0);player_set_reserves(&p,255);
 check_extra_life(&p);assert(p.lives==1&&!lives_256[0]&&player_reserves(&p)==0);
 /* Reserve254 + bonus =255; death consumes one ->254, retaining255 lives. */
 arcade_bonus_life_begin(&bonus_life[0],0);player_set_reserves(&p,254);
 check_extra_life(&p);assert(p.lives==0&&lives_256[0]);
 player_set_reserves(&p,(u8)(player_reserves(&p)-1));assert(p.lives==255&&!lives_256[0]);
 assert(cases==520);return 0;
}
'''
(o/'test.c').write_text(c);subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_bonus_life.c'),'-o',str(o/'test')],check=True);subprocess.run([str(o/'test')],check=True)
report={'reserve_roundtrips':512,'mode_player_scenarios':8,'reserve_rollover_scenarios':2,'game_source_sha256':hashlib.sha256(s.encode()).hexdigest(),'scope':'Extracted production bridge with mocked sound/clock: raw DIPs, alternating phase, live/out/Survival gates, repeated thresholds beyond third award, byte reserve rollover. Checks production death bridge and absence of results immediate bonus call. Host, not full gameplay qualification.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

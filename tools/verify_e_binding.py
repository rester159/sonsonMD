#!/usr/bin/env python3
"""Production E shot adapter and required live wiring, host only."""
from pathlib import Path
import subprocess,json,re
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/e-binding';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/entity.c').read_text();a=s.index('static void e_shot_collisions(');b=s.index('static void f_shot_collisions(',a)
adapter=s[a:b]
score_helper=re.search(r'static inline void game_score_add\(.*?\n\}',(r/'inc/game.h').read_text(),re.S).group()
presence='arcade_e_contact_presence'
setup=score_helper+'\n'
if presence in adapter:
 setup+='u8 '+presence+';\n'
 wrapper='static void fixture_shots(void){unsigned k;'+presence+'=0;for(k=0;k<2;k++)'+presence+'|=arcade_e.actors[k].active;e_shot_collisions();}\n'
else:wrapper='static void fixture_shots(void){e_shot_collisions();}\n'
adapter=setup+adapter+'\n'+wrapper
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_e_pool.h"
#include "arcade_e_combat.h"
typedef struct {u16 x,y;u8 active,phase,fireTimer,state;} Entity;
static Entity g_entities[48];
typedef struct {u32 score;} Player;
static struct {Player players[2];} g_game;
static u8 g_attract;
static ArcadeEPool arcade_e;
static u8 wave_clock,sounds[4],sound_count;
static u16 g_arcadeCombatDifficulty;
static void snd_command(u8 n){assert(sound_count<4);sounds[sound_count++]=n;}
__ADAPTER__
int main(void){unsigned owner,k,separate;
for(owner=0;owner<2;owner++)for(separate=0;separate<2;separate++){
 memset(&arcade_e,0,sizeof arcade_e);memset(&g_game,0,sizeof g_game);memset(g_entities,0,sizeof g_entities);
 wave_clock=owner;g_arcadeCombatDifficulty=0;arcade_e.live=2;sound_count=0;
 for(k=0;k<2;k++){
  ArcadeEActor*a=&arcade_e.actors[k];Entity*shot=&g_entities[16+owner*2+k];
  a->active=1;a->x=100+k*separate*40;a->y=100;
  shot->active=1;shot->x=a->x;shot->y=139;
 }
 e_shot_collisions();
 assert(g_game.players[owner].score==600&&!g_game.players[1-owner].score&&g_arcadeCombatDifficulty==152);
 assert(sound_count==2&&sounds[0]==12&&sounds[1]==12&&arcade_e.live==2);
 for(k=0;k<2;k++)assert(arcade_e.actors[k].active==2&&arcade_e.actors[k].state==3&&!arcade_e.actors[k].phase&&arcade_e.actors[k].owner==owner);
 assert(g_entities[16+owner*2].active==2);
 assert(g_entities[17+owner*2].active==(separate?2:1));
}
return 0;}
'''.replace('e_shot_collisions();','fixture_shots();').replace('__ADAPTER__',adapter)
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_e_combat.c'),str(r/'src/arcade_difficulty.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
for token in ['changed!=SS_WAVE_BIT_CLASSE','arcade_e_pool_reset(&arcade_e,cold)','arcade_e_pool_schedule(&arcade_e','arcade_e_pool_update(&arcade_e','arcade_e_contact_target(arcade_e.actors','arcade_e_bonus_collect(a,pk','input.state.e[k]=arcade_e.actors[k].active','arcade_e_upload(cells,704,2)','arcade_e.actors[i].x+=dx']:
 assert token in s,token
assert s.index('        e_shot_collisions();')>s.index('        d_shot_collisions();')
report={'adapter_sequences':4,'scope':'Production adapter: both owners, overlapping double kill or two separate hits, score/difficulty/sound order, no early live-count release. Static lifecycle/contact/upload wiring checks. Runtime E gameplay/performance not yet checked.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

#!/usr/bin/env python3
"""Production restart and player adapters with stubbed hardware/entity owners."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/course-player-binding';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/game.c').read_text()
def function(name):
 a=s.index(name);b=s.index('{',a);depth=1;e=b+1
 while depth:
  depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
h=(r/'inc/game.h').read_text();end=h.index('} Player;')+len('} Player;');start=h.rfind('typedef struct',0,end)
prefix=r'''
#include <assert.h>
#include <string.h>
#include "arcade_player.h"
#include "arcade_section.h"
#include "arcade_results.h"
typedef struct {u8 opaque[16];} Anim;
#define FALSE 0
#define TRUE 1
#define FLOW_GAME_OVER 3
#define FLOW_ALIVE 0
#define PV_RUN 0
#define PV_CLIMB_UP 1
#define PV_CLIMB_DOWN 2
#define PV_FALL 3
'''+h[start:end]+r'''
static struct {Player players[2];s16 cameraX;u8 numPlayers,stage,round;} g_game;
static ArcadePlayer player_motion[2];static ArcadePlayerAnimation player_animation[2];
static ArcadeSection g_arcadeSection;static ArcadeResults g_results;
static u8 life_owned[2],life_path[2],g_scrollSub,g_scrollColumnLatch,g_scrollColumnAdvance;
static u8 g_arcadeCombatDifficulty,g_foodRound,g_gameOverBeat,g_resultsActive,g_resultsDisplay,g_resultsPlayers,hud_valid,ribbon_plane_phase;
static unsigned refills;
static u16 game_player_target_flags(u8 k){return g_game.players[k].flow==FLOW_GAME_OVER?0:256;}
static void VDP_setEnable(u8 v){(void)v;}
static void DMA_flushQueue(void){}
static void arcade_results_display_restore(void){}
static void level_refill(void){refills++;}
static void entity_course_restart(u8 two,const u8 out[2]){(void)two;(void)out;}
static void arcade_food_world_begin(void){}
static void arcade_food_bonus_reset(void){}
static void arcade_release_background_reset(void){}
static void arcade_food_world_render(u8 p,u8 cold){(void)p;(void)cold;}
static void arcade_finish_background_reset(void){}
static void snd_command(u8 n){assert(n==0x12);}
'''
suffix=r'''
int main(void){unsigned seed,mask,two,k,cases=0;
for(seed=0;seed<256;seed++)for(mask=0;mask<4;mask++)for(two=0;two<2;two++){
 Player before[2];
 memset(&g_game,0,sizeof(g_game));g_game.numPlayers=two+1;g_game.cameraX=22016;
 for(k=0;k<2;k++){
  Player*p=&g_game.players[k];memset(p,seed,sizeof(*p));
  p->x=777+k;p->y=80+k;p->xSub=seed;p->ySub=255-seed;p->which=k;
  p->flow=(mask&(1<<k))?FLOW_GAME_OVER:FLOW_ALIVE;
  player_motion[k]=(ArcadePlayer){0};player_motion[k].previous=seed;
  player_animation[k]=(ArcadePlayerAnimation){k,9,8,77,88};
  life_owned[k]=life_path[k]=2;before[k]=*p;
 }
 refills=0;game_course_restart();assert(refills==1 && g_game.cameraX==0);
 for(k=0;k<2;k++){
  Player*p=&g_game.players[k];u8 eligible=!(mask&(1<<k)) && (!k || two);
  if(!eligible){assert(memcmp(p,&before[k],sizeof(*p))==0);assert(life_owned[k]==2 && life_path[k]==2);}
  else{
   assert(p->x==32 && p->y==(s16)(135-32*k) && p->facing==0);
   assert(p->xSub==seed && p->ySub==255-seed);
   assert(p->score==before[k].score && p->lives==before[k].lives);
   assert(p->firePrev==before[k].firePrev && p->which==k);
   assert(life_owned[k]==0 && life_path[k]==0);
   assert(player_animation[k].cursor==0 && player_animation[k].timer==0);
  }
  assert(player_motion[k].previous==seed);
  assert(player_animation[k].code==77 && player_animation[k].attr==88);
 }
 cases++;
}assert(cases==2048);return 0;}
'''
(o/'test.c').write_text(prefix+'\n'+function('static void motion_load(')+'\n'+function('static void motion_store(')+'\n'+function('static void game_course_restart(')+suffix)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_player_course.c'),str(r/'src/arcade_section.c'),str(r/'src/arcade_section_data.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'cases':2048,'scope':'Actual game_course_restart, motion adapters, player and section setup; graphics/entity reset stubbed. Excluded public player states unchanged; eligible positions, fractional/history preservation, score/lives and animation retention. Not full runtime parity.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

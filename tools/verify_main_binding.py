#!/usr/bin/env python3
"""Production main-shot adapter and resident renderer, with host game/I/O shims."""
from pathlib import Path
import subprocess,json,re
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/main-binding';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/entity.c').read_text();a=s.index('static void main_shot_collisions(');b=s.index('static void c_shot_collisions(',a)
adapter=s[a:b]
score_helper=re.search(r'static inline void game_score_add\(.*?\n\}',(r/'inc/game.h').read_text(),re.S).group()
presence='arcade_main_contact_presence'
setup=score_helper+'\n'
if presence in adapter:
 setup+='u8 '+presence+';\n'
 wrapper='static void fixture_shots(void){unsigned k;'+presence+'=0;for(k=0;k<16;k++)'+presence+'|=arcade_main.actors[k].active;main_shot_collisions();}\n'
else:wrapper='static void fixture_shots(void){main_shot_collisions();}\n'
adapter=setup+adapter+'\n'+wrapper
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_main_pool.h"
#include "arcade_main_render.h"
#include "arcade_main_art.h"
#include "arcade_bonus_art.h"
typedef struct {u16 x,y;u8 active,phase,fireTimer,state;} Entity;
static Entity g_entities[48];
typedef struct {u32 score;} Player;
static struct {Player players[2];} g_game;
static u8 g_attract;
static ArcadeMainPool arcade_main;
static u8 wave_clock,sounds[4],sound_count;
static u16 g_arcadeCombatDifficulty;
static void snd_command(u8 n){assert(sound_count<4);sounds[sound_count++]=n;}
__ADAPTER__
int main(void){unsigned owner,hits,k,x,flags;u16 sat[80*4];
for(owner=0;owner<2;owner++){
 memset(&arcade_main,0,sizeof(arcade_main));memset(&g_game,0,sizeof(g_game));
 ArcadeMainActor *a=&arcade_main.actors[0];
 a->x=100;a->y=100;a->active=1;a->hp=5;
 wave_clock=owner;g_arcadeCombatDifficulty=0;
 for(hits=1;hits<=5;hits++){
  memset(g_entities,0,sizeof(g_entities));sound_count=0;
  Entity *shot=&g_entities[16+owner*2];shot->active=1;shot->x=100;shot->y=139;
  main_shot_collisions();assert(a->hp==5-hits);
  assert(shot->active==2 && shot->phase==2 && !shot->fireTimer && shot->state==1);
  assert(sounds[0]==7 && sound_count==(hits==5?2:1));
  assert(g_game.players[owner].score==(hits==5?500:0));assert(!g_game.players[1-owner].score);
 }
 assert(sounds[1]==6 && a->owner==owner && a->state==5 && !a->phase);
 assert(g_arcadeCombatDifficulty==0x33);
 /* First shot kills slot0; second shot must continue to slot1. */
 memset(g_entities,0,sizeof(g_entities));a->active=1;a->hp=1;a->state=2;
 arcade_main.actors[1]=*a;arcade_main.actors[1].hp=5;sound_count=0;
 for(k=0;k<2;k++){Entity *shot=&g_entities[16+owner*2+k];shot->active=1;shot->x=100;shot->y=139;}
 main_shot_collisions();assert(!a->hp && arcade_main.actors[1].hp==4);
 assert(sound_count==3 && sounds[0]==7 && sounds[1]==6 && sounds[2]==7);
}
/* Exact stored normal/bonus poses, both palettes, all flag bytes, wrapping and bounded writes. */
for(k=0;k<50;k++)for(owner=0;owner<2;owner++)for(flags=0;flags<256;flags++)for(x=0;x<2;x++){
 ArcadeMainActor actors[16]={0};ArcadeMainActor*a=&actors[0];u16 tile,n;
 if(k<28){const ArcadeMainFrame*f=&arcade_main_frames[k];a->code=f->code;a->attr=f->attr;tile=arcade_main_tile(a->code,a->attr);}
 else{const ArcadeBonusFrame*f=&arcade_bonus_frames[k-28];a->code=f->code;a->attr=f->attr;tile=f->tile;if(owner && f->owner2_tile!=65535){a->attr=0xc3;tile=f->owner2_tile;}}
 a->active=flags;a->x=x?250:100;a->y=100;
 for(unsigned j=0;j<320;j++)sat[j]=0xbeef;
 n=arcade_main_render(actors,0,sat,2,5);
 if(!flags || (flags&128))assert(n==0);
 else{
  assert(tile!=65535 && n==(x?2:1));
  assert((sat[2*4+2]&2047)==tile && sat[2*4]==267);
  assert(sat[2*4+3]==(x?378:228));
  if(x)assert(sat[3*4+3]==122 && (sat[3*4+2]&2047)==tile);
 }
 for(unsigned j=0;j<8;j++)assert(sat[j]==0xbeef);
 for(unsigned j=(2+n)*4;j<320;j++)assert(sat[j]==0xbeef);
 for(unsigned j=0;j<320;j++)sat[j]=0xbeef;
 assert(arcade_main_render_masked(actors,0,0,sat,2,5)==0);
 for(unsigned j=0;j<320;j++)assert(sat[j]==0xbeef);
 assert(arcade_main_render(actors,0,sat,2,2)==0);
 for(unsigned j=0;j<320;j++)assert(sat[j]==0xbeef);
}
return 0;}
'''.replace('main_shot_collisions();','fixture_shots();').replace('__ADAPTER__',adapter)
(o/'test.c').write_text(c)
files=('arcade_main_collision','arcade_difficulty','arcade_main_render','arcade_main_art','arcade_main_actor_data','arcade_bonus_art','arcade_cell_sat')
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f'{f}.c')for f in files],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'adapter_hit_sequences':4,'render_cases':50*2*256*2,'scope':'Production shot adapter with game/sound shims: both owners, five-hit reward, shot ordering and first-free target progression. Real renderer: all normal/bonus poses, flags, palette ownership, X wrap and capacity guards under sanitizers. Not full target gameplay/source equivalence.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

#!/usr/bin/env python3
"""Compile production SAT emitters with game/upload shims and inspect linked order."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/front-sprite-chain';o.mkdir(parents=True,exist_ok=True)
g=(r/'src/game.c').read_text();g=g[g.index('u16 game_render_players('):g.index('void game_render(void)')]
e=(r/'src/entity.c').read_text();e=e[e.index('u16 entity_render_all('):]
old=(r/'tools/fixtures/entity_render_before_mode_snapshot.c').read_text().replace('entity_render_all','reference_render_all')
f=(r/'src/arcade_food_bonus.c').read_text();f=f[f.index('static u16 ordinary_sat('):]
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_main_pool.h"
#include "arcade_c_pool.h"
#include "arcade_d_pool.h"
#include "arcade_d_render.h"
#include "arcade_f_pool.h"
#include "arcade_f_render.h"
#include "arcade_e_pool.h"
#include "arcade_e_render.h"
#include "arcade_e_upload.h"
#include "arcade_c_render.h"
#include "arcade_main_render.h"
#include "arcade_native_admission.h"
#include "arcade_large_present.h"
#include "arcade_k.h"
#include "arcade_orb.h"
#include "arcade_bonus_art.h"
#include "arcade_powerup_upload.h"
#define MAX_PLAYERS 2
#define ENTITY_MAX 48
#define ENTITY_SAT_MAX 48
#define ENTITY_POPUP_GLYPH_MAX 10
#define POPUP_GLYPH_MAX 10
#define BODY_W 16
#define BODY_H 16
#define SCREEN_W 256
#define SCREEN_H 224
#define ET_POPUP 14
#define ET_SHOT 2
#define ARCADE_BONUS_POSE 32768
#define TILE_FONT_INDEX 1440
#define PAL1 1
#define PAL3 3
#define SPRITE_SIZE(w,h) (((w)-1)*4+(h)-1)
#define TILE_ATTR_FULL(p,pr,v,h,t) (((p)<<13)|((pr)<<15)|((v)<<12)|((h)<<11)|(t))
#define FLOW_GAME_OVER 4
#define FLOW_DYING 2
#define FLOW_RESPAWNING 3
#define BLINK_MASK 2
typedef struct {s16 x,y;u16 flowTimer,tile;u8 flow,hurt,facing;} Player;
static struct {Player players[2];s16 cameraX;u8 numPlayers;} g_game;
static u8 g_survivalRun,life_owned[2],player_active[2],fail_carrier;
static u16 game_player_target_flags(u8 k){return (u16)player_active[k]<<8;}
typedef struct {s16 x,y;u16 tile;u8 active,type,facing,popupLen;char popupStr[8];} Entity;
static Entity g_entities[48];
static ArcadeMainPool arcade_main;
static ArcadeCPool arcade_c;
static ArcadeDPool arcade_d;
static ArcadeEPool arcade_e;
static ArcadeFPool arcade_f;
static u16 arcade_e_ready[2];
u8 arcade_e_upload(const u16 cells[2],u16 bytes,u16 ops){(void)cells;assert(bytes==704&&ops==2);return 1;}
static ArcadeNativeAdmission arcade_admission;
static ArcadeFoe arcade_foes[12];
const ArcadeFoeFrame arcade_foe_frames[1]={{0,600,1,0xc0,0}};
static ArcadeFoeWeapon arcade_weapons[4];
static ArcadeLargeActor arcade_large_actors[3];
static ArcadeKProjectile arcade_k_actors[4];
static u16 large_last_sat[128],large_last_count,large_last_visible;
static u8 large_last_resident;
static ArcadePowerup arcade_food_pickup,arcade_food_conversion;
static ArcadeOrb arcade_food_carrier;
static void entity_sprite_admission(void){} /* Feed the tested admission contract. */
u16 arcade_powerup_art_cell(u8 code,u8 attr){(void)code;(void)attr;return 0;}
u8 arcade_powerup_upload(const u16 cells[3],u16 bytes,u16 ops){assert(bytes==512 && ops==1);return !(fail_carrier && cells[2]!=65535);}
u16 arcade_large_present_mixed(const ArcadeLargeCell cells[16],u16 visible,const u16*resident,const ArcadeSpriteView*v,u16*sat,u16 first,u16 limit,u16 bytes,u16 ops){(void)resident;(void)cells;(void)v;(void)sat;(void)first;(void)limit;(void)bytes;(void)ops;assert(!visible);return 0;}
__PLAYERS__
__SPECIALS__
__ENTITIES__
int main(void){unsigned flags,fail;u16 sat[80*4],expected[16];
for(flags=0;flags<4096;flags++)for(fail=0;fail<2;fail++){
 unsigned n=0,i;u16 got;
 memset(&arcade_admission,0,sizeof(arcade_admission));memset(g_entities,0,sizeof(g_entities));memset(&arcade_main,0,sizeof(arcade_main));memset(&arcade_c,0,sizeof(arcade_c));memset(&arcade_d,0,sizeof(arcade_d));memset(&arcade_e,0,sizeof(arcade_e));memset(&arcade_f,0,sizeof(arcade_f));
 g_game.numPlayers=2;g_game.cameraX=0;g_survivalRun=0;fail_carrier=fail;
 arcade_food_carrier.active=(flags&1)!=0;arcade_food_carrier.x=100;arcade_food_carrier.y=100;arcade_food_carrier.attr=0xc0;
 arcade_food_pickup.active=(flags&2)!=0;arcade_food_pickup.x=100;arcade_food_pickup.y=100;arcade_food_pickup.code=226;arcade_food_pickup.attr=209;
 arcade_food_conversion.active=(flags&4)!=0;arcade_food_conversion.x=100;arcade_food_conversion.y=100;arcade_food_conversion.attr=0xc0;
 if((flags&1) && !fail)expected[n++]=276;
 if(flags&2)expected[n++]=136;
 if(flags&4)expected[n++]=272;
 for(i=0;i<2;i++){
  if(flags&(8u<<i)){Entity*a=&g_entities[16+i];a->active=1;a->type=2;a->x=a->y=100;a->tile=1000+i;arcade_admission.order[arcade_admission.count++]=16+i;expected[n++]=1000+i;}
  player_active[i]=(flags&(32u<<i))!=0;g_game.players[i].x=g_game.players[i].y=100;g_game.players[i].tile=1100+i;
 }
 for(i=0;i<2;i++)if(player_active[i])expected[n++]=1100+i;
 if(flags&128){
  ArcadeMainActor*a=&arcade_main.actors[0];a->active=1;a->x=a->y=100;a->code=136;a->attr=0xca;arcade_admission.order[arcade_admission.count++]=48;expected[n++]=450;
 }
 if(flags&256){ArcadeCActor*a=&arcade_c.actors[0];a->active=1;a->x=a->y=100;a->code=19;a->attr=0xc5;arcade_admission.order[arcade_admission.count++]=64;expected[n++]=1;}
 if(flags&512){ArcadeDActor*a=&arcade_d.actors[0];a->active=1;a->x=a->y=100;a->code=16;a->attr=0x99;arcade_admission.order[arcade_admission.count++]=70;expected[n++]=1921;}
 if(flags&1024){ArcadeEActor*a=&arcade_e.actors[0];a->active=1;a->x=a->y=100;a->code=113;a->attr=4;arcade_admission.order[arcade_admission.count++]=74;expected[n++]=1945;}
 if(flags&2048){ArcadeFActor*a=&arcade_f.actors[0];a->active=1;a->x=a->y=100;a->code=183;a->attr=0x8f;arcade_admission.order[arcade_admission.count++]=76;expected[n++]=398;}
 if(flags&128){
  g_entities[0].active=1;g_entities[0].x=g_entities[0].y=100;g_entities[0].tile=1200;arcade_admission.order[arcade_admission.count++]=0;expected[n++]=1200;
 }
 for(i=0;i<320;i++)sat[i]=0xbeef;
 got=entity_render_all(sat,0);assert(got==n);
 if(!(flags&2048)){u16 reference[320];for(i=0;i<320;i++)reference[i]=0xbeef;assert(reference_render_all(reference,0)==got);assert(!memcmp(reference,sat,sizeof reference));}
 for(i=0;i<n;i++){assert((sat[i*4+2]&2047)==expected[i]);assert((sat[i*4+1]&127)==(i+1<n?i+1:0));}
 for(i=n*4;i<320;i++)assert(sat[i]==0xbeef);
}
/* Survival's direct pool and popup path must remain byte-identical too. */
for(unsigned trial=0;trial<256;trial++){
 u16 reference[320];g_survivalRun=1;g_game.cameraX=trial;
 memset(g_entities,0,sizeof g_entities);
 for(unsigned i=0;i<48;i++){
  Entity*a=&g_entities[i];a->active=(trial+i)%3;a->type=(trial+i)%15;
  a->x=(s16)(trial+(i*17)%290-17);a->y=(s16)((i*31+trial)%260-18);
  a->tile=300+i;a->facing=i&1;a->popupLen=4;memcpy(a->popupStr,"1234",4);
 }
 for(unsigned i=0;i<320;i++)sat[i]=reference[i]=0xbeef;
 assert(entity_render_all(sat,2)==reference_render_all(reference,2));
 assert(!memcmp(sat,reference,sizeof reference));
}
/* Appending players respects capacity and retains the Survival player slots. */
for(unsigned mode=0;mode<2;mode++)for(unsigned first=0;first<8;first++){
 g_survivalRun=mode;player_active[0]=player_active[1]=0;g_game.numPlayers=2;
 assert(game_render_players(sat,first,first)==0);
 assert(game_render_players(sat,first,first+1)==(mode?1:0));
}
return 0;}
'''.replace('__PLAYERS__',g).replace('__SPECIALS__',f).replace('__ENTITIES__',old+'\n'+e)
(o/'test.c').write_text(c)
files=['arcade_f_render','arcade_f_art','arcade_e_render','arcade_e_art','arcade_d_render','arcade_d_art','arcade_c_render','arcade_c_art','arcade_main_render','arcade_main_art','arcade_bonus_art','arcade_cell_sat','arcade_large_render']
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f'{x}.c')for x in files],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'chain_cases':8192,'player_capacity_cases':32,'survival_differential_cases':256,'scope':'Production player/special/entity SAT emitters with game state, admission and upload shims. Actual tile and next-link order, absent players, deferred carrier upload, empty chain and sentinel bounds. Does not verify source player lifecycle/pixels or large/shared-K ordering.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

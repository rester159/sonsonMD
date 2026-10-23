#!/usr/bin/env python3
"""Compare production enemy-update target arguments and world-player snapshots."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/enemy-target-snapshot';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/entity.c').read_text().replace('if(!survival)', 'if(!g_survivalRun)');a=s.index('    if(!g_survivalRun)\n',s.index('void entity_update_mode(') if 'void entity_update_mode(' in s else s.index('void entity_update_all('));b=s.index('    /* Enemy AI tracks a LIVE player:',a)
old=(r/'tools/fixtures/enemy_target_before_snapshot.c').read_text()
c='''#include <assert.h>
#include <string.h>
#include "arcade_tracking.h"
typedef struct {s16 x,y;} Player;
typedef struct {u16 x;u8 y,active,kind;} ArcadeFoePlayer;
static struct {Player players[2];s16 cameraX;u8 numPlayers;} g_game;
static struct {u16 camera;u8 clock,stage,tracking,scroll_stopped,purge,shot_budget;ArcadeFoePlayer players[2];} arcade_world;
static u8 g_survivalRun,wave_clock,arcade_food_conversion_pulse,arcade_shot_budget,arcade_main_ready;
static u16 g_arcadeCombatDifficulty,flags[2],queries;
static u16 dx,ex;static u8 dy,dt,ey,et,eligible;
static u16 game_player_target_flags(u8 k){assert(k<2);queries++;return flags[k];}
static u8 game_arcade_stopped(void){return 0;}
#define arcade_main_pool_update(...) ((void)0)
#define arcade_c_pool_update(...) ((void)0)
#define arcade_f_pool_update(...) ((void)0)
#define arcade_d_pool_update(pool,cam,clock,diff,stop,purge,x,y,target,next,limit) do{dx=x;dy=y;dt=target;}while(0)
#define arcade_e_pool_update(pool,cam,clock,stop,purge,x,y,target,allow,available,next,limit) do{ex=x;ey=y;et=target;eligible=allow;*available=7;}while(0)
'''
for name,body in [('before',old),('after',s[a:b])]:c+='static void '+name+'(void){u8 pk;\n'+body+'}\n'
c+='''int main(void){u32 seed=37;unsigned trial,k;
for(trial=0;trial<200000;trial++){
 u16 expected[7];ArcadeFoePlayer players[2];
 for(k=0;k<2;k++){seed=seed*1664525u+1013904223u;flags[k]=seed>>16;g_game.players[k].x=seed;g_game.players[k].y=seed>>8;}
 g_game.numPlayers=1+(trial&1);g_survivalRun=0;queries=0;before();assert(queries==5);
 expected[0]=dx;expected[1]=dy;expected[2]=dt;expected[3]=ex;expected[4]=ey;expected[5]=et;expected[6]=eligible;
 memcpy(players,arcade_world.players,sizeof players);queries=0;after();assert(queries==2);
 assert(dx==expected[0]&&dy==expected[1]&&dt==expected[2]&&ex==expected[3]&&ey==expected[4]&&et==expected[5]&&eligible==expected[6]);
 assert(!memcmp(players,arcade_world.players,sizeof players));assert(arcade_shot_budget==1&&arcade_world.shot_budget==1);
 g_survivalRun=1;queries=0;after();assert(!queries);
}return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'target_context_cases':200000,'queries_before':5,'queries_after':2,'scope':'Production update block with native pool call shims: target coordinates/index/eligibility and both world-player flag snapshots identical. Pool ownership audited separately; no whole-game timing/parity claim.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

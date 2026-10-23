#!/usr/bin/env python3
"""Production C shot adapter, independent admission priority and wiring checks."""
from pathlib import Path
import subprocess,json,re
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/c-binding';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/entity.c').read_text();a=s.index('static void c_shot_collisions(');b=s.index('static void d_shot_collisions(',a)
adapter=s[a:b]
score_helper=re.search(r'static inline void game_score_add\(.*?\n\}',(r/'inc/game.h').read_text(),re.S).group()
presence='arcade_c_contact_presence'
setup=score_helper+'\n'
if presence in adapter:
 setup+='u8 '+presence+';\n'
 wrapper='static void fixture_shots(void){unsigned k;'+presence+'=0;for(k=0;k<6;k++)'+presence+'|=arcade_c.actors[k].active;c_shot_collisions();}\n'
else:wrapper='static void fixture_shots(void){c_shot_collisions();}\n'
adapter=setup+adapter+'\n'+wrapper
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_c_pool.h"
#include "arcade_c_combat.h"
#include "arcade_native_admission.h"
typedef struct {u16 x,y;u8 active,phase,fireTimer,state;} Entity;
static Entity g_entities[48];
typedef struct {u32 score;} Player;
static struct {Player players[2];} g_game;
static u8 g_attract;
static ArcadeCPool arcade_c;
static u8 wave_clock,sounds[4],sound_count;
static u16 g_arcadeCombatDifficulty;
static void snd_command(u8 n){assert(sound_count<4);sounds[sound_count++]=n;}
__ADAPTER__
int main(void){unsigned owner,k,mask,reserved;
for(owner=0;owner<2;owner++){
 memset(&arcade_c,0,sizeof arcade_c);memset(&g_game,0,sizeof g_game);
 wave_clock=owner;g_arcadeCombatDifficulty=0;arcade_c.wave.reservation=6;arcade_c.live=6;
 for(k=0;k<6;k++){ArcadeCActor*a=&arcade_c.actors[k];a->active=1;a->x=a->y=100;}
 for(k=0;k<3;k++){
  unsigned shot;memset(g_entities,0,sizeof g_entities);sound_count=0;
  for(shot=0;shot<2;shot++){Entity*s=&g_entities[16+owner*2+shot];s->active=1;s->x=100;s->y=139;}
  c_shot_collisions();
  assert(arcade_c.wave.reservation==4-k*2&&arcade_c.live==6);
  assert(sound_count==(k==2?3:2)&&sounds[0]==13&&sounds[1]==13);
  if(k==2)assert(sounds[2]==14);
  for(shot=0;shot<2;shot++){Entity*s=&g_entities[16+owner*2+shot];assert(s->active==2&&s->phase==2&&!s->fireTimer&&s->state==1);}
 }
 assert(g_game.players[owner].score==1300&&!g_game.players[1-owner].score&&g_arcadeCombatDifficulty==7*0x33);
 for(k=0;k<6;k++)assert(arcade_c.actors[k].owner==owner&&arcade_c.actors[k].phase==(k==5?4:0));
}
/* All C flag combinations with capacity crossing main/C and C/later boundaries. */
for(mask=0;mask<4096;mask++)for(reserved=0;reserved<=24;reserved++){
 u8 e[48]={0},r[24],m[16]={0},large[3]={0},children[4]={0},cs[6],expected[24],n=0,left=reserved<24?24-reserved:0;
 ArcadeNativeAdmission got;memset(r,255,sizeof r);
 e[16]=1;m[0]=1;e[24]=1;r[0]=2;e[0]=1;
 for(k=0;k<6;k++){static const u8 flags[4]={0,1,64,128};cs[k]=flags[(mask>>(k*2))&3];}
 if(left){expected[n++]=16;left--;}if(left){expected[n++]=48;left--;}
 for(k=0;k<6;k++)if(cs[k]&&cs[k]<128&&left){expected[n++]=64+k;left--;}
 if(left){expected[n++]=24;left--;}if(left){expected[n++]=0;left--;}
 arcade_native_admission_c(e,r,m,reserved,large,children,cs,&got);
 assert(got.count==n&&got.remaining==left&&!memcmp(expected,got.order,n));
}
return 0;}
'''.replace('c_shot_collisions();','fixture_shots();').replace('__ADAPTER__',adapter);(o/'test.c').write_text(c)
files=['arcade_c_combat','arcade_difficulty','arcade_native_admission']
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f'{f}.c')for f in files],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
# Verify live ownership without treating these static checks as runtime evidence.
assert s.index('main_shot_collisions();')<s.index('c_shot_collisions();')<s.index('if(!survival && pass==5)')
assert 'changed!=SS_WAVE_BIT_CLASSC' in s
assert s.index('arcade_main_pool_update(')<s.index('arcade_c_pool_update(')
assert s.index('if(!survival && i>=64)')<s.index('if(!survival && i>=48)')
g=(r/'src/game.c').read_text();assert g.index('VDP_loadTileData(ss_scene_tiles')<g.index('VDP_loadTileData(arcade_c_pixels')
report={'adapter_complete_clear_sequences':2,'admission_priority_cases':4096*25,'scope':'Production shot adapter, source-family sprite priority and bounded capacity. Static live wiring checks supplement host tests; target gameplay still needs runtime evidence.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

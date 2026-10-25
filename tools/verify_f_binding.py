#!/usr/bin/env python3
"""Production F shot adapter, independent admission priority and wiring checks."""
from pathlib import Path
import subprocess,json,re
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/f-binding';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/entity.c').read_text();a=s.index('static void f_shot_collisions(');b=s.index('/* H then G',a)
adapter=s[a:b]
score_helper=re.search(r'static inline void game_score_add\(.*?\n\}',(r/'inc/game.h').read_text(),re.S).group()
presence='arcade_f_contact_presence'
setup=score_helper+'\n'
if presence in adapter:
 setup+='u8 '+presence+';\n'
 wrapper='static void fixture_shots(void){unsigned k;'+presence+'=0;for(k=0;k<4;k++)'+presence+'|=arcade_f.actors[k].active;f_shot_collisions();}\n'
else:wrapper='static void fixture_shots(void){f_shot_collisions();}\n'
adapter=setup+adapter+'\n'+wrapper
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_f_pool.h"
#include "arcade_f_combat.h"
#include "arcade_native_admission.h"
typedef struct {u16 x,y;u8 active,phase,fireTimer,state;} Entity;
static Entity g_entities[48];
typedef struct {u32 score;} Player;
static struct {Player players[2];} g_game;
static u8 g_attract;
static ArcadeFPool arcade_f;
static u8 wave_clock,sounds[4],sound_count;
static u16 g_arcadeCombatDifficulty;
static void snd_command(u8 n){assert(sound_count<4);sounds[sound_count++]=n;}
__ADAPTER__
int main(void){unsigned owner,k;
for(owner=0;owner<2;owner++){
 memset(&arcade_f,0,sizeof arcade_f);memset(&g_game,0,sizeof g_game);
 wave_clock=owner;g_arcadeCombatDifficulty=0;arcade_f.wave.reservation=4;arcade_f.live=4;
 for(k=0;k<4;k++){ArcadeFActor*a=&arcade_f.actors[k];a->active=1;a->x=a->y=100;}
 for(k=0;k<2;k++){
  unsigned shot;memset(g_entities,0,sizeof g_entities);sound_count=0;
  for(shot=0;shot<2;shot++){Entity*s=&g_entities[16+owner*2+shot];s->active=1;s->x=100;s->y=139;}
  f_shot_collisions();
  assert(arcade_f.wave.reservation==2-k*2&&arcade_f.live==4);
  assert(sound_count==(k==1?3:2)&&sounds[0]==12&&sounds[1]==12);
  if(k==1)assert(sounds[2]==14);
  for(shot=0;shot<2;shot++){Entity*s=&g_entities[16+owner*2+shot];assert(s->active==2&&s->phase==2&&!s->fireTimer&&s->state==1);}
 }
 assert(g_game.players[owner].score==700&&!g_game.players[1-owner].score&&g_arcadeCombatDifficulty==4*0x33+0x40);
 for(k=0;k<4;k++)assert(arcade_f.actors[k].owner==owner&&arcade_f.actors[k].phase==(k==3?4:0));
}
return 0;}
'''.replace('f_shot_collisions();','fixture_shots();').replace('__ADAPTER__',adapter);(o/'test.c').write_text(c)
files=['arcade_f_combat','arcade_difficulty','arcade_native_admission']
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f'{f}.c')for f in files],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
for token in ['changed!=SS_WAVE_BIT_CLASSF','arcade_f_pool_reset(&arcade_f,cold)','arcade_f_pool_schedule(&arcade_f','arcade_f_pool_update(&arcade_f','arcade_f_contact_target(arcade_f.actors','arcade_f_bonus_collect(a,pk','input.state.f[k]=arcade_f.actors[k].active','arcade_f.actors[i].x+=dx','arcade_f_render_actor(&arcade_f.actors[i-76]']:
 assert token in s,token
assert s.index('        e_shot_collisions();')<s.index('        f_shot_collisions();')
assert s.index('arcade_e_pool_update(')<s.index('arcade_f_pool_update(')
assert s.index('arcade_e_contact_target(')<s.index('arcade_f_contact_target(')<s.index('/* E2F8')
report={'adapter_complete_clear_sequences':2,'scope':'Production shot adapter, 700-point clear, first-hit ordering and source-family adapter ordering. Static live wiring checks supplement host tests; target gameplay still needs runtime evidence.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

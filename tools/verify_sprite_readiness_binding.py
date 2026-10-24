#!/usr/bin/env python3
"""Production logical readiness adapter: slot ownership and no SAT/culling dependency."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/sprite-readiness-binding';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/entity.c').read_text();a=s.index('static void entity_sprite_admission(');b=s.index('u8 entity_arcade_sprite_ready(',a)
old=(r/'tools/fixtures/readiness_before_split.c').read_text().replace('entity_sprite_admission','reference_sprite_admission').replace('ArcadeAdmissionInput input;', 'ArcadeAdmissionInput input={0};for(u8 fk=0;fk<4;fk++)input.state.f[fk]=arcade_f.actors[fk].active;')
c=r''' 
#include <assert.h>
#include <string.h>
#include "arcade_native_admission.h"
static ArcadeAdmissionCache arcade_admission_cache;
#define arcade_admission (arcade_admission_cache.result)
static u8 arcade_main_ready,arcade_shot_budget,legacy_pool_used;
#define ENTITY_MAX 48
#define ET_POPUP 14
static struct {u8 active;} arcade_food_pickup,arcade_food_conversion,arcade_food_carrier;
static struct {struct {u8 active;} actors[16];} arcade_main;
static struct {struct {u8 active;} actors[6];} arcade_c;
static struct {struct {u8 active;} actors[4];} arcade_d;
static struct {struct {u8 active;} actors[2];} arcade_e;
static struct {struct {u8 active;} actors[4];} arcade_f;
static struct {struct {u8 active;} body;} arcade_large_actors[3];
static struct {u8 active;} arcade_weapons[4],arcade_k_actors[4];
typedef struct {u8 active,type;} Entity;
static Entity g_entities[48];
static struct {u8 numPlayers;} g_game;
static u8 player_active[2];
static u16 game_player_target_flags(u8 k){return (u16)player_active[k]<<8;}
__FUNCTION__
static u8 entity_logical_sprite_remaining(void){ArcadeAdmissionInput expected;reference_sprite_admission();expected=arcade_admission_cache.input;candidate_sprite_admission();assert(!memcmp(&expected,&arcade_admission_cache.input,sizeof expected));entity_arcade_prepare_admission();assert(!memcmp(&expected,&arcade_admission_cache.input,sizeof expected));assert(arcade_shot_budget==(arcade_main_ready!=0));return arcade_main_ready;}
static u32 seed=193;
static u8 random_active(void){static const u8 flags[16]={0,0,0,0,0,0,0,0,0,0,0,1,2,64,128,130};seed=seed*1664525u+1013904223u;return flags[seed>>28];}
int main(void){unsigned trial,k;u8 expected,active;
 for(trial=0;trial<100000;trial++){
  g_game.numPlayers=1+(trial&1);
  arcade_food_pickup.active=random_active();arcade_food_conversion.active=random_active();arcade_food_carrier.active=random_active();
  for(k=0;k<2;k++)player_active[k]=random_active();
  for(k=0;k<16;k++)arcade_main.actors[k].active=random_active();
  for(k=0;k<6;k++)arcade_c.actors[k].active=random_active();
  for(k=0;k<4;k++)arcade_d.actors[k].active=random_active();
  for(k=0;k<2;k++)arcade_e.actors[k].active=random_active();
  for(k=0;k<4;k++)arcade_f.actors[k].active=random_active();
  for(k=0;k<48;k++){g_entities[k].active=random_active();g_entities[k].type=(trial+k)%15;}
  for(k=0;k<3;k++)arcade_large_actors[k].body.active=random_active();
  for(k=0;k<4;k++){arcade_weapons[k].active=random_active();arcade_k_actors[k].active=random_active();}
  legacy_pool_used=trial&1;
  if(!legacy_pool_used)for(k=24;k<48;k++)g_entities[k].active=0;
  /* Independent conceptual emitter: each eligible single decrements one;
   * large consumes four atomically; shared K is counted only once. */
  u8 singles[81];unsigned n=0;
  singles[n++]=arcade_food_carrier.active;singles[n++]=arcade_food_pickup.active;singles[n++]=arcade_food_conversion.active;
  for(k=16;k<20;k++)singles[n++]=g_entities[k].type==ET_POPUP?0:g_entities[k].active;
  singles[n++]=player_active[0];singles[n++]=g_game.numPlayers==2?player_active[1]:0;
  for(k=0;k<16;k++)singles[n++]=arcade_main.actors[k].active;
  for(k=0;k<48;k++)if(k<12 || k>=20)singles[n++]=g_entities[k].type==ET_POPUP?0:g_entities[k].active;
  for(k=0;k<6;k++)singles[n++]=arcade_c.actors[k].active;
  for(k=0;k<4;k++)singles[n++]=arcade_d.actors[k].active;
  for(k=0;k<2;k++)singles[n++]=arcade_e.actors[k].active;
  for(k=0;k<4;k++)singles[n++]=arcade_f.actors[k].active;
  assert(n==81);expected=24;
  for(k=0;k<n;k++)if(singles[k] && singles[k]<128 && expected)expected--;
  for(k=0;k<3;k++)if(arcade_large_actors[k].body.active && expected>=4)expected-=4;
  for(k=0;k<4;k++){active=arcade_weapons[k].active?arcade_weapons[k].active:arcade_k_actors[k].active;if(active && active<128 && expected)expected--;}
  assert(entity_logical_sprite_remaining()==expected);
 }
 /* No visible-count inference: inactive P2 and offscreen source flags matter. */
 memset(g_entities,0,sizeof(g_entities));memset(&arcade_main,0,sizeof(arcade_main));memset(&arcade_c,0,sizeof(arcade_c));memset(&arcade_d,0,sizeof(arcade_d));memset(&arcade_e,0,sizeof(arcade_e));memset(&arcade_f,0,sizeof(arcade_f));
 memset(arcade_large_actors,0,sizeof(arcade_large_actors));memset(arcade_weapons,0,sizeof(arcade_weapons));memset(arcade_k_actors,0,sizeof(arcade_k_actors));
 arcade_food_pickup.active=arcade_food_conversion.active=arcade_food_carrier.active=0;
 legacy_pool_used=0;
 player_active[0]=1;player_active[1]=0;g_game.numPlayers=2;
 assert(entity_logical_sprite_remaining()==23);
 arcade_main.actors[0].active=1;assert(entity_logical_sprite_remaining()==22);
 arcade_main.actors[0].active=130;assert(entity_logical_sprite_remaining()==23);
 /* Fixed gathers preserve every active byte at every source position. */
 for(k=0;k<26;k++)for(unsigned flag=0;flag<256;flag++){
  memset(&arcade_main,0,sizeof(arcade_main));
  memset(&arcade_c,0,sizeof(arcade_c));memset(&arcade_d,0,sizeof(arcade_d));
  if(k<16)arcade_main.actors[k].active=flag;
  else if(k<22)arcade_c.actors[k-16].active=flag;
  else arcade_d.actors[k-22].active=flag;
  entity_logical_sprite_remaining();
 }
 return 0;
}
'''.replace('__FUNCTION__',old+'\n'+(r/'tools/fixtures/readiness_split_candidate.c').read_text().replace('entity_sprite_admission','candidate_sprite_admission').replace('ArcadeAdmissionInput input;', 'ArcadeAdmissionInput input={0};for(u8 fk=0;fk<4;fk++)input.state.f[fk]=arcade_f.actors[fk].active;')+'\n'+s[a:b])
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_native_admission.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
assert 'if(i>=24)legacy_pool_used=1;' in s
assert 'g_entities[i].active = 0;\n    legacy_pool_used=0;' in s
report={'adapter_cases':106659,'scope':'Production readiness mapper with minimal actor/game types: active flags, absent P2, main reservation flags, shared K ownership, large atomic cost and no native popup cost. No geometry/SAT dependency. Provisional family populations remain provisional; render admission/order not covered.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

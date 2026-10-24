#!/usr/bin/env python3
"""Compare live pool mapping/cache history using extracted candidate functions."""
from pathlib import Path
import subprocess,json,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/admission-inplace';o.mkdir(parents=True,exist_ok=True)
c='''#include <assert.h>
#include <string.h>
#include "arcade_native_admission.h"
#define ET_POPUP 14
typedef struct {u8 active,type;} Entity;
typedef struct {u8 active;} Actor;
static Entity g_entities[48];
static struct {Actor actors[16];} arcade_main;
static struct {Actor actors[6];} arcade_c;
static struct {Actor actors[4];} arcade_d,arcade_f;
static struct {Actor actors[2];} arcade_e;
static struct {Actor body;} arcade_large_actors[3];
static Actor arcade_weapons[4],arcade_k_actors[4];
static Actor arcade_food_pickup,arcade_food_conversion,arcade_food_carrier;
static struct {u8 numPlayers;} g_game;
static u8 legacy_pool_used,arcade_main_ready,arcade_shot_budget;
static u16 flags[2];
static u16 game_player_target_flags(u8 k){return flags[k];}
static ArcadeAdmissionCache cache_before,cache_after;
'''
for variant in ('baseline','candidate'):
 s=(o/(variant+'.c')).read_text();a=s.index('static void entity_sprite_admission(void)');b=s.index('/* Logical readiness',a)
 if variant=='candidate':a=s.index('static inline void admission_set(')
 name='before' if variant=='baseline' else 'after'
 block=s[a:b].replace('entity_sprite_admission',name).replace('arcade_admission_cache','cache_'+name).replace('arcade_admission.remaining','cache_'+name+'.result.remaining')
 c+=block
c+='''static u32 rng=82;
static u8 byte(void){rng=rng*1664525u+1013904223u;return rng>>24;}
int main(void){
 for(unsigned trial=0;trial<20000;trial++){
  unsigned k;u8 ready,budget;
  legacy_pool_used=trial&1;g_game.numPlayers=1+(trial%3!=0);
  for(k=0;k<48;k++){g_entities[k].active=byte();g_entities[k].type=byte();}
  for(k=0;k<16;k++)arcade_main.actors[k].active=byte();
  for(k=0;k<6;k++)arcade_c.actors[k].active=byte();
  for(k=0;k<4;k++){
   arcade_d.actors[k].active=byte();arcade_f.actors[k].active=byte();
   arcade_weapons[k].active=byte();arcade_k_actors[k].active=byte();
  }
  for(k=0;k<2;k++){arcade_e.actors[k].active=byte();flags[k]=(u16)byte()<<8;}
  for(k=0;k<3;k++)arcade_large_actors[k].body.active=byte();
  arcade_food_pickup.active=byte();arcade_food_conversion.active=byte();arcade_food_carrier.active=byte();
  if(trial%17==0)cache_before.valid=cache_after.valid=0;
  for(k=0;k<3;k++){
   before();ready=arcade_main_ready;budget=arcade_shot_budget;after();
   assert(ready==arcade_main_ready && budget==arcade_shot_budget);
   assert(!memcmp(&cache_before.input,&cache_after.input,sizeof(ArcadeAdmissionInput)));
   assert(!memcmp(&cache_before.result,&cache_after.result,sizeof(ArcadeNativeAdmission)));
   assert(cache_before.hits==cache_after.hits && cache_before.misses==cache_after.misses);
   assert(cache_before.valid==cache_after.valid);
  }
 }
 return 0;
}
'''
(o/'host.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'host.c'),str(r/'src/arcade_native_admission.c'),'-o',str(o/'host')],check=True)
subprocess.run([str(o/'host')],check=True)
report={'passed':True,'pool_states':20000,'cache_calls_each':60000,'hashes':{n:hashlib.sha256((o/n).read_bytes()).hexdigest() for n in ('baseline.c','candidate.c','host.c')},'scope':'Actual extracted pool mapping and cache update functions, real native admission. Compare every cache input/output byte, counters and published readiness over varying actor types/active bytes, player eligibility, legacy ownership, invalidations and repeated calls. ASan/UBSan; not target timing or full game proof.'}
(o/'host-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

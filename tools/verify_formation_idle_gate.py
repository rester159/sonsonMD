#!/usr/bin/env python3
"""Actual G/H scheduler adapters versus real controller, before/after idle gate."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/formation-idle-gate';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/entity.c').read_text();a=s.index('static void formation_schedule(');b=s.index('\nvoid entity_schedule_tick',a)
old=(r/'tools/fixtures/formation_schedule_before_idle_gate.c').read_text()
c='''#include <assert.h>
#include <string.h>
#include "arcade_foes.h"
#include "ss_wave_sched.h"
#define ET_ENEMY_A 1
#define ET_CREATURE_G 2
typedef struct {s16 x,y;u8 active,type,fromSched,fruit,owner,facing;} Entity;
static Entity g_entities[12];static ArcadeFoe arcade_foes[12];static ArcadeWaves arcade_waves;
static ArcadeRng arcade_rng;static u8 formation_reservation[2],wave_clock;static u16 g_arcadeCombatDifficulty;
static struct {s16 cameraX;}g_game;
static unsigned allocations;
static Entity *entity_spawn_range(u8 type,s16 x,s16 y,u16 first,u16 end){
 assert(first<12&&end==first+1);allocations++;
 Entity*e=&g_entities[first];if(e->active)return 0;
 memset(e,0,sizeof(*e));e->active=1;e->type=type;e->x=x;e->y=y;return e;
}
'''
c+=old.replace('formation_schedule','before')+'\n'+(r/'tools/fixtures/formation_schedule_idle_gate_candidate.c').read_text().replace('formation_schedule','after')
c+='''
static u32 seed=3;
static u32 next(void){return seed=seed*1664525u+1013904223u;}
int main(void){
 for(unsigned n=0;n<200000;n++){
  Entity initial[12],expected[12];ArcadeFoe initial_foes[12],expected_foes[12];
  ArcadeWaves initial_waves,expected_waves;ArcadeRng initial_rng,expected_rng;
  u8 initial_res[2],expected_res[2],family=n&1;unsigned expected_alloc;
  memset(g_entities,0,sizeof g_entities);memset(arcade_foes,0,sizeof arcade_foes);memset(&arcade_waves,0,sizeof arcade_waves);
  for(unsigned k=0;k<12;k++){g_entities[k].active=(n%3)?0:((next()>>24)&3);arcade_foes[k].body.xf=next();arcade_foes[k].kind=next();}
  ArcadeWaveControl*c=&arcade_waves.controls[family?SS_WAVE_BIT_CLASSH:SS_WAVE_BIT_CLASSG];
  c->active=(n%5)?1:0;c->state=(n/5)%3;c->phase=(n/15)%4;c->wait=next();c->parameter=next();c->auxiliary=next();
  g_game.cameraX=next();wave_clock=next();g_arcadeCombatDifficulty=next();u16 tracked=next();
  arcade_rng.seed=next();arcade_rng.last=next();formation_reservation[0]=next();formation_reservation[1]=next();
  memcpy(initial,g_entities,sizeof initial);memcpy(initial_foes,arcade_foes,sizeof initial_foes);initial_waves=arcade_waves;initial_rng=arcade_rng;memcpy(initial_res,formation_reservation,2);
  allocations=0;before(family,tracked);expected_alloc=allocations;
  memcpy(expected,g_entities,sizeof expected);memcpy(expected_foes,arcade_foes,sizeof expected_foes);expected_waves=arcade_waves;expected_rng=arcade_rng;memcpy(expected_res,formation_reservation,2);
  memcpy(g_entities,initial,sizeof initial);memcpy(arcade_foes,initial_foes,sizeof initial_foes);arcade_waves=initial_waves;arcade_rng=initial_rng;memcpy(formation_reservation,initial_res,2);
  allocations=0;after(family,tracked);
  assert(allocations==expected_alloc&&!memcmp(expected,g_entities,sizeof expected)&&!memcmp(expected_foes,arcade_foes,sizeof expected_foes));
  assert(!memcmp(&expected_waves,&arcade_waves,sizeof arcade_waves)&&!memcmp(&expected_rng,&arcade_rng,sizeof arcade_rng)&&!memcmp(expected_res,formation_reservation,2));
 }
 return 0;
}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_waves.c'),str(r/'src/arcade_formations.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'cases':200000,'scope':'Actual old/new adapters with real formation controller/RNG, allocation shim. Compare all actor/controller/reservation/RNG bytes and allocation counts. Both families, inactive and live slots, wait/birth states; not whole-game parity.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

#!/usr/bin/env python3
"""Differential production large-update loop using real native actors."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/large-idle-skip';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/entity.c').read_text();a=s.index('        u8 k,j;',s.index('profile_large_update_begin'));b=s.index('\n    }\n    __asm__',a);body=s[a:b]
line='            if(!arcade_large_actors[k].body.active)continue;\n';assert body.count(line)==1
c='''#include <assert.h>
#include <string.h>
#include "arcade_large.h"
#include "arcade_k.h"
static ArcadeLargeActor arcade_large_actors[3];static ArcadeKProjectile arcade_k_actors[4];
static ArcadeFoeWeapon arcade_weapons[4];static ArcadeFoeWorld arcade_world;static ArcadeRng arcade_rng;
static u16 g_arcadeCombatDifficulty;
'''
c+='static void optimized(void){\n'+body+'\n}\nstatic void reference(void){\n'+body.replace(line,'')+'\n}\n'
c+='''int main(void){unsigned mask,occupied,tick,i;
for(mask=0;mask<8;mask++){for(occupied=0;occupied<256;occupied++){
 ArcadeLargeActor initial[3],expected[3];ArcadeKProjectile children[4],expected_children[4];ArcadeRng rng,expected_rng;
 memset(arcade_large_actors,0,sizeof(arcade_large_actors));memset(arcade_k_actors,0,sizeof(arcade_k_actors));memset(arcade_weapons,0,sizeof(arcade_weapons));
 arcade_rng=(ArcadeRng){occupied*251,17};arcade_world=(ArcadeFoeWorld){0};
 arcade_world.players[0]=(ArcadeFoePlayer){90,104,1,0};arcade_world.players[1]=(ArcadeFoePlayer){120,136,1,0};arcade_world.tracking=1;
 for(i=0;i<3;i++){arcade_large_actors[i].body.active=(mask>>i)&1;arcade_large_actors[i].body.subtype=i;arcade_large_actors[i].x=232;arcade_large_actors[i].y=144;}
 for(i=0;i<4;i++){arcade_k_actors[i].active=(occupied>>i)&1;arcade_weapons[i].active=(occupied>>(i+4))&1;}
 for(tick=0;tick<180;tick++){
  arcade_world.clock=tick;
  memcpy(initial,arcade_large_actors,sizeof(initial));memcpy(children,arcade_k_actors,sizeof(children));rng=arcade_rng;
  reference();memcpy(expected,arcade_large_actors,sizeof(expected));memcpy(expected_children,arcade_k_actors,sizeof(expected_children));expected_rng=arcade_rng;
  memcpy(arcade_large_actors,initial,sizeof(initial));memcpy(arcade_k_actors,children,sizeof(children));arcade_rng=rng;
  optimized();assert(!memcmp(expected,arcade_large_actors,sizeof(expected)));assert(!memcmp(expected_children,arcade_k_actors,sizeof(expected_children)));assert(arcade_rng.seed==expected_rng.seed && arcade_rng.last==expected_rng.last);
 }
}}
return 0;}
'''
(o/'test.c').write_text(c)
sources=[r/'src'/n for n in ('arcade_large_actor.c','arcade_large.c','arcade_large_fire.c','arcade_large_death.c','arcade_large_animation.c','arcade_large_animation_data.c','arcade_large_motion_data.c')]
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-I'+str(r/'inc'),str(o/'test.c'),*map(str,sources),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'updates':8*256*180,'scope':'Production loop with and without early inactive skip, real large actor functions, all live-slot masks and shared projectile occupancy. Actor/child/RNG state equivalence; host only.'}
(o/'equivalence.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

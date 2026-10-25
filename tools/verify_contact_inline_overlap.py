#!/usr/bin/env python3
"""Actual H/G/I contact-loop differential; bonus awards stubbed deterministically; wrapped overlap boundaries covered."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/contact-inline-overlap';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/entity.c').read_text();a=s.rfind('            u8 k;',0,s.index('/* E2FE/E301/E304: H, G, I.'));b=s.index('\n        }',a);body=(r/'tools/fixtures/contact_inline_overlap_candidate.c').read_text() # rejected candidate
reference=(r/'tools/fixtures/contact_before_inline_overlap.c').read_text()
c='''#include <assert.h>
#include <string.h>
#include "arcade_collision.h"
#include "arcade_rewards.h"
#define HURT_FRAMES 8
typedef struct {s16 x,y;u8 active;} Entity;
typedef struct {s16 x,y;u32 score;u8 deadReq,hurt;} Player;
static Entity g_entities[24];static ArcadeFoe arcade_foes[12];static u16 g_arcadeCombatDifficulty;
static u32 sounds;
static void snd_command(u8 s){sounds=sounds*31+s;}
u8 arcade_gh_bonus_collect(ArcadeFoe*a,u8 owner,u16 combat,ArcadeBonusAward*out){if(!(a->kind&1))return 0;a->kind=0;out->combat=combat+1;out->points=100+owner;out->sound=4;return 1;}
'''
c+='static void optimized(Player*p,u8 pk){\n'+body+'\n}\nstatic void reference(Player*p,u8 pk){\n'+reference+'\n}\n'
c+='''int main(void){unsigned n,k;u32 seed=1;
/* Exhaust every byte-Y and word-X displacement at several wrapping origins. */
for(n=0;n<65536;n++)for(k=0;k<4;k++){
 u16 px=(u16)(k*21845u),ex=(u16)(px+n);u8 py=(u8)(k*85u),ey=(u8)(py+n);
 assert(((u16)(px-ex+7)<=14)==arcade_body_contact(px,py,ex,py,1,1));
 assert(((u8)((u16)ey-(u16)py+7)<=14)==arcade_body_contact(px,(u8)(239-py),px,(u8)(239-ey),1,1));
}
for(n=0;n<100000;n++){
 Player p={0},q;Entity initial[24],expected[24];ArcadeFoe foes[12],expected_foes[12];u16 combat;u32 expected_sound;
 seed=seed*1664525u+1013904223u;p.x=(s16)seed;p.y=(s16)(seed>>16);p.hurt=n&1;
 memset(g_entities,0,sizeof(g_entities));memset(arcade_foes,0,sizeof(arcade_foes));
 for(k=0;k<24;k++){
  seed=seed*1664525u+1013904223u;
  g_entities[k].x=(s16)(p.x+(s16)((seed>>8)%35)-17);g_entities[k].y=(s16)(p.y+(s16)((seed>>16)%35)-17);
  g_entities[k].active=(u8)seed;if(k<12)arcade_foes[k].kind=seed>>24;
 }
 memcpy(initial,g_entities,sizeof(initial));memcpy(foes,arcade_foes,sizeof(foes));q=p;g_arcadeCombatDifficulty=n;sounds=0;
 reference(&q,n&1);memcpy(expected,g_entities,sizeof(expected));memcpy(expected_foes,arcade_foes,sizeof(expected_foes));combat=g_arcadeCombatDifficulty;expected_sound=sounds;
 memcpy(g_entities,initial,sizeof(initial));memcpy(arcade_foes,foes,sizeof(foes));g_arcadeCombatDifficulty=n;sounds=0;
 optimized(&p,n&1);
 assert(p.score==q.score && p.deadReq==q.deadReq && p.hurt==q.hurt);assert(combat==g_arcadeCombatDifficulty && sounds==expected_sound);
 assert(!memcmp(expected,g_entities,sizeof(expected)) && !memcmp(expected_foes,arcade_foes,sizeof(expected_foes)));
}return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_collision.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'cases':100000,'overlap_boundary_checks':524288,'scope':'Actual production body-contact loop versus archived general-overlap loop, real overlap function. Mixed active flags, wrapped coordinates, both owners, family skip and death ordering; bonus award effects stubbed.'}
(o/'equivalence.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

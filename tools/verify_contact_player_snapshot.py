#!/usr/bin/env python3
"""Actual native contact dispatch before/after snapshot, with real combat code."""
from pathlib import Path
import argparse
import json
import subprocess
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline-entity',type=Path);parser.add_argument('--candidate-entity',type=Path);parser.add_argument('--output-dir',type=Path)
args=parser.parse_args()
if bool(args.baseline_entity)!=bool(args.candidate_entity):parser.error('Specify both entity variants')
out=args.output_dir.resolve() if args.output_dir else root/'out/parity-audit/contact-player-snapshot'
out.mkdir(parents=True,exist_ok=True)
old=(root/'tools/fixtures/contact_before_player_snapshot.c').read_text()
source=(args.candidate_entity or root/'src/entity.c').read_text()
if args.baseline_entity:
 old=args.baseline_entity.read_text();start=old.index('        /* Food/background displacement is complete.');old=old[start:old.index('        /* E2F8 precedes',start)]
a=source.index('        /* Food/background displacement is complete.')
b=source.index('        /* E2F8 precedes',a)
c='''#include <assert.h>
#include <string.h>
#include "arcade_main_actor.h"
#include "arcade_c_combat.h"
#include "arcade_d_combat.h"
#include "arcade_e_combat.h"
#include "arcade_f_combat.h"
#include "arcade_difficulty.h"
#define FLOW_ALIVE 0
#define HURT_FRAMES 8
typedef struct {s16 x,y;u32 score;u8 flow,invuln,deadReq,hurt;} Player;
typedef struct {ArcadeMainActor main[16];ArcadeCActor c[6];ArcadeDActor d[4];ArcadeEActor e[2];ArcadeFActor f[4];} Pools;
static struct {ArcadeMainActor actors[16];} arcade_main;
static struct {ArcadeCActor actors[6];} arcade_c;
static struct {ArcadeDActor actors[4];} arcade_d;
static struct {ArcadeEActor actors[2];} arcade_e;
static struct {ArcadeFActor actors[4];} arcade_f;
static void game_score_add(Player*p,u32 points){p->score+=points;}
static u8 g_survivalRun;static u16 flags,queries;static u32 sounds;
static u16 game_player_target_flags(u8 k){assert(k<2);queries++;return flags;}
static void snd_command(u8 cmd){sounds=sounds*31+cmd;}
static void save(Pools*p){memcpy(p->main,arcade_main.actors,sizeof p->main);memcpy(p->c,arcade_c.actors,sizeof p->c);memcpy(p->d,arcade_d.actors,sizeof p->d);memcpy(p->e,arcade_e.actors,sizeof p->e);memcpy(p->f,arcade_f.actors,sizeof p->f);}
static void load(const Pools*p){memcpy(arcade_main.actors,p->main,sizeof p->main);memcpy(arcade_c.actors,p->c,sizeof p->c);memcpy(arcade_d.actors,p->d,sizeof p->d);memcpy(arcade_e.actors,p->e,sizeof p->e);memcpy(arcade_f.actors,p->f,sizeof p->f);}
'''
for name,body in [('before',old),('after',source[a:b])]:
    c+='static void '+name+'(Player*p,u8 pk){\n'+body+'}\n'
c+='''int main(void){u32 seed=91;unsigned trial,k;unsigned long reduced=0;
for(trial=0;trial<200000;trial++){
 Pools initial,expected,actual;Player p={0},q;u16 combat,old_queries;u32 sound;
 memset(&initial,0,sizeof initial);seed=seed*1664525u+1013904223u;
 p.x=(s16)seed;p.y=(s16)(seed>>16);p.score=seed;
 p.flow=(trial%17==0)?1:0;p.invuln=(trial%19==0);p.deadReq=(trial%23==0);p.hurt=trial&1;
 g_survivalRun=(trial%29==0);flags=(trial%5)?256:(u16)seed;
 #define FILL(field,count,bonus) for(k=0;k<count;k++){seed=seed*1664525u+1013904223u;initial.field[k].x=(u16)(p.x+(s16)((seed>>8)%33)-16);initial.field[k].y=(u8)(239-p.y+(s16)((seed>>16)%33)-16);initial.field[k].active=(u8)seed;initial.field[k].bonus=(seed>>24)%13;initial.field[k].phase=(u8)(seed>>16);}
 FILL(main,16,hit);FILL(c,6,bonus_kind);FILL(d,4,bonus_kind);FILL(e,2,bonus_kind);FILL(f,4,bonus_kind);
 #undef FILL
 load(&initial);q=p;queries=0;sounds=0;g_arcadeCombatDifficulty=(u16)trial;
 before(&q,trial&1);save(&expected);combat=g_arcadeCombatDifficulty;sound=sounds;old_queries=queries;
 load(&initial);queries=0;sounds=0;g_arcadeCombatDifficulty=(u16)trial;
 after(&p,trial&1);save(&actual);
 assert(!memcmp(&p,&q,sizeof p)&&!memcmp(&expected,&actual,sizeof expected));
 assert(combat==g_arcadeCombatDifficulty&&sound==sounds);
 assert(queries<=1&&queries<=old_queries);reduced+=old_queries-queries;
}assert(reduced>100000);return 0;}
'''
if args.baseline_entity:c=c.replace('assert(reduced>100000);','(void)reduced;')
(out/'test.c').write_text(c)
files=['arcade_main_collision','arcade_c_combat','arcade_d_combat','arcade_e_combat','arcade_f_combat','arcade_difficulty']
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'inc'),str(out/'test.c'),*[str(root/'src'/f'{f}.c') for f in files],'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
report={'cases':200000,'scope':'Actual dispatch blocks and all five real contact/bonus implementations: ordered effects, actor bytes, wrapped positions, score, difficulty, sound sequence and player death/hurt flags. Stable lifecycle flags supplied by a query shim; no full-game parity claim.'}
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))

#!/usr/bin/env python3
"""Compare fixed-family functions with the independently compiled old body."""
from pathlib import Path
import json
import subprocess
root=Path(__file__).resolve().parents[1]
out=root/'out/parity-audit/foe-family-specialization'
out.mkdir(parents=True,exist_ok=True)
c='''#include <assert.h>
#include <string.h>
#include "arcade_foes.h"
void reference_shared(ArcadeFoe*,u8,u8,const ArcadeFoeWorld*,ArcadeFloor,void*,ArcadeFoeWeapon*,u8);
static u32 floor_hash;static unsigned floor_count;
static s16 floor_probe(u16 x,u8 y,void *ctx){u16 key=*(u16*)ctx;floor_count++;floor_hash=floor_hash*31+x+y;return ((x+y+key)&3)?ARCADE_NO_FLOOR:(s16)(key%8);}
static u32 seed=103;
static u32 next(void){return seed=seed*1664525u+1013904223u;}
int main(void){unsigned trial,k,step;
for(trial=0;trial<200000;trial++){
 ArcadeFoe a={0},b;ArcadeFoeWorld w={0};ArcadeFoeWeapon weapons[4]={{0}},reference[4];
 u8 family=trial&1,slot=trial%6,reserved=(trial>>1)&15;u16 key=next();
 a.body.x=next();a.body.xf=next();a.body.y=next();a.body.yf=next();
 a.body.active=(trial%7==0)?0:(trial%7==1)?128:1;
 a.body.state=trial%8;a.body.phase=(trial/8)%5;a.body.delay=next();a.body.facing=(trial&2)?3:1;
 a.kind=(trial&4)?0:1;a.anim_timer=next();a.stamp=next();a.target=trial%3;a.hit_style=(trial>>1)&1;a.death_timer=next();
 w.camera=next();w.clock=next();w.stage=next();w.tracking=trial&1;w.scroll_stopped=(trial>>1)&1;w.shot_budget=(trial>>2)&1;w.purge=(trial%31==0);
 for(k=0;k<2;k++){w.players[k].x=next();w.players[k].y=next();w.players[k].active=(trial>>k)&1;w.players[k].kind=(trial>>(k+2))&1;}
 for(k=0;k<4;k++){weapons[k].x=next();weapons[k].xf=next();weapons[k].yf=next();weapons[k].active=(trial>>k)&1;}
 b=a;memcpy(reference,weapons,sizeof weapons);
 for(step=0;step<16;step++){
  u32 expected_hash;unsigned expected_count;w.clock++;
  floor_hash=floor_count=0;reference_shared(&b,family,slot,&w,floor_probe,&key,reference,family?0:reserved);expected_hash=floor_hash;expected_count=floor_count;
  floor_hash=floor_count=0;
  if(family)arcade_foe_step_h(&a,slot,&w,floor_probe,&key,weapons);
  else arcade_foe_step_g(&a,slot,&w,floor_probe,&key,weapons,reserved);
  assert(!memcmp(&a,&b,sizeof a)&&!memcmp(reference,weapons,sizeof weapons));
  assert(floor_hash==expected_hash&&floor_count==expected_count);
 }
}return 0;}
'''
(out/'test.c').write_text(c)
flags=['-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'inc')]
subprocess.run(['cc',*flags,'-Darcade_foe_step=reference_step','-Darcade_foe_step_shared=reference_shared','-Darcade_foe_weapon_step=reference_weapon','-c',str(root/'tools/fixtures/foes_before_family_specialization.c'),'-o',str(out/'reference.o')],check=True)
files=['arcade_foes','arcade_foe_data','arcade_waves','arcade_formations']
subprocess.run(['cc',*flags,str(out/'test.c'),str(out/'reference.o'),*[str(root/'src'/f'{f}.c') for f in files],'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
report={'initial_states':200000,'updates':3200000,'scope':'Independent old translation unit versus fixed-family entry points, real animation/formation data, actor and shared weapon bytes, floor callback order and arguments. Bounded valid facing/target/cursor values; not full source-arcade parity.'}
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))

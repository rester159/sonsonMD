#!/usr/bin/env python3
"""Offline enemyMain shot/bonus contracts; source revision and events guarded."""
from pathlib import Path
import hashlib,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/main-combat';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
assert b[0xe3cd-0x4000:0xe3dc-0x4000]==bytes.fromhex('6a 02 27 0b e6 02 5a 26 2b c6 01 e7 13 20 25')
# 4CD9..4CF4 predecrements both score and award pointers four times.
# Table entries point AFTER the BCD amount, not at its first byte.
assert b[0x4cd9-0x4000:0x4cf5-0x4000]==bytes.fromhex(
    'a6 82 ab a2 19 a7 84 a6 82 a9 a2 19 a7 84 '
    'a6 82 a9 a2 19 a7 84 a6 82 a9 a2 19 a7 84')
a=0x4d84+0x2b*2;ptr=int.from_bytes(b[a-0x4000:a-0x4000+2],'big')
award=b[ptr-4-0x4000:ptr-0x4000]
assert award==bytes.fromhex('00 00 05 00')
assert int(award.hex())==500
assert b[0xb9cd-0x4000:0xb9d3-0x4000]==bytes.fromhex('bd bb 31 bd a9 2e')
assert b[0xbb31-0x4000:0xbb45-0x4000]==bytes.fromhex('e6 11 2b 10 e6 13 27 0c 6f 13 cc bb 7f ed 1e 6f 1d 7e 96 17')
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_main_actor.h"
#include "arcade_bonus_art.h"
int main(void){unsigned hp,owner,combat,i,kind,limit;
for(hp=0;hp<256;hp++)for(owner=0;owner<2;owner++)for(combat=0;combat<65536;combat+=127){
 ArcadeMainActor a={0};ArcadePlayerShot s={0};ArcadeMainHit h;u16 sum=(u16)(combat+0x33);
 a.hp=hp;a.active=1;a.state=3;a.phase=1;a.hit=77;a.owner=1-owner;
 arcade_main_hit(&a,&s,owner,combat,&h);
 assert(a.hp==(u8)(hp-1) && s.active==2 && s.state==2 && s.phase==0 && h.sound_hit==7);
 if(hp==1){assert(a.active==2 && a.state==5 && !a.phase && a.owner==owner);assert(h.points==500 && h.event==(0x42b|(owner<<7)) && h.sound_kill==6);assert(h.combat==(sum<0x3f00?sum:combat));}
 else{assert(a.active==1 && a.state==3 && a.phase==1);assert(!h.points && !h.event && !h.sound_kill && h.combat==combat);assert(a.hit==(hp==2?1:77));}
}
for(i=0;i<65536;i++){
 ArcadeMainActor pool[16]={0};u16 x=(u16)i;u8 y=(u8)i;u8 got;
 pool[15].active=1;pool[15].x=65532;pool[15].y=253;
 got=arcade_main_shot_target(pool,x,y,1);
 assert(got==(((u8)(253-y+9)<=14 && (u16)(65532-x+7)<=14)?15:255));
 pool[0]=pool[15];assert(arcade_main_shot_target(pool,x,y,1)==(got==15?0:255));
 pool[0].state=255;assert(arcade_main_shot_target(pool,x,y,1)==got);
 assert(arcade_main_shot_target(pool,x,y,2)==255);
}
for(kind=0;kind<11;kind++)for(limit=0;limit<256;limit++)for(owner=0;owner<2;owner++){
 ArcadeMainActor a={0};u8 next=kind;const ArcadeBonusFrame*f=&arcade_bonus_frames[arcade_bonus_roots[kind]];
 a.active=2;a.state=255;a.x=100;a.owner=owner;
 arcade_main_bonus_step(&a,0,0,&next,limit);
 assert(a.active==64 && a.phase==1 && a.hit==kind && next==(kind+1>limit?0:kind+1));
 assert(a.code==f->code && a.attr==f->attr && a.cursor==f->next && a.anim_timer==f->timer);
 a.phase=2;a.active=2;a.anim_timer=0;f=&arcade_bonus_frames[a.cursor];
 arcade_main_bonus_step(&a,0,0,&next,limit);
 assert(a.phase==3 && a.code==f->code && a.attr==(owner?0xc3:f->attr));
 a.x=248;arcade_main_bonus_step(&a,0,0,&next,limit);assert(a.state==5 && a.phase==2 && a.hit==0);
 if(kind){const ArcadeMainFrame*w=&arcade_main_frames[arcade_main_wounded];assert(a.code==w->code && a.attr==w->attr && a.cursor==w->next);}
 a.state=255;a.phase=1;a.x=8;arcade_main_bonus_step(&a,0,1,&next,limit);assert(a.state==255 && !a.phase);
}
return 0;}
'''
(o/'test.c').write_text(c)
files=('arcade_main_actor.c','arcade_main_actor_data.c','arcade_main_collision.c','arcade_main_bonus.c','arcade_difficulty.c','arcade_bonus_art.c')
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f)for f in files],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'hit_cases':256*2*len(range(0,65536,127)),'geometry_cases':65536,'bonus_cases':11*256*2,'scope':'Offline contracts: byte HP wrap, fifth-shot defeat, hit sound/order, reward/difficulty, first-slot target geometry, conversion poses and owner palette. Not original runtime comparison or gameplay binding.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

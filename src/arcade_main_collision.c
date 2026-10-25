#include "arcade_main_actor.h"
#include "arcade_difficulty.h"
u8 arcade_main_shot_target(const ArcadeMainActor actors[16],u16 x,u8 y,u8 active)
{
    u8 k;if(active!=1)return 255;
    for(k=0;k<16;k++)
    {
        const ArcadeMainActor *a=&actors[k];
        if(a->active==1 && !(a->state&128) &&
           (u8)(a->y-y+9)<=14 && (u16)(a->x-x+7)<=14)return k;
    }
    return 255;
}
void arcade_main_hit(ArcadeMainActor *a,ArcadePlayerShot *shot,u8 owner,u16 combat,ArcadeMainHit *hit)
{
    shot->active=2;shot->state=2;shot->phase=0;
    hit->points=hit->event=0;hit->combat=combat;hit->sound_hit=7;hit->sound_kill=0;
    if(!--a->hp)
    {
        a->active=2;a->state=5;a->phase=0;a->owner=owner;
        hit->points=500;hit->event=(u16)(0x42b|((u16)owner<<7));hit->sound_kill=6;
        hit->combat=arcade_difficulty_add(combat,0x33);
    }
    else if(a->hp==1)a->hit=1;
}

/* ECB7/ECB9 -> ECE9: first overlap returns, whether bonus or hostile. */
u8 arcade_main_contact_target(const ArcadeMainActor actors[16],u16 x,u8 y,u8 active)
{
    u8 k;if(active!=1)return 255;
    for(k=0;k<16;k++)
    {
        const ArcadeMainActor *a=&actors[k];
        if(a->active && !(a->active&2) &&
           (u8)(y-a->y+7)<=14 && (u16)(x-a->x+7)<=14)return k;
    }
    return 255;
}
u8 arcade_main_bonus_collect(ArcadeMainActor *a,u8 owner,u16 combat,ArcadeBonusAward *award)
{
    u8 kind=a->hit;
    if(kind>10)return 0;
    a->active=2;a->phase++;a->owner=owner;
    award->points=kind==10?20000:(u16)(kind+1)*1000;
    award->event=(u16)(0x400|(kind==10?0x2a:kind+9)|((u16)owner<<7));
    award->sound=5;award->combat=arcade_difficulty_add(combat,0xc0);
    return 1;
}

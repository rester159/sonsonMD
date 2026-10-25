#include "arcade_e_combat.h"
#include "arcade_difficulty.h"
u8 arcade_e_shot_targets(const ArcadeEActor actors[2],u16 x,u8 y,u8 active)
{
    u8 k,mask=0;
    if(active!=1)return 0;
    for(k=0;k<2;k++)
    {
        const ArcadeEActor *a=&actors[k];
        if(a->active==1 && !(a->state&128) &&
           (u8)(a->y-y+9)<=16 && (u16)(a->x-x+8)<=16)
            mask|=(u8)(1<<k);
    }
    return mask;
}
void arcade_e_hit(ArcadeEActor *a,ArcadePlayerShot *shot,u8 owner,u16 combat,ArcadeEHit *hit)
{
    a->owner=owner;a->active=2;a->state=3;a->phase=0;
    shot->active=2;shot->state=2;shot->phase=0;
    hit->points=300;hit->event=(u16)(0x418|((u16)owner<<7));hit->sound=12;
    hit->combat=arcade_difficulty_add(combat,0x4c);
}
u8 arcade_e_contact_target(const ArcadeEActor actors[2],u16 x,u8 y,u8 active)
{
    u8 k;if(active!=1)return 255;
    for(k=0;k<2;k++)
    {
        const ArcadeEActor *a=&actors[k];
        if(a->active && !(a->active&2) &&
           (u8)(y-a->y+7)<=14 && (u16)(x-a->x+7)<=14)return k;
    }
    return 255;
}
u8 arcade_e_bonus_collect(ArcadeEActor *a,u8 owner,u16 combat,ArcadeBonusAward *award)
{
    u8 kind=a->bonus_kind;
    if(kind>10)return 0;
    a->active=2;a->phase++;a->owner=owner;
    award->points=kind==10?20000:(u16)(kind+1)*1000;
    award->event=(u16)(0x400|(kind==10?0x2a:kind+9)|((u16)owner<<7));
    award->sound=5;award->combat=arcade_difficulty_add(combat,0xc0);
    return 1;
}

#include "arcade_c_combat.h"
#include "arcade_difficulty.h"
u8 arcade_c_shot_target(const ArcadeCActor actors[6],u16 x,u8 y,u8 active)
{
    u8 k;if(active!=1)return 255;
    for(k=0;k<6;k++)
    {
        const ArcadeCActor *a=&actors[k];
        if(a->active==1 && !(a->state&128) &&
           (u8)(a->y-y+10)<=16 && (u16)(a->x-x+8)<=16)return k;
    }
    return 255;
}
void arcade_c_hit(ArcadeCActor *a,ArcadePlayerShot *shot,u8 owner,u8 *remaining,u16 combat,ArcadeCHit *hit)
{
    a->active=2;a->state=2;a->phase=0;a->owner=owner;
    shot->active=2;shot->state=2;shot->phase=0;
    hit->points=50;hit->event=(u16)(0x416|((u16)owner<<7));hit->sound=13;
    hit->bonus_points=0;hit->bonus_event=0;hit->bonus_sound=0;
    hit->combat=arcade_difficulty_add(combat,0x33);
    if(!--*remaining)
    {
        a->phase=4;
        hit->bonus_points=1000;hit->bonus_event=(u16)(0x414|((u16)owner<<7));hit->bonus_sound=14;
        /* Two separately capped additions, in source event order. */
        hit->combat=arcade_difficulty_add(hit->combat,0x33);
    }
}
u8 arcade_c_contact_target(const ArcadeCActor actors[6],u16 x,u8 y,u8 active)
{
    u8 k;if(active!=1)return 255;
    for(k=0;k<6;k++)
    {
        const ArcadeCActor *a=&actors[k];
        if(a->active && !(a->active&2) &&
           (u8)(y-a->y+7)<=14 && (u16)(x-a->x+7)<=14)return k;
    }
    return 255;
}
u8 arcade_c_bonus_collect(ArcadeCActor *a,u8 owner,u16 combat,ArcadeBonusAward *award)
{
    u8 kind=a->bonus_kind;
    if(kind>10)return 0;
    a->active=2;a->phase++;a->owner=owner;
    award->points=kind==10?20000:(u16)(kind+1)*1000;
    award->event=(u16)(0x400|(kind==10?0x2a:kind+9)|((u16)owner<<7));
    award->sound=5;award->combat=arcade_difficulty_add(combat,0xc0);
    return 1;
}

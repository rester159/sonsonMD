#include "arcade_i.h"
#include "arcade_collision.h"
#include "arcade_difficulty.h"
/* EA0E/EA3C: four owned slots; same wrapped geometry as G/H. */
u8 arcade_i_shot_target(const ArcadeIActor actors[4],u16 shot_x,u8 shot_y)
{
    u8 k;
    for(k=0;k<4;k++)
        if(actors[k].body.active==1 && !(actors[k].body.state&128) &&
           arcade_gh_shot_overlap(actors[k].body.x,actors[k].body.y,shot_x,shot_y))return k;
    return 255;
}
void arcade_i_kill(ArcadeGHKill *out,u16 combat,u8 remaining,u8 owner)
{
    out->owner=owner;out->remaining=(u8)(remaining-1);out->phase=0;
    out->points=50;out->event=(u16)(0x423|((u16)owner<<7));out->bonus=0;
    combat=arcade_difficulty_add(combat,0x40);
    if(!out->remaining)
    {
        out->phase=4;out->points+=500;out->bonus=(u16)(0x413|((u16)owner<<7));
        combat=arcade_difficulty_add(combat,0x40);
    }
    out->combat=combat;
}

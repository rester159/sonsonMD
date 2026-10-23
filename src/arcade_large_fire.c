#include "arcade_large.h"
u8 arcade_large_fire(u16 x,u8 y,u8 *countdown,u8 period,
                     const ArcadeFoePlayer players[2],const u8 occupied[4],
                     ArcadeLargeShotBirth *birth)
{
    u8 k,slot;
    if(*countdown){--*countdown;return 0;}
    for(k=0;k<2;k++)
        if(players[k].active && (u8)(y+16-players[k].y)<=8)break;
    if(k==2)return 0;
    for(slot=0;slot<4;slot++)if(!occupied[slot])break;
    if(slot==4)return 0;
    *countdown=period;birth->slot=slot;birth->y=(u8)(y+12);
    birth->facing=players[k].x>x?1:3;
    birth->x=(u16)(x+(birth->facing==1?6:20));
    return 1;
}

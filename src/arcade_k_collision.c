#include "arcade_k.h"
u8 arcade_k_shot_target(const ArcadeKProjectile pool[4],u16 x,u8 y,u8 active)
{
    u8 k;
    if(active!=1)return 255;
    for(k=0;k<4;k++)
        if(pool[k].active==1 && !(pool[k].state&128) &&
           (u8)(y-pool[k].y+7)<=14 && (u16)(x-pool[k].x+9)<=18)return k;
    return 255;
}
u8 arcade_k_contact_target(const ArcadeKProjectile pool[4],u16 x,u8 y,u8 active)
{
    u8 k;
    if(active!=1)return 255;
    for(k=0;k<4;k++)
        if(pool[k].active && !(pool[k].active&2) &&
           (u8)(y-pool[k].y+6)<=12 && (u16)(x-pool[k].x+6)<=12)return k;
    return 255;
}
u16 arcade_k_cancel(ArcadeKProjectile *enemy,ArcadePlayerShot *shot,u8 owner)
{
    enemy->active=shot->active=2;enemy->state=3;enemy->phase=0;
    shot->state=2;shot->phase=0;
    return (u16)(0x0430|((owner&1)<<7));
}

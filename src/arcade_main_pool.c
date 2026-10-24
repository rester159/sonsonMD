#include "arcade_main_pool.h"
#include <string.h>
/* Scratch summary valid after update and through this tick's contacts. */
u8 arcade_main_contact_presence;
void arcade_main_pool_reset(ArcadeMainPool *p,u8 cold)
{
    u8 k;
    if(cold)memset(p,0,sizeof(*p));
    else {for(k=0;k<16;k++)p->actors[k].active=0;p->live=0;}
}
void arcade_main_pool_update(ArcadeMainPool *p,u16 camera,u8 clock,u8 stopped,u8 purge,u8 *next,u8 limit)
{
    u8 k;arcade_main_contact_presence=0;
    for(k=0;k<16;k++)
    {
        ArcadeMainActor *a=&p->actors[k];
        if(!a->active)continue;
        if(a->state==255)arcade_main_bonus_step(a,camera,purge,next,limit);
        else arcade_main_actor_step(a,k,camera,clock,stopped,purge,&p->live);
        arcade_main_contact_presence|=a->active;
    }
}
u8 arcade_main_pool_schedule(ArcadeMainPool *p,u8 ready,u8 vsync,u8 clock,u8 difficulty,u16 camera,ArcadeRng *rng)
{
    u8 k,n,occupied[16];ArcadeMainBirth births[12];
    if(!p->wave.control.active)return 0;
    for(k=0;k<16;k++)occupied[k]=p->actors[k].active;
    n=arcade_main_wave_step(&p->wave,p->live,ready,vsync,clock,difficulty,camera,occupied,rng,births);
    for(k=0;k<n;k++)
    {
        const ArcadeMainBirth *b=&births[k];ArcadeMainActor *a=&p->actors[b->slot];
        a->x=b->x;a->y=b->y;a->variant=b->variant;a->delay=b->delay;
        a->direction=0;a->active=128;a->state=a->phase=0;
    }
    return n;
}

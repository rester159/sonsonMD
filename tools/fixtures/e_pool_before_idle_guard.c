#include "arcade_e_pool.h"
#include <string.h>
void arcade_e_pool_reset(ArcadeEPool *p,u8 cold)
{
    u8 k;
    if(cold)memset(p,0,sizeof(*p));
    else {for(k=0;k<2;k++)p->actors[k].active=0;p->live=0;}
}
u8 arcade_e_pool_schedule(ArcadeEPool *p,u8 clock,u8 difficulty,u16 camera)
{
    ArcadeEBirth births[2];u8 k,n;
    n=arcade_e_wave_step(&p->wave,p->live,clock,difficulty,camera,births);
    for(k=0;k<n;k++)
    {
        ArcadeEActor *a=&p->actors[k];const ArcadeEBirth *b=&births[k];
        a->x=b->x;a->y=b->y;a->delay=b->delay;a->direction=b->direction;
        a->active=128;a->state=a->phase=0;
    }
    return n;
}
void arcade_e_pool_update(ArcadeEPool *p,u16 camera,u8 clock,u8 stopped,u8 purge,
    u16 target_x,u8 target_y,u8 target,u8 eligible,u8 *available,u8 *next,u8 limit)
{
    u8 k;
    for(k=0;k<2;k++)
        arcade_e_actor_step(&p->actors[k],k,camera,clock,stopped,purge,
            target_x,target_y,target,eligible,&p->live,available,next,limit);
}

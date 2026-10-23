#include "arcade_d_pool.h"
#include <string.h>
/* Scratch summary valid after update and through this tick's contacts. */
u8 arcade_d_contact_presence;
void arcade_d_pool_reset(ArcadeDPool *p,u8 cold)
{
    u8 k;
    if(cold)memset(p,0,sizeof(*p));
    else {for(k=0;k<4;k++)p->actors[k].active=0;p->live=0;}
}
void arcade_d_pool_update(ArcadeDPool *p,u16 camera,u8 clock,u8 difficulty,u8 stopped,u8 purge,
    u16 target_x,u8 target_y,u8 target,u8 *next,u8 limit)
{
    u8 k;arcade_d_contact_presence=0;
    /* active128/255 still own entry/cleanup work; only zero is idle. */
    for(k=0;k<4;k++)if(p->actors[k].active)
    {
        arcade_d_actor_step(&p->actors[k],k,camera,clock,difficulty,stopped,purge,
            target_x,target_y,target,&p->live,next,limit);
        arcade_d_contact_presence|=p->actors[k].active;
    }
}
u8 arcade_d_pool_schedule(ArcadeDPool *p,u8 clock,u8 difficulty,u16 camera,u16 tracked_x)
{
    u8 k,n;ArcadeDBirth births[4];
    n=arcade_d_wave_step(&p->wave,p->live,clock,difficulty,camera,tracked_x,births);
    for(k=0;k<n;k++)
    {
        const ArcadeDBirth *b=&births[k];ArcadeDActor *a=&p->actors[k];
        a->x=b->x;a->y=b->y;a->delay=b->delay;a->kind=b->kind;
        a->target_offset=b->target_offset;a->direction=b->direction;
        a->active=128;a->state=a->phase=0;
    }
    return n;
}

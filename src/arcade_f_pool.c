#include "arcade_f_pool.h"
#include <string.h>
/* Scratch summary valid after update and through this tick's contacts. */
u8 arcade_f_contact_presence;
void arcade_f_pool_reset(ArcadeFPool *p,u8 cold)
{
    u8 k;
    if(cold)memset(p,0,sizeof(*p));
    else {for(k=0;k<4;k++)p->actors[k].active=0;p->live=0;}
}
u8 arcade_f_pool_schedule(ArcadeFPool *p,u8 clock,u8 difficulty,u16 camera)
{
    ArcadeFBirth births[4];u8 k,n;
    n=arcade_f_wave_step(&p->wave,p->live,clock,difficulty,camera,births);
    for(k=0;k<n;k++)
    {
        ArcadeFActor *a=&p->actors[k];const ArcadeFBirth *b=&births[k];
        a->x=b->x;a->y=b->y;a->origin_y=b->origin_y;
        a->delay=b->delay;a->direction=b->direction;
        a->active=128;a->state=a->phase=0;
    }
    return n;
}
void arcade_f_pool_update(ArcadeFPool *p,u16 camera,u8 stopped,u8 purge,
    u8 *next,u8 limit)
{
    u8 k;arcade_f_contact_presence=0;
    /* active128/255 still own entry/cleanup work; only zero is idle. */
    for(k=0;k<4;k++)if(p->actors[k].active)
    {
        arcade_f_actor_step(&p->actors[k],camera,stopped,purge,&p->live,next,limit);
        arcade_f_contact_presence|=p->actors[k].active;
    }
}

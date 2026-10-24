#include "arcade_c_pool.h"
#include <string.h>
/* Scratch summary valid after update and through this tick's contacts. */
u8 arcade_c_contact_presence;
void arcade_c_pool_reset(ArcadeCPool *p,u8 cold)
{
    u8 k;
    if(cold)memset(p,0,sizeof(*p));
    else {for(k=0;k<6;k++)p->actors[k].active=0;p->live=0;}
}
void arcade_c_pool_update(ArcadeCPool *p,u16 camera,u8 stopped,u8 purge,u8 *next,u8 limit)
{
    u8 k;arcade_c_contact_presence=0;
    /* active128/255 still own entry/cleanup work; only zero is idle. */
    for(k=0;k<6;k++)if(p->actors[k].active)
    {
        arcade_c_actor_step(&p->actors[k],camera,stopped,purge,&p->live,next,limit);
        arcade_c_contact_presence|=p->actors[k].active;
    }
}
u8 arcade_c_pool_schedule(ArcadeCPool *p,u8 clock,u8 difficulty,u16 camera)
{
    u8 k,n;ArcadeCBirth births[6];
    n=arcade_c_wave_step(&p->wave,p->live,clock,difficulty,camera,births);
    for(k=0;k<n;k++)
    {
        const ArcadeCBirth *b=&births[k];ArcadeCActor *a=&p->actors[k];
        a->x=b->x;a->y=b->y;a->delay=b->delay;a->kind=b->kind;
        a->active=128;a->state=a->phase=0;
    }
    return n;
}

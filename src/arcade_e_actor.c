#include "arcade_e_actor.h"
#include "arcade_bonus_art.h"
static void animate(ArcadeEActor *a)
{
    const ArcadeEFrame *f;
    if(a->anim_timer){a->anim_timer--;return;}
    f=&arcade_e_frames[a->animation];a->code=f->code;a->attr=f->attr;
    a->anim_timer=f->timer;a->animation=f->next;
}
static void pose(ArcadeEActor *a,u16 index)
{a->animation=index;a->anim_timer=0;animate(a);}
static void direction_pose(ArcadeEActor *a)
{if(a->direction)pose(a,arcade_e_poses[a->direction-1]);}
static void add_x(ArcadeEActor *a,s32 delta)
{
    u32 x=(((u32)a->x<<8)|a->xf)+(u32)delta;
    a->x=(u16)(x>>8);a->xf=(u8)x;
}
static void move(ArcadeEActor *a,u8 stopped)
{
    const ArcadeEVector *v=&arcade_e_vectors[stopped][a->direction];
    u16 y=(u16)((((u16)a->y<<8)|a->yf)+(u16)v->dy);
    add_x(a,v->dx);a->y=(u8)(y>>8);a->yf=(u8)y;animate(a);
}
static void edge_x(ArcadeEActor *a,u16 camera)
{if((u8)(a->x-camera)>=248){a->state=3;a->phase=2;}}
static void edge_y(ArcadeEActor *a)
{if(a->y>=248){a->state=3;a->phase=2;}}
static void bonus_animate(ArcadeEActor *a)
{
    const ArcadeBonusFrame *f;
    if(a->anim_timer){a->anim_timer--;return;}
    f=&arcade_bonus_frames[a->animation];a->code=f->code;a->attr=f->attr;
    a->anim_timer=f->timer;a->animation=f->next;
}
void arcade_e_actor_step(ArcadeEActor *a,u8 slot,u16 camera,u8 clock,u8 stopped,
    u8 purge,u16 target_x,u8 target_y,u8 target,u8 eligible,
    u8 *live,u8 *available,u8 *next,u8 limit)
{
    if(!a->active)return;
    if(a->state==255)
    {
        if(!a->phase)
        {
            if(*next>10)return;
            a->phase++;a->active=(u8)((a->active&253)|64);
            a->bonus_kind=*next;*next=(u8)(*next+1);if(*next>limit)*next=0;
            a->animation=arcade_bonus_roots[a->bonus_kind];a->anim_timer=0;bonus_animate(a);
        }
        else if(a->phase==2)
        {a->phase++;bonus_animate(a);if(a->owner)a->attr=0xc3;}
        edge_x(a,camera);
    }
    else if(!a->state)
    {
        if(!a->phase)
        {
            a->active=1;direction_pose(a);--*available;++*live;
            a->turns=9;a->phase++; /* C332 returns before delay handling. */
        }
        else if(a->phase==1)
        {
            if(!a->delay){a->state++;a->phase=0;}
            else {a->delay--;if(!stopped)add_x(a,141);}
        }
    }
    else if(a->state==1)
    {
        switch(a->phase)
        {
            case 0:
                if(!((u8)(clock+1-slot)&1))
                {
                    a->target=target;
                    if(!eligible){a->state=2;a->phase=0;break;}
                    a->direction=arcade_e_turn(a->direction,a->x,a->y,target_x,target_y);
                    direction_pose(a);a->delay=8;a->phase++;
                    if(!--a->turns){a->turns=9;a->phase=2;}
                }
                move(a,stopped);edge_x(a,camera);edge_y(a);break;
            case 1:
                if(!--a->delay)a->phase--;
                move(a,stopped);edge_x(a,camera);edge_y(a);break;
            case 2:a->delay=16;a->phase++;
                /* C3AA falls directly through to pause decrement. */
                /* fall through */
            case 3:
                if(!--a->delay)a->phase=0;
                animate(a);edge_x(a,camera);edge_y(a);
                if(!stopped)add_x(a,141);break;
        }
    }
    else if(a->state==2)
    {
        if(!a->phase){a->direction=9;direction_pose(a);a->phase++;}
        if(a->phase==1){move(a,stopped);edge_x(a,camera);}
    }
    else if(a->state==3)
    {
        switch(a->phase)
        {
            case 0:pose(a,arcade_e_death[a->owner]);a->timer=32;a->phase++;break;
            case 1:if(--a->timer)animate(a);else a->phase++;break;
            case 2:a->active=a->state=a->phase=0;++*available;--*live;break;
        }
    }
    if(purge && (u16)(a->x-camera-8)<=232){a->state=255;a->phase=0;}
}

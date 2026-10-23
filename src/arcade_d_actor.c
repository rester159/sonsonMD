#include "arcade_d_actor.h"
#include "arcade_bonus_art.h"
static void animate(ArcadeDActor *a)
{
    const ArcadeDFrame *f;
    if(a->anim_timer){a->anim_timer--;return;}
    f=&arcade_d_frames[a->animation];a->code=f->code;a->attr=f->attr;
    a->anim_timer=f->timer;a->animation=f->next;
}
static void pose(ArcadeDActor *a,u16 index)
{a->animation=index;a->anim_timer=0;animate(a);}
static void direction_pose(ArcadeDActor *a)
{if(a->direction)pose(a,arcade_d_poses[a->direction-1]);}
static void add_x(ArcadeDActor *a,s16 delta)
{
    u32 x=(((u32)a->x<<8)|a->xf)+(u32)(s32)delta;
    a->x=(u16)(x>>8);a->xf=(u8)x;
}
static void move(ArcadeDActor *a,u8 stopped)
{
    const ArcadeDVector *v=&arcade_d_vectors[stopped][a->direction];
    u16 y=(u16)((((u16)a->y<<8)|a->yf)+(u16)v->dy);
    add_x(a,v->dx);a->y=(u8)(y>>8);a->yf=(u8)y;animate(a);
}
static void edge(ArcadeDActor *a,u16 camera)
{if((u8)(a->x-camera)>=248){a->state=4;a->phase=2;}}
static void bonus_animate(ArcadeDActor *a)
{
    const ArcadeBonusFrame *f;
    if(a->anim_timer){a->anim_timer--;return;}
    f=&arcade_bonus_frames[a->animation];a->code=f->code;a->attr=f->attr;
    a->anim_timer=f->timer;a->animation=f->next;
}
void arcade_d_actor_step(ArcadeDActor *a,u8 slot,u16 camera,u8 clock,u8 difficulty,
    u8 stopped,u8 purge,u16 target_x,u8 target_y,u8 target,u8 *live,u8 *next,u8 limit)
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
        edge(a,camera);
    }
    else if(!a->state)
    {
        if(!a->phase){direction_pose(a);++*live;a->phase++;}
        if(a->phase==1)
        {
            if(!a->delay){a->active=1;a->state++;a->phase=0;}
            else {a->delay--;if(!stopped)add_x(a,141);}
        }
    }
    else if(a->state==1 && !a->phase)
    {
        move(a,stopped);
        if((u16)(a->target_offset+camera-a->x+4)<=8){a->state++;a->phase=0;}
    }
    else if(a->state==2)
    {
        switch(a->phase)
        {
            case 0:a->direction=0;a->delay=arcade_d_wait[(difficulty&48)>>4];a->phase++;break;
            case 1:move(a,stopped);if(!--a->delay)a->phase++;break;
            case 2:
                if(!((u8)(clock+3-slot)&7))
                {
                    a->target=target;a->direction=arcade_d_aim(a->x,a->y,target_x,target_y);
                    direction_pose(a);a->delay=24;a->phase++;
                }
                break;
            case 3:
                move(a,stopped);
                if(a->y>=248){a->state=4;a->phase=2;}
                edge(a,camera);break;
        }
    }
    else if(a->state==4)
    {
        switch(a->phase)
        {
            case 0:pose(a,arcade_d_death[a->owner]);a->timer=32;a->phase++;break;
            case 1:if(--a->timer)animate(a);else a->phase++;break;
            case 2:a->active=255;a->phase++;break;
            case 3:a->active=a->state=a->phase=0;--*live;break;
            case 4:pose(a,arcade_d_score[a->owner]);a->timer=32;a->phase=1;break;
        }
    }
    if(purge && (u16)(a->x-camera-8)<=232){a->state=255;a->phase=0;}
}

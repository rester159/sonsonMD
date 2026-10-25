#include "arcade_c_actor.h"
#include "arcade_bonus_art.h"
static void animate(ArcadeCActor *a)
{
    const ArcadeCFrame *f;
    if(a->anim_timer){a->anim_timer--;return;}
    f=&arcade_c_frames[a->animation];a->code=f->code;a->attr=f->attr;
    a->anim_timer=f->timer;a->animation=f->next;
}
static void pose(ArcadeCActor *a,u16 index)
{a->animation=index;a->anim_timer=0;animate(a);}
static void sequence(ArcadeCActor *a)
{
    const ArcadeCScript *s;
    if(a->script_timer){a->script_timer--;return;}
    s=&arcade_c_script[a->script++];a->direction=s->direction;
    a->script_timer=s->timer;pose(a,s->animation);
}
static void add_x(ArcadeCActor *a,s16 delta)
{
    u32 x=(((u32)a->x<<8)|a->xf)+(u32)(s32)delta;
    a->x=(u16)(x>>8);a->xf=(u8)x;
}
static void edge(ArcadeCActor *a,u16 camera)
{if((u8)(a->x-camera)>248){a->state=2;a->phase=2;}}
static void bonus_animate(ArcadeCActor *a)
{
    const ArcadeBonusFrame *f;
    if(a->anim_timer){a->anim_timer--;return;}
    f=&arcade_bonus_frames[a->animation];a->code=f->code;a->attr=f->attr;
    a->anim_timer=f->timer;a->animation=f->next;
}
void arcade_c_actor_step(ArcadeCActor *a,u16 camera,u8 stopped,u8 purge,
                         u8 *live,u8 *next,u8 limit)
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
        if(!a->phase)
        {
            pose(a,arcade_c_initial);a->script=0;a->script_timer=0;sequence(a);
            ++*live;a->phase++;
        }
        if(a->phase==1)
        {
            if(!a->delay){a->active=1;a->state++;a->phase=0;}
            else{a->delay--;if(!stopped)add_x(a,141);}
        }
    }
    else if(a->state==1 && !a->phase)
    {
        const ArcadeCVector *v=&arcade_c_vectors[stopped!=0][a->direction];
        u16 y=(u16)((((u16)a->y<<8)|a->yf)+(u16)v->dy);
        add_x(a,v->dx);a->y=(u8)(y>>8);a->yf=(u8)y;animate(a);
        sequence(a);edge(a,camera);
        if(a->direction&128){a->state=2;a->phase=2;}
    }
    else if(a->state==2)
    {
        switch(a->phase)
        {
            case 0:pose(a,arcade_c_death[a->owner]);a->timer=32;a->phase++;break;
            case 1:if(--a->timer)animate(a);else a->phase++;break;
            case 2:a->active=255;a->phase++;break;
            case 3:a->active=a->state=a->phase=0;--*live;break;
            case 4:pose(a,arcade_c_score[a->owner]);a->timer=32;a->phase=1;break;
        }
    }
    /* BBF1 calls A92E even after a cleanup handler clears active. */
    if(purge && (u16)(a->x-camera-8)<=232){a->state=255;a->phase=0;}
}

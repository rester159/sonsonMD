#include "arcade_main_actor.h"
static void animate(ArcadeMainActor *a)
{
    const ArcadeMainFrame *f;
    if(a->anim_timer){a->anim_timer--;return;}
    f=&arcade_main_frames[a->cursor];a->code=f->code;a->attr=f->attr;
    a->anim_timer=f->timer;a->cursor=f->next;
}
static void pose(ArcadeMainActor *a,u16 cursor)
{a->cursor=cursor;a->anim_timer=0;animate(a);}
static void add_x(ArcadeMainActor *a,s16 delta)
{
    u32 x=(((u32)a->x<<8)|a->xf)+(u32)(s32)delta;
    a->x=(u16)(x>>8);a->xf=(u8)x;
}
static void move(ArcadeMainActor *a)
{
    u16 y=(u16)((((u16)a->y<<8)|a->yf)+(u16)arcade_main_dy[a->direction]);
    add_x(a,arcade_main_dx[a->direction]);a->y=(u8)(y>>8);a->yf=(u8)y;animate(a);
}
static void edge(ArcadeMainActor *a,u16 camera)
{if((u8)(a->x-camera)>=248){a->state=5;a->phase=2;}}
void arcade_main_actor_post(ArcadeMainActor *a,u16 camera,u8 purge)
{
    /* B9CD/B9D0 also run after the bonus handler changes state at the edge. */
    if(!(a->state&128) && a->hit){a->hit=0;pose(a,arcade_main_wounded);}
    if(purge && (u16)(a->x-camera-8)<=232){a->state=255;a->phase=0;}
}
void arcade_main_actor_step(ArcadeMainActor *a,u8 slot,u16 camera,u8 clock,u8 stopped,u8 purge,u8 *live)
{
    if(!a->active || a->state==255)return;
    if(a->state==0)
    {
        a->active|=2;a->hp=5;a->x=(u16)(camera+248);
        pose(a,arcade_main_walk[(16-slot)&3]);
        ++*live;a->state=1;a->phase=0;
        /* BA31 falls directly into the delay state on this same update. */
    }
    switch(a->state)
    {
        case 1:
            if(a->phase==0)
            {
                if(!a->delay)a->phase++;
                else
                {
                    if(!stopped)add_x(a,141);
                    if(!(clock&31))a->delay--;
                }
            }
            else if(a->phase==1)
            {
                move(a);
                if((u16)(a->x-camera)<246){a->state=(u8)(2+a->variant);a->phase=0;a->active=1;}
            }
            break;
        case 2:
            if(!a->phase){a->direction=0;a->phase++;}
            move(a);edge(a,camera);break;
        case 3:
            if(!a->phase){a->direction=3;a->phase++;}
            move(a);edge(a,camera);break;
        case 4:
            if(!a->phase){a->direction=3;a->origin=a->x;a->phase++;}
            else if(a->phase==1)
            {
                move(a);
                if((u16)(a->origin-a->x)>48){a->origin=a->x;a->direction=1;a->phase++;}
            }
            else if(a->phase==2)
            {
                move(a);
                if((u16)(a->x-a->origin)>24){a->origin=a->x;a->direction=3;a->phase--;}
            }
            edge(a,camera);break;
        case 5:
            if(!a->phase){pose(a,arcade_main_death[a->owner]);a->timer=32;a->phase++;}
            else if(a->phase==1){if(--a->timer)animate(a);else a->phase++;}
            else if(a->phase==2){a->active=a->state=a->phase=0;--*live;}
            break;
    }
    arcade_main_actor_post(a,camera,purge);
}

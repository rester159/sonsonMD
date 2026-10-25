#include "arcade_life.h"
static void animate(ArcadeLife *p)
{
    u8 k=p->cursor;
    if(p->anim_timer){p->anim_timer--;return;}
    if(k<8)
    {
        p->code=(u8)(0x42+(k&3)+(k>=4?8:0));p->attr=k<4?0xc0:0xc1;
        p->anim_timer=2;p->cursor=(u8)((k&~3)|((k+1)&3));
    }
    else if(k<16)
    {
        static const u8 frames[8]={0x40,0x41,0x50,0x41,0x48,0x49,0x51,0x49};
        p->code=frames[k-8];p->attr=k<12?0xc0:0xc1;
        p->anim_timer=4;p->cursor=(u8)((k&~3)|((k+1)&3));
    }
    else
    {p->code=k==16?0x53:0x54;p->attr=k==16?0xc0:0xc1;p->anim_timer=127;}
}
static void select_animation(ArcadeLife *p,u8 cursor)
{p->cursor=cursor;p->anim_timer=0;animate(p);}
void arcade_life_animation_step(ArcadeLife *p){animate(p);}
u8 arcade_life_step(ArcadeLife *p,u16 camera,u8 stopped,u8 *reserves,u8 *gate,u16 *event,u8 *sound)
{
    u16 fraction;
    *event=0xffff;*sound=0;
    if(p->state>6)return 0;
    switch(p->state)
    {
        case 0:
            if(!p->phase)
            {
                *sound=0x0a;p->active=2;select_animation(p,p->owner*4);
                p->timer=60;p->phase++;
            }
            if(p->phase==1)
            {if(--p->timer)animate(p);else{p->phase=0;p->state++;}}
            break;
        case 1:case 2:
            animate(p);p->y=(u8)(p->y-2);
            if(p->y>=248){p->y=248;p->state=3;}
            break;
        case 3:
            select_animation(p,(u8)(8+p->owner*4));p->facing=1;p->y=248;p->state++;
            break;
        case 4:
            if(*reserves){p->state=5;p->phase=0;}
            else{*event=(u16)(0x20e + p->owner);*gate=1;p->kind=0;p->state=4;p->phase=0;}
            break;
        case 5:
            --*reserves;*event=(u16)(0xc00|p->owner);p->state=6;p->phase=0;
            break;
        case 6:
            select_animation(p,(u8)(16+p->owner));p->x=(u16)(camera-8);p->y=104;
            p->facing=1;p->reentry_lock=16;p->state++;p->phase=0;
            break;
    }
    if(!stopped){fraction=(u16)(p->xf+141);p->xf=(u8)fraction;p->x=(u16)(p->x+(fraction>>8));}
    return 1;
}

static void reentry_move(ArcadeLife *p)
{
    static const s16 dx[9]={0,288,203,0,-203,-288,-203,0,203};
    static const s16 dy[9]={0,0,-203,-288,-203,0,203,288,203};
    s32 x=(s32)p->xf+dx[p->facing];
    u16 y=(u16)(((u16)p->y<<8)|p->yf);
    p->x=(u16)(p->x+(s16)(x>>8));p->xf=(u8)x;
    y=(u16)(y+dy[p->facing]);p->y=(u8)(y>>8);p->yf=(u8)y;
}
static void reentry_release(ArcadeLife *p)
{
    select_animation(p,(u8)(8+p->owner*4));p->facing=1;p->active=1;
    p->kind=p->state=p->phase=0;
}
u8 arcade_life_reentry(ArcadeLife *p,u8 *path,u8 clock,u8 stopped,
                       u8 input,u8 previous,ArcadeFirePose *fire)
{
    static const u8 directions[4]={1,5,1,255};
    static const u8 durations[4]={152,80,28,0};
    u8 fire_ready=0;
    u16 fraction;
    switch(p->phase)
    {
        case 0:p->timer=16;p->phase=1; /* fall through */
        case 1:
            if(--p->timer){reentry_move(p);break;}
            p->facing=directions[0];p->timer=durations[0];*path=1;p->phase++;
            /* Source falls into phase2 and moves/decrements on this update. */
        case 2:
            reentry_move(p);
            if(! --p->timer)
            {
                p->facing=directions[*path];p->timer=durations[*path];++*path;
                if(p->facing==255){p->facing=0;p->timer=6;p->phase++;}
            }
            break;
        case 3:
            reentry_move(p);
            if(p->timer<=3 && !(clock&15))
                p->code=(clock&16)?(p->owner?0x48:0x40):(p->owner?0x54:0x53);
            if(!(clock&63) && ! --p->timer)reentry_release(p);
            break;
    }
    if(p->reentry_lock)p->reentry_lock--;
    else
    {
        fire_ready=1;fire->x=p->x;fire->y=p->y;fire->facing=p->kind?1:p->facing;
        if(input&0x3c)
        {
            u8 edge=(u8)(input & (u8)~previous);
            reentry_release(p);
            /* AACF only selects a fresh climb state; it does not move now. */
            if(edge&0x10)p->state=2;
            else if((edge&0x20) && p->y>40)p->state=3;
        }
    }
    if(!stopped){fraction=(u16)(p->xf+141);p->xf=(u8)fraction;p->x=(u16)(p->x+(fraction>>8));}
    return fire_ready;
}

#include "arcade_i.h"
/* Source D50C flight and D4CB/D4D9/D4EB/D4F9 death scripts, flattened
 * into semantic frames. Cursor is an index, never an arcade ROM pointer. */
static void animate(ArcadeIActor *a)
{
    u8 k;
    if(a->anim_timer){a->anim_timer--;return;}
    if(a->cursor<14)
    {
        k=a->cursor;a->code=0x6d-k;
        a->anim_timer=k==13?15:(k==6 || k==7)?6:7;
        a->cursor=k==13?0:k+1;
    }
    else
    {
        k=(a->cursor-14)&3;
        a->code=k<3?0x1a+k:a->cursor<18?0:a->cursor<22?3:0x31;
        a->attr=k<3 || a->cursor<22?0xc7:a->cursor<26?0xc4:0xc3;
        a->anim_timer=k<3?2:127;
        if(k<3)a->cursor++;
    }
}
static void flight(ArcadeIActor *a)
{if(a->body.facing){a->cursor=0;a->anim_timer=0;animate(a);}}
static void add_x(ArcadeFormationActor *b,u32 delta)
{
    u32 x=(((u32)b->x<<8)|b->xf)+delta;
    b->x=(u16)(x>>8);b->xf=(u8)x;
}
void arcade_i_actor_step(ArcadeIActor *a,u16 camera,u8 stopped,u8 *live)
{
    ArcadeFormationActor *b=&a->body;
    u16 y;
    if(!b->active)return;
    switch(b->state)
    {
        case 0:
            b->active=1;a->x_speed=0x00008d00;a->y_speed=0x044000;
            a->hop_timer=0;b->x=(u16)(camera+232);b->y=248;
            flight(a);a->attr=b->facing==1?0x89:b->facing==3?0xc9:0;
            ++*live;b->state++;b->phase=0;break;
        case 1:case 2:
            add_x(b,0-(a->x_speed>>8));
            y=(u16)(((u16)b->y<<8)|b->yf);
            y=(u16)(b->state==1?y+(a->y_speed>>8):y-(a->y_speed>>8));
            b->y=(u8)(y>>8);b->yf=(u8)y;animate(a);
            if(b->state==1)
            {
                a->hop_timer++;a->x_speed+=24;a->y_speed=(a->y_speed-0x1210)&0xffffff;
                if((a->y_speed>>8)<=0x12){b->state++;b->phase=0;}
            }
            else
            {
                a->x_speed-=24;a->y_speed=(a->y_speed+0x1210)&0xffffff;
                if(! --a->hop_timer){b->state++;b->phase=0;}
            }
            if((u8)(b->x-camera)>=248){b->state=4;b->phase=2;}
            if(!stopped)add_x(b,141);
            break;
        case 3:
            flight(a);b->x=(u16)(b->x-16);b->y=248;b->yf=0;b->state=1;b->phase=0;break;
        case 4:
            switch(b->phase)
            {
                case 0:case 4:
                    a->cursor=(u8)(14+a->hit_style*4+(b->phase==4?8:0));
                    a->anim_timer=0;animate(a);a->y_speed=(a->y_speed&0xffff00)|32;
                    b->phase=1;break;
                case 1:
                    a->y_speed=(a->y_speed&0xffff00)|(u8)(a->y_speed-1);
                    if((u8)a->y_speed)animate(a);else b->phase++;
                    break;
                case 2:b->active=255;b->phase++;break;
                case 3:b->active=b->state=b->phase=0;--*live;break;
            }
            break;
    }
}

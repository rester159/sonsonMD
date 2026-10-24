#include "arcade_orb.h"
static void animate(ArcadeOrb *a)
{
    static const u8 small[4]={0x92,0x98,0x99,0x9a};
    u8 frame;
    if(a->anim_timer){a->anim_timer--;return;}
    frame=a->cursor;
    a->code=a->variant?(u8)(0xba+frame):small[frame];
    a->attr=a->variant?0x89:0xf8;
    a->anim_timer=3;
    a->cursor=(u8)(frame+1);
    if(a->cursor==(a->variant?6:4))a->cursor=0;
}
static void move(ArcadeOrb *a)
{
    /* E27D five-byte vectors: signed 24-bit X and wrapping 16-bit Y. */
    static const s16 dx[5]={0,320,448,-320,-448};
    u32 x=((u32)a->x<<8)|a->subx;
    x=(u32)(x+dx[a->direction])&0xffffff;
    a->x=(u16)(x>>8);a->subx=(u8)x;
}
void arcade_orb_step(ArcadeOrb *a,ArcadePowerup *parent,u16 camera)
{
    if(!a->active)return;
    switch(a->state)
    {
        case 0:
            a->active=1;a->y=(u8)(parent->y+(a->variant?16:0));
            a->cursor=0;a->anim_timer=0;animate(a);
            a->x=(u16)(camera-8);a->direction=1;a->state++;
            break;
        case 1:
            if(!a->phase){a->timer=16;a->phase++;}
            if(a->phase==1)
            {
                move(a);
                if(--a->timer){animate(a);break;}
                a->phase++;
            }
            if(a->phase==2)
            {
                move(a);animate(a);
                /* Source CMPB tests only the low byte of X-camera. */
                if((u8)(a->x-camera)>248){a->state=2;a->phase=0;}
            }
            break;
        case 2:
            parent->active=1;parent->x=a->x;parent->state=parent->phase=0;
            a->active=a->state=a->phase=0;
            break;
    }
}

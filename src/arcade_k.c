#include "arcade_k.h"
static void animate(ArcadeKProjectile *a)
{
    u8 k;
    if(a->anim_timer){a->anim_timer--;return;}
    k=a->cursor;a->code=(u8)(0x89+k);a->attr=0xe4;
    a->anim_timer=k==8?127:3;a->cursor=k==3?0:k==8?4:(u8)(k+1);
}
static void add_x(ArcadeKProjectile *a,s16 dx)
{
    u32 x=(((u32)a->x<<8)|a->xf)+(u32)(s32)dx;
    a->x=(u16)(x>>8);a->xf=(u8)x;
}
void arcade_k_projectile_step(ArcadeKProjectile *a,u16 camera,u8 stopped,u8 purge)
{
    if(!a->active)return;
    switch(a->state)
    {
        case 0:
            a->cursor=0;a->anim_timer=0;animate(a);
            a->origin=(u16)((a->origin&0xff00)|6);a->state++;a->phase=0;break;
        case 1:
            a->origin=(u16)((a->origin&0xff00)|(u8)(a->origin-1));
            if(!(u8)a->origin){a->state=2;a->phase=0;}
            break;
        case 2:
            if(!a->phase){a->origin=a->x;a->phase++;}
            if(a->facing==1)add_x(a,608);
            else if(a->facing==3)add_x(a,-608);
            animate(a);
            if((u8)(a->x-camera)>=248){a->state=3;a->phase=2;}
            if(!stopped)add_x(a,141);
            break;
        case 3:
            if(!a->phase)
            {a->cursor=4;a->anim_timer=0;animate(a);a->death_timer=16;a->phase++;}
            else if(a->phase==1)
            {if(--a->death_timer)animate(a);else a->phase++;}
            else if(a->phase==2)a->active=a->state=a->phase=0;
            break;
    }
    /* CBC8 invokes CBEB after the handler, including its cleanup update. */
    if(purge){a->state=3;a->phase=2;}
}

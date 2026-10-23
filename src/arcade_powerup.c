#include "arcade_powerup.h"
u8 arcade_powerup_collect_conversion(ArcadePowerup *a,u16 x,u8 y,u8 owner,
                                    u8 *limit,u8 *next,u8 *conversion)
{
    if(a->kind!=255 || a->active!=1)return 0;
    if((u8)(y-a->y+7)>14 || (u16)(x-a->x+3)>6)return 0;
    a->active=2;a->owner=owner;a->state=3;a->phase=0;
    *conversion=1;*next=0;
    *limit=(u8)(*limit+1);
    if(*limit>3)*limit=0;
    return 1;
}
static void animate(ArcadePowerup *a)
{
    static const u8 codes[2][8]={
        {0x37,0x3c,0x3d,0x3e,0x3f,0x3e,0x3d,0x3c},
        {0x82,0x9d,0x9e,0x9f,0,0,0,0}
    };
    u8 family=(u8)(a->kind==255);
    if(a->anim_timer){a->anim_timer--;return;}
    a->code=codes[family][a->cursor];a->attr=family?0xcb:0xf9;
    a->anim_timer=5;
    a->cursor=(u8)((a->cursor+1)&(family?3:7));
}
void arcade_powerup_step(ArcadePowerup *a,u16 camera)
{
    if(!a->active || a->kind<254)return;
    switch(a->state)
    {
        case 0:
            if(!a->phase)
            {
                a->y=(u8)(a->placement<<3);a->x=(u16)(camera+248);
                a->timer=32;a->phase++;a->cursor=0;a->anim_timer=0;animate(a);
            }
            else if(a->phase==1 && !--a->timer){a->state=2;a->phase=0;}
            break;
        case 2:
            animate(a);
            if((u16)(a->x-camera)>=248)
            {if(!a->exit_latched)a->exit_latched=1;a->state=3;a->phase=0;}
            break;
        case 3:a->active=a->state=a->phase=0;break;
    }
}

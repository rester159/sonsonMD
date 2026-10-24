#include "arcade_foe_bonus.h"
static void animate_bonus(ArcadeFoe *a)
{
    const ArcadeBonusFrame *f;
    if(a->anim_timer){a->anim_timer--;return;}
    f=&arcade_bonus_frames[a->cursor];
    a->pose=(u16)(ARCADE_BONUS_POSE|a->cursor);
    a->anim_timer=f->timer;a->cursor=f->next;
}
void arcade_foe_bonus_step(ArcadeFoe *a,u16 camera,u8 purge,u8 *next,u8 limit)
{
    if(!a->body.active)return;
    switch(a->body.phase)
    {
        case 0:
            if(*next>10)return; /* Recovered source table has eleven entries. */
            a->body.phase++;
            a->body.active=(u8)((a->body.active&253)|64);
            a->kind=*next;*next=(u8)(*next+1);if(*next>limit)*next=0;
            a->cursor=arcade_bonus_roots[a->kind];a->anim_timer=0;animate_bonus(a);
            break;
        case 2:
            a->body.phase++;animate_bonus(a);
            if(a->hit_style)a->pose|=ARCADE_BONUS_OWNER2;
            break;
    }
    if((u8)(a->body.x-camera)>=248){a->body.state=6;a->body.phase=2;}
    if(purge && (u16)(a->body.x-camera-8)<=232){a->body.state=255;a->body.phase=0;}
}

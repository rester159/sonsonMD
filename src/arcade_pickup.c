#include "arcade_powerup.h"
#include "arcade_bonus_art.h"
static void animate(ArcadePowerup *a)
{
    const ArcadeBonusFrame *f;
    if(a->anim_timer){a->anim_timer--;return;}
    f=&arcade_bonus_frames[a->cursor];
    a->code=f->code;a->attr=f->attr;a->anim_timer=f->timer;
    a->cursor=(u8)f->next;
}
static u8 retire(ArcadePowerup *a,u16 camera)
{
    u8 orb=0;
    if((u16)(a->x-camera)<248)return 0;
    if(!a->exit_latched)
    {
        a->exit_latched=1;
        if(a->kind<=9 && !a->anim_timer)
        {
            orb=(u8)(a->kind==9?2:1);
            a->kind>>=1;
        }
    }
    a->state=3;a->phase=0;
    return orb;
}
static u8 pickup_step(ArcadePowerup *a,u16 camera,u8 variant)
{
    u8 orb=0;
    if(!a->active)return 0;
    switch(a->state)
    {
        case 0:
            if(!a->phase)
            {
                a->y=(u8)(a->placement<<3);a->x=(u16)(camera+248);
                a->timer=32;a->phase++;
                a->cursor=(u8)arcade_bonus_roots[a->kind];a->anim_timer=0;
                animate(a);
            }
            else if(a->phase==1 && !--a->timer){a->state=2;a->phase=0;}
            break;
        case 1:
            if(!a->phase)
            {
                if(a->kind==10)
                {
                    a->code=arcade_variant_score_codes[variant];
                    a->attr=a->owner?0xc3:0xc4;a->anim_timer=127;
                    /* B213/9617 consumes a terminal score record. Its source
                     * cursor is never animated again in this state. Preserve
                     * a distinct opaque token without a source ROM pointer. */
                    a->cursor=(u8)(22+variant*2+a->owner);
                }
                else {animate(a);if(a->owner)a->attr=0xc3;}
                a->timer=96;a->phase++;
            }
            if(a->phase==1)
            {
                orb=retire(a,camera);
                if(!--a->timer){a->state=3;a->phase=0;}
            }
            break;
        case 2:orb=retire(a,camera);break;
        case 3:a->active=a->state=a->phase=0;break;
    }
    return orb;
}

u8 arcade_pickup_step(ArcadePowerup *a,u16 camera)
{
    if(a->kind>9)return 0;
    return pickup_step(a,camera,0);
}
u8 arcade_variant_pickup_step(ArcadePowerup *a,u16 camera,u8 variant)
{
    /* Normal A7D0 grade saturates at6; unsupported injected values must not
     * read beyond the recovered table. Owner is the source player0/1. */
    if(a->kind!=10 || variant>6 || a->owner>1)return 0;
    return pickup_step(a,camera,variant);
}

#include "arcade_main_actor.h"
#include "arcade_bonus_art.h"
static void animate(ArcadeMainActor *a)
{
    const ArcadeBonusFrame *f;
    if(a->anim_timer){a->anim_timer--;return;}
    f=&arcade_bonus_frames[a->cursor];a->code=f->code;a->attr=f->attr;
    a->anim_timer=f->timer;a->cursor=f->next;
}
void arcade_main_bonus_step(ArcadeMainActor *a,u16 camera,u8 purge,u8 *next,u8 limit)
{
    if(!a->active || a->state!=255)return;
    if(a->phase==0)
    {
        if(*next>10)return; /* Recovered B291 has eleven valid kinds. */
        a->phase++;a->active=(u8)((a->active&253)|64);
        a->hit=*next;*next=(u8)(*next+1);if(*next>limit)*next=0;
        a->cursor=arcade_bonus_roots[a->hit];a->anim_timer=0;animate(a);
    }
    else if(a->phase==2)
    {
        a->phase++;animate(a);if(a->owner)a->attr=0xc3;
    }
    if((u8)(a->x-camera)>=248){a->state=5;a->phase=2;}
    arcade_main_actor_post(a,camera,purge);
}

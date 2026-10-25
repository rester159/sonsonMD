#include "arcade_shots.h"
/* Source AFEC/AFF4: two frames each, duration 1 (load then decrement tick). */
static void animate(ArcadePlayerShot *a)
{
    if(a->anim_timer){a->anim_timer--;return;}
    a->pose=a->cursor;a->cursor^=1;a->anim_timer=1;
}
void arcade_player_shot_step(ArcadePlayerShot *a,u16 camera,u8 *available)
{
    u8 newborn=(u8)(a->state==0);
    if(!a->active)return;
    switch(a->state)
    {
        case 0:
            --*available;a->origin=a->x;
            a->cursor=0;a->anim_timer=0;animate(a);a->state++;
            /* Source B05E falls through the first flight update. */
        case 1:
            /* On the birth tick, the source animation call redirects its parent
             * pointer into ROM, so an immediate offscreen release does not
             * restore parent availability. Preserve this observed edge case. */
            if((u8)(a->x-camera)>=240)
            {a->active=a->state=a->phase=0;if(!newborn)++*available;}
            /* B0C0 returns to B075 even after releasing the shot. */
            if(a->facing==1)a->x=(u16)(a->x+6);
            else if(a->facing==3)a->x=(u16)(a->x-6);
            if((u16)(a->x-a->origin+96)<192)animate(a);
            else a->state=2;
            break;
        case 2:
            a->cursor=2;a->anim_timer=0;a->timer=4;a->state++;
            animate(a);break;
        case 3:
            if(--a->timer)animate(a);
            else {++*available;a->state=a->phase=a->active=0;}
            break;
    }
}

/* AD10 only writes integer position, facing, active and state/phase. */
u8 arcade_player_shot_fire(const ArcadeFireInput *p,u8 first_active,ArcadeShotBirth *birth)
{
    if(!p->available || !(p->input & (u8)~p->previous & 1))return 255;
    birth->x=p->x;birth->y=(u8)(p->y-(p->state==5?8:0));
    birth->facing=p->kind?1:p->facing;
    return first_active?1:0;
}

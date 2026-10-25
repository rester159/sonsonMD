#include "arcade_foes.h"

static void animate(ArcadeFoe *a)
{
    const ArcadeFoeFrame *f;
    if(a->anim_timer){a->anim_timer--;return;}
    a->pose=a->cursor;f=&arcade_foe_frames[a->cursor];
    a->anim_timer=f->duration;a->cursor=f->next;
}
static void select_animation(ArcadeFoe *a,u16 first)
{a->cursor=first;a->anim_timer=0;animate(a);}
static void walking_animation(ArcadeFoe *a,u8 family)
{
    u8 d=a->body.facing;
    if(!d)return;
    select_animation(a,(family?arcade_foe_h_walk:a->kind?arcade_foe_g_unarmed:arcade_foe_g_walk)[d-1]);
}
static void add_y(ArcadeFormationActor *a,s16 velocity)
{
    u16 y=(u16)(((u16)a->y<<8)|a->yf);
    y=(u16)(y+(u16)velocity);a->y=(u8)(y>>8);a->yf=(u8)y;
}
static void move(ArcadeFoe *a,u8 family)
{
    const ArcadeFoeVector *v=&arcade_foe_vectors[family?2:a->kind?1:0][a->body.facing];
    /* Extracted dx is -384..384; with xf0..255 the sum is -384..639. */
    s16 x=(s16)((s16)a->body.xf+v->dx);
    a->body.x=(u16)(a->body.x+(s16)(x>>8));a->body.xf=(u8)x;
    add_y(&a->body,v->dy);animate(a);
}
static const ArcadeFoePlayer *target(const ArcadeFoe *a,const ArcadeFoeWorld *w)
{return a->target?&w->players[a->target-1]:&arcade_foe_empty_target;}
static u8 choose_direction(ArcadeFoe *a,u8 family,const ArcadeFoeWorld *w)
{
    const ArcadeFoePlayer *p;
    u8 k=0,eligible0,eligible1,window=family?8:4;
    if((family?!a->kind:a->kind) || (w->players[0].kind|w->players[1].kind))return family?3:1;
    eligible0=(u8)((~w->players[0].kind)&w->players[0].active&1);
    eligible1=(u8)((~w->players[1].kind)&w->players[1].active&1);
    if(w->tracking && eligible1 && (!eligible0 || w->players[1].x>w->players[0].x))k=1;
    a->target=(u8)(k+1);p=&w->players[k];
    if(!(k?eligible1:eligible0))
    {if(!family){a->body.state=3;a->body.phase=0;}return 1;}
    if((u8)(p->y-a->body.y+window)<=window*2)return a->body.x>p->x?3:1;
    a->body.phase=0;
    if(a->body.y>p->y){a->body.state=4;return 2;}
    a->body.state=5;return 4;
}
static void face_target(ArcadeFoe *a,const ArcadeFoeWorld *w)
{a->body.facing=a->body.x>target(a,w)->x?3:1;}
static void offscreen(ArcadeFoe *a,const ArcadeFoeWorld *w)
{if((u8)(a->body.x-w->camera)>=248){a->body.state=6;a->body.phase=2;}}
static void walking(ArcadeFoe *a,u8 family,u8 slot,const ArcadeFoeWorld *w,ArcadeFloor floor,void *ctx)
{
    u8 d;
    s16 adjust;
    if(a->body.phase==0 && ((u8)(5-slot+w->clock)&7)==0)
    {a->body.delay=family?50:80;a->body.phase=1;}
    if(a->body.phase==0)move(a,family);
    else if(a->body.phase==1)
    {if(--a->body.delay)move(a,family);else a->body.phase=2;}
    else
    {
        a->body.phase=0;d=choose_direction(a,family,w);
        if(a->body.facing!=d){a->body.facing=d;walking_animation(a,family);}
    }
    if(!family && !a->kind)
    {
        const ArcadeFoePlayer *p=target(a,w);
        if((u8)(p->y-a->body.y+4)<=8 && (w->stage>=24 || a->body.x>p->x))
        {a->body.state=3;a->body.phase=0;}
    }
    a->body.y--;
    adjust=floor(a->body.x,a->body.y,ctx);
    if(adjust==ARCADE_NO_FLOOR){a->body.state=4;a->body.phase=0;}
    else a->body.y=(u8)(a->body.y+adjust);
    offscreen(a,w);
}
static void throwing(ArcadeFoe *a,const ArcadeFoeWorld *w,ArcadeFoeWeapon weapons[4],u8 slot,u8 reserved)
{
    u8 i;
    switch(a->body.phase)
    {
        case 0:
            select_animation(a,arcade_foe_g_throw[a->body.facing]);
            a->body.delay=16;a->body.phase=1;break;
        case 1:
            move(a,0);
            if(--a->body.delay){animate(a);break;}
            a->body.phase=2;
            /* Source windup falls through to the spawn transaction. */
            /* fall through */
        case 2:
            for(i=0;i<4;i++)if(!weapons[i].active && !(reserved&(1u<<i)))break;
            if(!w->shot_budget || a->kind || i==4)
            {walking_animation(a,0);a->body.state=2;a->body.phase=1;break;}
            weapons[i].x=a->body.x;weapons[i].y=a->body.y;
            weapons[i].active=1;weapons[i].state=weapons[i].phase=0;
            weapons[i].parent=slot;
            weapons[i].facing=target(a,w)->x>a->body.x?1:3;
            a->body.facing=weapons[i].facing;a->kind++;a->body.phase++;
            break;
        case 3:
            select_animation(a,arcade_foe_g_recover[a->body.facing]);
            a->body.delay=16;a->body.phase++;break;
        case 4:
            if(--a->body.delay)animate(a);
            else {a->body.state=2;a->body.phase=1;walking_animation(a,0);}
            break;
    }
    offscreen(a,w);
}
static void descending(ArcadeFoe *a,u8 family,const ArcadeFoeWorld *w,ArcadeFloor floor,void *ctx)
{
    s16 adjust;
    switch(a->body.phase)
    {
        case 0:
            if(family && !a->kind)a->kind=255;
            walking_animation(a,family);a->body.delay=8;a->body.phase++;
            /* fall through */
        case 1:
            animate(a);a->body.y++;
            if(!--a->body.delay){a->body.phase++;a->body.delay=16;animate(a);}
            break;
        case 2:
            a->body.y--;if(!--a->body.delay)a->body.phase++;break;
        case 3:
            add_y(&a->body,arcade_foe_vectors[family?2:0][2].dy);
            adjust=floor(a->body.x,a->body.y,ctx);
            if(adjust!=ARCADE_NO_FLOOR)
            {a->body.y=(u8)(a->body.y+adjust);a->body.phase++;a->body.delay=family?6:8;}
            break;
        case 4:
            if(--a->body.delay)animate(a);
            else
            {face_target(a,w);if(!family)a->body.delay=10;a->body.state=2;a->body.phase=0;walking_animation(a,family);}
            break;
    }
    offscreen(a,w);
}
static void ascending(ArcadeFoe *a,u8 family,const ArcadeFoeWorld *w,ArcadeFloor floor,void *ctx)
{
    s16 adjust;
    switch(a->body.phase)
    {
        case 0:
            if(family && !a->kind)a->kind=255;
            walking_animation(a,family);a->stamp=a->body.y;a->body.delay=8;a->body.phase++;
            /* fall through */
        case 1:
            if(--a->body.delay){animate(a);break;}
            a->body.phase++;
            /* fall through */
        case 2:
            add_y(&a->body,arcade_foe_vectors[family?2:0][4].dy);
            adjust=floor(a->body.x,a->body.y,ctx);
            if(adjust!=ARCADE_NO_FLOOR){a->body.delay=family?6:8;a->body.phase++;}
            else if((u8)(a->body.y-a->stamp)>32){a->body.state=4;a->body.phase=0;}
            break;
        case 3:
            a->body.y++;if(!--a->body.delay)a->body.phase++;break;
        case 4:
            a->body.y--;adjust=floor(a->body.x,a->body.y,ctx);
            if(adjust==ARCADE_NO_FLOOR){animate(a);break;}
            a->body.y=(u8)(a->body.y+adjust);face_target(a,w);
            if(!family)a->body.delay=10;
            a->body.state=2;a->body.phase=0;walking_animation(a,family);break;
    }
    offscreen(a,w);
}
static void dying(ArcadeFoe *a,u8 family)
{
    const u16 *table;
    switch(a->body.phase)
    {
        case 0:case 4:
            table=family?(a->body.phase?arcade_foe_h_exit:arcade_foe_h_death):
                         (a->body.phase?arcade_foe_g_exit:arcade_foe_g_death);
            select_animation(a,table[a->hit_style*5+a->body.facing]);
            a->death_timer=32;a->body.phase=1;break;
        case 1:if(--a->death_timer)animate(a);else a->body.phase++;break;
        case 2:a->body.active=255;a->body.phase++;break;
        case 3:a->body.active=a->body.state=a->body.phase=0;break;
    }
}
void arcade_foe_step(ArcadeFoe *a,u8 family,u8 slot,const ArcadeFoeWorld *w,
                     ArcadeFloor floor,void *ctx,ArcadeFoeWeapon weapons[4])
{
    arcade_foe_step_shared(a,family,slot,w,floor,ctx,weapons,0);
}
void arcade_foe_step_shared(ArcadeFoe *a,u8 family,u8 slot,const ArcadeFoeWorld *w,
                     ArcadeFloor floor,void *ctx,ArcadeFoeWeapon weapons[4],u8 reserved)
{
    if(!a->body.active)return;
    switch(a->body.state)
    {
        case 0:
            if(family)a->kind=10;
            walking_animation(a,family);
            arcade_formation_enter(&a->body,family,w->camera,w->scroll_stopped);
            if(family)animate(a);
            break;
        case 1:
            if(a->body.phase==0)arcade_formation_enter(&a->body,family,w->camera,w->scroll_stopped);
            else
            {
                move(a,family);
                if((u16)(a->body.x-w->camera-8)<224){a->body.state=2;a->body.phase=0;}
            }
            break;
        case 2:walking(a,family,slot,w,floor,ctx);break;
        case 3:if(!family)throwing(a,w,weapons,slot,reserved);break;
        case 4:descending(a,family,w,floor,ctx);break;
        case 5:ascending(a,family,w,floor,ctx);break;
        case 6:dying(a,family);break;
        /* Shared pickup/bonus state $FF is owned by the pending reward system. */
    }
    if(w->purge && (u16)(a->body.x-w->camera-8)<=232){a->body.state=255;a->body.phase=0;}
}
static void weapon_animate(ArcadeFoeWeapon *a)
{
    const ArcadeFoeFrame *f;
    if(a->anim_timer){a->anim_timer--;return;}
    a->pose=a->cursor;f=&arcade_foe_frames[a->cursor];a->cursor=f->next;a->anim_timer=f->duration;
}
static void weapon_animation(ArcadeFoeWeapon *a,u16 first)
{a->cursor=first;a->anim_timer=0;weapon_animate(a);}
void arcade_foe_weapon_step(ArcadeFoeWeapon *a,const ArcadeFoeWorld *w,const ArcadeFoe parents[6])
{
    s32 x;
    if(!a->active)return;
    switch(a->state)
    {
        case 0:
            a->origin=a->x;
            weapon_animation(a,arcade_foe_weapon_init[parents[a->parent].body.facing-1]);
            a->state=1;a->phase=0;break;
        case 1:a->state++;break;
        case 2:
            if(!a->phase)
            {weapon_animation(a,arcade_foe_weapon_fly[a->facing-1]);a->phase++;}
            if((u16)(a->x-a->origin+98)>=196){a->state=3;a->phase=0;}
            if(a->facing)
            {
                x=(s32)a->xf+(a->facing==1?512:-512);
                a->x=(u16)(a->x+(s16)(x>>8));a->xf=(u8)x;
            }
            if((u8)(a->x-w->camera)>=248){a->state=3;a->phase=2;}
            weapon_animate(a);break;
        case 3:
            if(a->phase==0)
            {weapon_animation(a,arcade_foe_weapon_end[a->facing==3?1:0]);a->timer=8;a->phase=1;}
            else if(a->phase==1){if(--a->timer)weapon_animate(a);else a->phase++;}
            else {a->active=a->state=a->phase=0;}
            break;
    }
    if(w->purge){a->state=3;a->phase=2;}
}

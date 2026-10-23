#include "arcade_large.h"
static void add_x(ArcadeLargeActor *a,s16 dx)
{
    u32 x=(((u32)a->x<<8)|a->xf)+(u32)(s32)dx;
    a->x=(u16)(x>>8);a->xf=(u8)x;
}
static void move(ArcadeLargeActor *a)
{
    const ArcadeFoeVector *v=&arcade_large_vectors[a->retreat][a->body.subtype][a->direction];
    u16 y=(u16)((((u16)a->y<<8)|a->yf)+(u16)v->dy);
    add_x(a,v->dx);a->y=(u8)(y>>8);a->yf=(u8)y;
}
static void attributes(ArcadeLargeActor *a)
{
    u8 k,attr=(u8)(((a->facing&2)?0xfa:0xba)+a->body.subtype);
    for(k=0;k<4;k++)a->body.animation.attr[k]=attr;
}
static void walk_pose(ArcadeLargeActor *a)
{
    /* D92A entry must not consume the independent pending-hit byte. */
    u8 force=1;
    arcade_large_damage_pose(&a->body.animation,a->body.hp,a->facing,a->body.subtype,&force);
}
static void edge(ArcadeLargeActor *a,u16 camera)
{if((u8)(a->x-camera)>=232){a->body.state=2;a->body.phase=4;}}
void arcade_large_initialize(ArcadeLargeActor *a,u8 difficulty)
{
    ArcadeLargeBody *b=&a->body;
    u8 index=(u8)(((difficulty&48)>>3)+((difficulty&48)>>4)+b->subtype);
    b->hp=7;b->timer=16;a->direction=a->facing=3;attributes(a);
    b->animation.cursor=arcade_large_entry;b->animation.timer=0;arcade_large_animate(&b->animation);
    a->fire_timer=a->fire_period=arcade_large_fire_periods[index];b->phase++;
}
u8 arcade_large_step(ArcadeLargeActor *a,const ArcadeFoeWorld *w,
                     u8 difficulty,u16 *rng,u8 *sample,
                     const u8 occupied[4],ArcadeLargeShotBirth *birth)
{
    ArcadeLargeBody *b=&a->body;
    u8 k,valid,index,fired=0;
    if(!b->active)return 0;
    switch(b->state)
    {
        case 0:
            switch(b->phase)
            {
                case 0:
                    arcade_large_initialize(a,difficulty);break;
                case 1:
                    if(--b->timer)arcade_large_animate(&b->animation);
                    else {a->retreat=0;a->direction=a->facing=3;attributes(a);walk_pose(a);b->phase++;}
                    break;
                case 2:
                    arcade_large_animate(&b->animation);
                    index=(u8)(((difficulty&56)>>1)+((difficulty&56)>>2)+b->subtype);
                    if((u16)(a->x-w->camera)<=((u16)arcade_large_thresholds[index]<<8|arcade_large_thresholds[index+1]))
                    {
                        index=(u8)(((difficulty&48)>>3)+((difficulty&48)>>4)+b->subtype);
                        a->lifetime=arcade_large_lifetimes[index];b->phase=0;b->state++;
                    }
                    else move(a);
                    break;
            }
            break;
        case 1:
            if(!b->phase)
            {
                k=arcade_large_target(w,&valid);a->target=(u8)(k+1);
                a->direction=arcade_large_steer(a->y,w->players[k].y,b->hp,a->direction,w->clock,rng,sample);
                k=a->x>w->players[k].x?3:1;
                if(k!=a->facing){a->facing=k;walk_pose(a);}
                b->timer=48;b->phase++;
            }
            move(a);
            if(--b->timer)arcade_large_animate(&b->animation);else b->phase=0;
            fired=arcade_large_fire(a->x,a->y,&a->fire_timer,a->fire_period,w->players,occupied,birth);
            if(!(w->clock&63) && !--a->lifetime){b->state=3;b->phase=0;}
            arcade_large_damage_pose(&b->animation,b->hp,a->facing,b->subtype,&a->hit);
            edge(a,w->camera);break;
        case 2:arcade_large_death_step(b);break;
        case 3:
            if(!b->phase){a->retreat=1;a->direction=3;b->phase++;}
            arcade_large_animate(&b->animation);
            k=arcade_large_target(w,&valid);
            if(valid)a->direction=(u8)(a->y+12)<w->players[k].y?4:2;
            move(a);
            arcade_large_damage_pose(&b->animation,b->hp,a->facing,b->subtype,&a->hit);
            fired=arcade_large_fire(a->x,a->y,&a->fire_timer,a->fire_period,w->players,occupied,birth);
            edge(a,w->camera);break;
    }
    /* D55B applies compensation even on the update that clears active. */
    if(w->scroll_stopped)add_x(a,141);
    return fired;
}

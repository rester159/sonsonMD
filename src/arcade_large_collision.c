#include "arcade_large.h"
#include "arcade_difficulty.h"
u8 arcade_large_shot_overlap(u16 ex,u8 ey,u16 sx,u8 sy)
{
    return (u8)(ey-sy+28)<40 && (u16)(ex-sx+28)<=40;
}
void arcade_large_hit(ArcadeLargeActor *a,ArcadePlayerShot *s,u8 owner,
                      u16 combat,ArcadeLargeHit *out)
{
    static const u8 events[4][3]={{0x1b,0x1c,0x1e},{0x1c,0x1d,0x20},{0x1d,0x1e,0x25},{0x1e,0x1f,0x26}};
    static const u16 points[4][3]={{3000,4000,6000},{4000,5000,8000},{5000,6000,9000},{6000,7000,10000}};
    ArcadeLargeBody *b=&a->body;
    s->active=2;s->state=2;s->phase=0;a->hit=1;
    out->combat=combat;out->event=out->points=0;out->sound=7;
    if(! --b->hp)
    {
        b->active=2;b->state=2;b->phase=0;b->owner=owner&1;
        out->event=(u16)(0x400|events[b->variant][b->subtype]|((owner&1)<<7));
        out->points=points[b->variant][b->subtype];out->combat=arcade_difficulty_add(combat,0x80);
    }
}

u8 arcade_large_shot_target(const ArcadeLargeActor actors[3],u16 x,u8 y,u8 active)
{
    u8 k;
    if(active!=1)return 255;
    for(k=0;k<3;k++)if(actors[k].body.active==1 &&
        arcade_large_shot_overlap(actors[k].x,actors[k].y,x,y))return k;
    return 255;
}
u8 arcade_large_contact_target(const ArcadeLargeActor actors[3],u16 x,u8 y,u8 active)
{
    u8 k;
    if(active!=1)return 255;
    for(k=0;k<3;k++)if(actors[k].body.active==1 &&
        (u8)(actors[k].y-y+24)<=32 && (u16)(actors[k].x-x+24)<=32)return k;
    return 255;
}

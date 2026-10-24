#include "arcade_rewards.h"
#include "arcade_difficulty.h"
static u8 collect(ArcadePowerup *actor,u8 variant,u16 x,u8 y,u8 owner,u16 combat,
                        u8 *count,ArcadeSpecialSpawn *spawn,
                        ArcadePowerup *conversion,ArcadeBonusAward *out)
{
    if(actor->kind>10 || (actor->kind==10 && variant>7) || actor->active!=1)return 0;
    if((u8)(y-actor->y+7)>14 || (u16)(x-actor->x+3)>6)return 0;
    actor->active=2;actor->owner=owner;actor->state=1;actor->phase=0;
    if(actor->kind==10)
    {
        out->points=variant==7?20000:(u16)(variant+4)*1000;
        out->event=(u16)(0x400|(variant==7?0x2a:12+variant)|((u16)owner<<7));
        out->combat=combat;
    }
    else
    {
        out->points=(u16)(actor->kind+1)*1000;
        out->event=(u16)(0x400|(9+actor->kind)|((u16)owner<<7));
        out->combat=arcade_difficulty_add(combat,0xc0);
    }
    out->sound=5;
    *count=(u8)(*count+1);
    if(*count>=8)
    {
        *count=0;
        conversion->kind=255;
        spawn->active=1;spawn->phase=0;spawn->placement=0x68;
    }
    return 1;
}
u8 arcade_pickup_collect(ArcadePowerup *actor,u16 x,u8 y,u8 owner,u16 combat,
                        u8 *count,ArcadeSpecialSpawn *spawn,
                        ArcadePowerup *conversion,ArcadeBonusAward *out)
{
    if(actor->kind>=10)return 0;
    return collect(actor,0,x,y,owner,combat,count,spawn,conversion,out);
}
u8 arcade_variant_pickup_collect(ArcadePowerup *actor,u8 variant,u16 x,u8 y,
                        u8 owner,u16 combat,u8 *count,ArcadeSpecialSpawn *spawn,
                        ArcadePowerup *conversion,ArcadeBonusAward *out)
{
    if(actor->kind!=10)return 0;
    return collect(actor,variant,x,y,owner,combat,count,spawn,conversion,out);
}
void arcade_gh_kill(ArcadeGHKill *out,u16 combat,u8 remaining,u8 family,u8 owner)
{
    out->owner=owner;
    out->remaining=(u8)(remaining-1);
    out->phase=0;
    out->points=150;
    out->event=(u16)((family?0x422:0x421)|((u16)owner<<7));
    out->bonus=0;
    combat=arcade_difficulty_add(combat,family?0x19:0x33);
    if(!out->remaining)
    {
        out->phase=4;
        out->points+=(family?1000:2000);
        out->bonus=(u16)((family?0x414:0x424)|((u16)owner<<7));
        combat=arcade_difficulty_add(combat,family?0x40:0x4c);
    }
    out->combat=combat;
}

/* Shared converted-enemy pickup. Original E89C event selectors and BCD awards. */
u8 arcade_gh_bonus_collect(ArcadeFoe *actor,u8 owner,u16 combat,ArcadeBonusAward *out)
{
    static const u16 points[11]={1000,2000,3000,4000,5000,6000,7000,8000,9000,10000,20000};
    u8 kind=actor->kind;
    if(kind>10)return 0;
    actor->body.active=2;
    actor->body.phase++;
    actor->hit_style=owner;
    out->points=points[kind];
    out->event=(u16)(0x400|(kind==10?0x2a:9+kind)|((u16)owner<<7));
    out->sound=5;
    out->combat=arcade_difficulty_add(combat,0xc0);
    return 1;
}

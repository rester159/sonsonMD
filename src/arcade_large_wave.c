#include "arcade_large.h"
u8 arcade_large_wave_step(ArcadeWaveControl *c,ArcadeLargeActor actors[3],
                          u16 camera,u8 difficulty)
{
    u8 born=0;
    if(!c->active)return 0;
    if(!c->state){c->wait=3;c->auxiliary=0;c->state++;return 0;}
    if(c->state==1)
    {
        ArcadeLargeActor *a=&actors[c->auxiliary];
        if(!a->body.active)
        {
            static const u8 ys[4]={64,96,160,176};
            a->body.active=1;a->x=(u16)(camera+232);a->y=ys[c->wait];
            a->body.variant=c->parameter;a->body.subtype=(u8)(3-c->wait);
            a->body.state=a->body.phase=0;arcade_large_initialize(a,difficulty);
            born=(u8)(1<<c->auxiliary);
        }
        c->auxiliary++;
        if(! --c->wait)c->active=c->state=c->phase=0;
    }
    return born;
}

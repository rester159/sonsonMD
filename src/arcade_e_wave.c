#include "arcade_e_wave.h"
u8 arcade_e_wave_step(ArcadeWaveControl *c,u8 live,u8 clock,u8 difficulty,
                     u16 camera,ArcadeEBirth births[2])
{
    u8 k,sequence;
    if(!c->active)return 0;
    if(!c->state)
    {
        if(live)return 0;
        if(!c->phase){c->wait=c->parameter;c->phase++;}
        /* F468 falls through: a phase0 clock consumes the first count now. */
        if(c->phase==1 && !(clock&31) && !--c->wait)
        {c->state++;c->phase=0;}
        return 0;
    }
    if(c->state!=1 || c->phase)return 0;
    sequence=(difficulty&56)>>3;
    for(k=0;k<2;k++)
    {
        births[k].x=(u16)(camera+248);
        births[k].y=arcade_e_placements[sequence][k][0];
        births[k].delay=arcade_e_placements[sequence][k][1];
        births[k].direction=9;
    }
    c->state=c->phase=0;
    return 2;
}

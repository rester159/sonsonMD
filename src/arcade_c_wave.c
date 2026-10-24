#include "arcade_c_wave.h"
u8 arcade_c_wave_step(ArcadeCWave *w,u8 live,u8 clock,u8 difficulty,
                     u16 camera,ArcadeCBirth births[6])
{
    ArcadeWaveControl *c=&w->control;u8 k,sequence;
    if(!c->active)return 0;
    if(!c->state)
    {
        if(live)return 0;
        if(!c->phase)
        {
            c->wait=c->parameter;w->timer=30;c->phase++;
            /* F292 falls directly into F294: first tick ends at29. */
        }
        if(c->phase==1)
        {
            if(--w->timer)return 0;
            w->timer=30;
            if(--c->wait)return 0;
            w->reservation=6;w->kind=0;c->state++;c->phase=0;
        }
        return 0;
    }
    if(c->state!=1 || c->phase)return 0;
    sequence=(u8)(((clock>>5)^difficulty)&7);
    for(k=0;k<6;k++)
    {
        births[k].x=(u16)(camera+248);
        births[k].y=arcade_c_placements[sequence][k][0];
        births[k].delay=arcade_c_placements[sequence][k][1];
        births[k].kind=w->kind;
    }
    c->state=c->phase=0;
    return 6;
}

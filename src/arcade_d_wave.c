#include "arcade_d_wave.h"
u8 arcade_d_wave_step(ArcadeDWave *w,u8 live,u8 clock,u8 difficulty,
                     u16 camera,u16 tracked_x,ArcadeDBirth births[4])
{
    ArcadeWaveControl *c=&w->control;u8 k,sequence,quarter;
    if(!c->active)return 0;
    if(!c->state)
    {
        if(live)return 0;
        if(!c->phase)
        {
            c->wait=c->parameter;w->timer=30;c->phase++;
            /* F381 falls directly into F383: first tick ends at29. */
        }
        if(c->phase==1)
        {
            if(--w->timer)return 0;
            w->timer=30;
            if(--c->wait)return 0;
            w->reservation=4;w->kind=0;c->state++;c->phase=0;
        }
        return 0;
    }
    if(c->state!=1 || c->phase)return 0;
    sequence=(u8)(((clock>>5)^difficulty)&7);
    quarter=(u8)(((u8)(tracked_x-camera)&192)+64);
    if(!quarter)quarter=16;
    quarter=(u8)(quarter+16);
    for(k=0;k<4;k++)
    {
        births[k].x=(u16)(camera+248);
        births[k].y=arcade_d_placements[sequence][k][0];
        births[k].delay=arcade_d_placements[sequence][k][1];
        births[k].kind=w->kind;births[k].direction=9;
        births[k].target_offset=quarter;
    }
    c->state=c->phase=0;
    return 4;
}

#include "arcade_f_wave.h"
u8 arcade_f_wave_step(ArcadeFWave *w,u8 live,u8 clock,u8 difficulty,
    u16 camera,ArcadeFBirth births[4])
{
    ArcadeWaveControl *c=&w->control;u8 k,group,sequence,left;
    if(!c->active)return 0;
    if(!c->state)
    {
        if(live)return 0;
        if(!c->phase){c->wait=c->parameter;c->phase++;}
        if(c->phase==1 && !(clock&31) && !--c->wait)
        {w->reservation=4;c->state++;c->phase=0;}
        return 0;
    }
    if(c->state!=1 || c->phase)return 0;
    group=(difficulty&48)>>4;sequence=(difficulty&14)>>1;left=difficulty&32;
    for(k=0;k<4;k++)
    {
        const u8 *p=arcade_f_placements[group][sequence][k];
        births[k].x=(u16)(camera+(left?-8:248));
        births[k].y=births[k].origin_y=p[0];births[k].delay=p[1];
        births[k].direction=left?1:5;
    }
    c->state=c->phase=0;
    return 4;
}

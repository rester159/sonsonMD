#include "arcade_i.h"
/* F97A..FA06. Called after actor updates, following G/H controllers. */
u8 arcade_i_wave_step(ArcadeIWave *w,u8 live,u8 clock,u16 camera,
                      u8 *reservation,ArcadeBirth *birth)
{
    ArcadeWaveControl *c=&w->control;
    if(!c->active)return 0;
    if(!c->state)
    {
        if(live)return 0;
        switch(c->phase)
        {
            case 0:c->wait=c->parameter&127;c->phase++;break;
            case 1:
                if(!((u8)(clock+3)&31) && !--c->wait)c->phase++;
                break;
            case 2:
                w->cursor=0;w->remaining=*reservation=4;
                c->state++;c->phase=0;break;
        }
    }
    else if(c->state==1)
    {
        if(!c->phase)
        {
            if(!w->remaining){c->state=c->phase=0;return 0;}
            birth->x=(u16)(camera+248);birth->y=248;birth->facing=3;birth->delay=0;
            w->cursor++;w->remaining--;c->wait=12;c->phase++;
            return 1;
        }
        else if(c->phase==1 && !--c->wait)c->phase=0;
    }
    return 0;
}

#include "arcade_results.h"
void arcade_results_step(ArcadeResults *s,const ArcadeResultsInput *in,
                         ArcadeResultsEvent *e)
{
    e->flags=0;e->player_mask=e->marker=e->number=0;
    e->points[0]=e->points[1]=0;
    if(s->stage==0)
    {
        if(s->phase==2){s->stage=1;s->phase=0;return;}
        /* 9815 falls through into981B on the initialization update. */
        s->phase=1;e->flags=ARCADE_RESULTS_PLAY;
        if(!(in->clock&63))s->phase=2;
        return;
    }
    if(s->stage==1)
    {
        if(in->screen_busy)return;
        if(!s->phase){e->flags=ARCADE_RESULTS_CLEAR;s->phase=1;}
        else{s->stage=2;s->phase=0;}
        return;
    }
    if(s->stage!=2)return;
    switch(s->phase)
    {
        case 0:
            e->flags=ARCADE_RESULTS_BOARD;
            e->number=(u8)(s->course+1);
            if(e->number>9)e->number=0x10;
            ++s->phase;break;
        case 1:case 2:
        {
            u8 p2=s->phase==2;
            if(p2?((in->two_players&1) && !in->p2_out):!in->p1_out)
            {
                e->flags=ARCADE_RESULTS_PLAYER;e->player_mask=p2?2:1;
                if(in->marker==e->player_mask)e->marker=in->marker;
            }
            ++s->phase;break;
        }
        case 3:
            e->flags=ARCADE_RESULTS_DECOR;s->timer=5;++s->phase;break;
        case 4:
            e->flags=ARCADE_RESULTS_ANIMATE;
            if(!(in->clock&63) && !--s->timer)++s->phase;
            break;
        case 5:
        {
            u8 number=(u8)(s->course+1);
            u32 points=(u32)(number>9?10:number)*10000;
            e->flags=ARCADE_RESULTS_AWARD;
            e->player_mask=(!in->p1_out?1:0)|
                (((in->two_players&1) && !in->p2_out)?2:0);
            /* 99E9 selects marker1 separately; every other nonzero value
             * selects the second marker award. Do not clamp this selector. */
            e->marker=in->marker?(in->marker==1?1:2):0;
            if(e->player_mask&1)e->points[0]=points;
            if(e->player_mask&2)e->points[1]=points;
            if(e->marker)e->points[e->marker-1]+=30000;
            ++s->phase;break;
        }
        case 6:
            ++s->course;e->flags=ARCADE_RESULTS_RESTART;
            s->stage=3;s->phase=0;break;
    }
}

#include "arcade_main_wave.h"
static void birth(ArcadeMainBirth *b,u8 slot,u8 sequence,u8 cursor,u16 camera,ArcadeRng *rng)
{
    b->slot=slot;b->x=(u16)(camera+248);
    b->y=arcade_main_placements[sequence][cursor][0];
    b->delay=arcade_rng_next(rng)&7;
    b->variant=arcade_main_placements[sequence][cursor][1];
}
u8 arcade_main_wave_step(ArcadeMainWave *w,u8 live,u8 ready,u8 vsync,
    u8 clock,u8 difficulty,u16 camera,const u8 occupied[16],ArcadeRng *rng,
    ArcadeMainBirth births[12])
{
    ArcadeWaveControl *c=&w->control;u8 k,sequence;
    if(!c->active)return 0;
    if(!c->state)
    {
        if(live){c->state=1;c->phase=1;}
        else c->state++; /* phase is retained by F004. */
        return 0;
    }
    if(c->state!=1)return 0;
    switch(c->phase)
    {
        case 0:
            if(ready<12)return 0;
            sequence=(u8)((difficulty&48)>>2)+((vsync&6)>>1);
            for(k=0;k<12;k++)birth(&births[k],k,sequence,k,camera,rng);
            c->phase++;return 12;
        case 1:
            w->sequence=(u8)((difficulty&48)>>2)+(((vsync^clock)&6)>>1);
            w->cursor=0;w->remaining=12;c->phase++;return 0;
        case 2:
            if(!ready || live==16)return 0;
            for(k=0;k<16;k++)if(!occupied[k])
            {
                birth(births,k,w->sequence,w->cursor,camera,rng);
                w->cursor++;
                if(!--w->remaining)c->phase=1;
                return 1;
            }
            return 0;
    }
    return 0;
}

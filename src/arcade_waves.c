#include "arcade_waves.h"
#include "ss_wave_sched.h"
extern const u8 arcade_formations[2][4][48];

/* $95F4: two ASLs select complemented XOR bit 6, then rotate the 16-bit seed. */
u8 arcade_rng_next(ArcadeRng *rng)
{
    u8 feedback=(u8)~((u8)rng->seed ^ (u8)(rng->seed>>8));
    rng->seed=(u16)((rng->seed<<1)|((feedback>>6)&1));
    rng->last=(u8)rng->seed;
    return rng->last;
}
void arcade_waves_reset(ArcadeWaves *w,u8 cached_coarse)
{
    u8 i;
    w->mask=w->placement=0;
    w->cached_coarse=cached_coarse;
    w->state=w->cursor=w->remaining=w->delay=0;
    for(i=0;i<ARCADE_WAVE_CONTROLLERS;i++)
    {
        ArcadeWaveControl *c=&w->controls[i];
        c->auxiliary=0;
        c->active=c->state=c->phase=c->wait=c->parameter=0;
    }
}
/* 9048..9065 and96C0..9775: course setup is not a producer reset.
 * 0628 cached coarse clears; 062A producer state and its cursor/mask survive.
 * 0638..0643 clears the first controller except its parameter at0646.
 * Other listed controllers clear only active; 06C8 and06E8 are untouched. */
void arcade_waves_course_begin(ArcadeWaves *w)
{
    u8 i;ArcadeWaveControl *c=&w->controls[0];
    w->cached_coarse=0;
    c->active=c->state=c->phase=c->wait=0;c->auxiliary=0;
    for(i=1;i<=8;i++)w->controls[i].active=0;
    w->controls[10].active=0;
}
s16 arcade_waves_step(ArcadeWaves *w,u8 coarse,u8 clock_phase)
{
    ArcadeWaveControl *c;
    u8 index;
    if(w->delay)
    {
        if(!(clock_phase&63))w->delay--;
        return -1;
    }
    if(!w->state)
    {
        u8 wave=(u8)(coarse&127);
        if(coarse==w->cached_coarse)return -1;
        /* Course progression must select a valid source descriptor. */
        if(wave>=SS_WAVE_COUNT)return -1;
        w->cached_coarse=coarse;
        w->mask=ss_wave_mask[wave];
        w->placement=ss_wave_place_off[wave];
        w->cursor=0;w->remaining=12;w->state=1;
        return -1;
    }
    index=w->cursor;c=&w->controls[index];
    if(w->mask&1)
    {
        c->parameter=ss_wave_place[w->placement++];
        if(!c->active){c->active=1;c->state=c->phase=0;}
    }
    else {c->active=c->state=c->phase=0;}
    w->mask>>=1;w->cursor++;
    if(!--w->remaining)w->state=0;
    return index;
}
u8 arcade_formation_step(ArcadeWaveControl *c,u8 family,u8 live_count,
                         u8 clock_phase,u8 stage,u16 camera,u16 tracked_x,
                         ArcadeRng *rng,ArcadeBirth births[6],u8 *reservation)
{
    const u8 *data;
    u8 i,variant;
    u16 x;
    if(!c->active)return 0;
    if(!c->state)
    {
        if(live_count)return 0;
        if(!c->phase)
        {
            c->wait=(u8)(c->parameter&127);c->phase=1;
            if(family)return 0; /* H returns; G falls through on the same tick. */
        }
        if(c->phase==1)
        {
            if(!((u8)(clock_phase+1+family)&31))
            {c->wait--;if(!c->wait)c->phase=2;}
            return 0;
        }
        *reservation=6;c->state=1;c->phase=0;
        return 0;
    }
    if(family)
    {
        u8 quarter=(u8)(((u8)(tracked_x-camera)&192)+64);
        if(!quarter)quarter=16;
        c->auxiliary=(u8)(quarter+16);
    }
    variant=(u8)(arcade_rng_next(rng)&3);
    /* G intentionally selects overlapping windows; H selects separate groups. */
    data=&arcade_formations[family][(stage&48)>>4][variant*(family?12:2)];
    x=(u16)(camera+((c->parameter&128)?(u16)-8:248));
    for(i=0;i<6;i++)
    {
        births[i].x=x;births[i].y=*data++;births[i].delay=*data++;
        births[i].facing=(c->parameter&128)?1:3;
    }
    c->state=c->phase=0;
    return 6;
}

/* $C6DA/$C6F4/$C711 and $CF39/$CF58/$CF75. Animation is owned separately. */
void arcade_formation_enter(ArcadeFormationActor *a,u8 family,u16 camera,u8 scroll_stopped)
{
    if(!a->state)
    {
        a->state=1;a->phase=0;
        if(family)
        {a->x=(u16)(a->x+(a->facing==1?1:(u16)-1));return;}
    }
    if(a->state!=1)return;
    if(!a->phase)
    {
        if(!a->delay){a->active=1;a->phase=1;}
        else
        {
            a->delay--;
            if(!scroll_stopped)
            {
                u16 fraction=(u16)(a->xf+141);
                a->xf=(u8)fraction;a->x=(u16)(a->x+(fraction>>8));
            }
        }
        return;
    }
    a->x=(u16)(a->x+(a->facing==1?1:(u16)-1));
    if((u16)(a->x-camera-8)<224){a->state=2;a->phase=0;}
}

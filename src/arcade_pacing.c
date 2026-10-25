#include "arcade_pacing.h"
void arcade_pacing_init(ArcadePacing *p,u8 pal,u16 refresh)
{
    /* 55.4=277/5; H32/H40 both3420 master clocks/line. Genesis Plus GX
     * core/system.h clocks; non-interlaced262/313 lines. Region compatibility
     * and actual console oscillator variation need separate validation. */
    p->numerator=277UL*3420*(pal?313:262);
    p->denominator=5UL*(pal?53203424UL:53693175UL);
    p->phase=p->pending=0;p->last_refresh=refresh;
}
void arcade_pacing_observe(ArcadePacing *p,u16 refresh,u8 paused)
{
    u16 elapsed=(u16)(refresh-p->last_refresh);
    p->last_refresh=refresh;
    if(paused)return;
    while(elapsed--)
    {
        /* Largest sum is below2^30, including PAL's occasional second tick. */
        p->phase+=p->numerator;
        while(p->phase>=p->denominator)
        {p->phase-=p->denominator;++p->pending;}
    }
}
u8 arcade_pacing_take(ArcadePacing *p)
{
    if(!p->pending)return 0;
    --p->pending;return 1;
}

static ArcadePacing session_pacing;
static u8 session_ready;
ArcadePacing *arcade_pacing_enter(u8 pal,u16 refresh)
{
    if(!session_ready)
    {
        arcade_pacing_init(&session_pacing,pal,refresh);
        session_ready=1;
    }
    else arcade_pacing_observe(&session_pacing,refresh,1);
    return &session_pacing;
}

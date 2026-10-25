#include "arcade_large.h"
u8 arcade_large_target(const ArcadeFoeWorld *w,u8 *eligible)
{
    u8 a=(u8)(~w->players[0].kind&w->players[0].active&1);
    u8 b=(u8)(~w->players[1].kind&w->players[1].active&1);
    u8 k=(u8)(w->tracking && b && (!a || w->players[1].x>w->players[0].x));
    *eligible=k?b:a;
    return k;
}
u8 arcade_large_steer(u8 y,u8 target_y,u8 hp,u8 previous,u8 clock,
                       u16 *rng,u8 *sample)
{
    u8 direction=(u8)(y+12)<target_y?4:2;
    if(hp==1 && direction!=previous && !(clock&32))
    {
        /* 95F4: two shifts of ~(high^low), then rotate low and high.
         * The second shift supplies complement XOR bit6 as the new bit0. */
        u16 seed=*rng;
        u8 carry=(u8)(((u8)~((seed>>8)^seed)>>6)&1);
        u8 candidate;
        *rng=(u16)((seed<<1)|carry);
        *sample=(u8)*rng;
        candidate=(u8)(((*sample&1)<<1)|1);
        if(candidate!=previous)direction=candidate;
    }
    return direction;
}

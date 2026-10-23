#ifndef ARCADE_TRACKING_H
#define ARCADE_TRACKING_H
#include "arcade_types.h"
/* AD53/AD71. Flags pack source active in the high byte and kind in the low.
 * When neither qualifies, Y still points to P1 (carry is ignored by spawners).
 * Source compares unsigned world X and resolves ties in favor of P1. */
static inline u8 arcade_tracking_player(u8 tracking,u16 x0,u16 flags0,u16 x1,u16 flags1)
{
    u8 eligible0=(u8)((flags0>>8)&~flags0&1);
    u8 eligible1=(u8)((flags1>>8)&~flags1&1);
    return (u8)(tracking && eligible1 && (!eligible0 || x1>x0));
}
#endif

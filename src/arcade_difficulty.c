#include "arcade_difficulty.h"
u16 g_arcadeCombatDifficulty;
/* $E407: 16-bit addition first, then reject a resulting high byte >= $3F. */
u16 arcade_difficulty_add(u16 current, u16 amount)
{
    u16 next=(u16)(current+amount);
    return next<0x3f00 ? next : current;
}
/* $A73F: preserve the fractional byte, and don't reset combat at the base cap. */
u16 arcade_difficulty_seam(u8 *base, u16 combat)
{
    if(*base<0x1c)
    {
        ++*base;
        combat=(u16)(((u16)*base<<8)|(combat&255));
    }
    return combat;
}

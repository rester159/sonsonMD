#ifndef ARCADE_SPRITE_BUDGET_H
#define ARCADE_SPRITE_BUDGET_H
#include "arcade_types.h"
/* FED8 order: special[3], shots[4], players[2], ordinary[55], large[3], K[4].
 * Logical source cells, independent of Genesis wrap copies or viewport culling. */
#define ARCADE_SPRITE_ACTORS 71
u8 arcade_sprite_budget(const u8 active[ARCADE_SPRITE_ACTORS],u8 suppress_p2,
                         u8 admitted[9]);
/* Count-only equivalent for gameplay readiness: single-cell actors before
 * large, then shared K. Caller counts active nonnegative singles without culling. */
u8 arcade_sprite_remaining(u8 singles,const u8 large[3],const u8 children[4]);
#endif

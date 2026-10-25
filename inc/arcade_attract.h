#ifndef ARCADE_ATTRACT_H
#define ARCADE_ATTRACT_H
#include "arcade_types.h"

/* Byte offsets into the shared original data span, not separate looping tracks. */
typedef struct {
    u16 cursor[2];
    u8 input[2], previous[2], delay[2];
} ArcadeAttract;

/* 9492 demo setup retains previous inputs; the first step overwrites them. */
void arcade_attract_init(ArcadeAttract *state);
/* 93BA prelude setup: retain input/previous bytes, replace pointers/delays. */
void arcade_attract_prelude_init(ArcadeAttract *state);
/* Returns zero without mutation if a read would leave the recovered data span.
 * Caller owns source clock phase and demo lifetime; this is not a loop marker. */
u8 arcade_attract_step(ArcadeAttract *state, u8 clock_low);
#endif

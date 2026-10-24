#ifndef ARCADE_TITLE_FLOW_H
#define ARCADE_TITLE_FLOW_H
#include "arcade_types.h"
typedef struct {
    u16 timer, demo_timer, shown_credits;
    u8 state, phase;
} ArcadeTitleFlow;
typedef struct { u16 credits; u8 clock, busy, free_play; } ArcadeTitleInput;
enum {
    TITLE_SEED_RNG=1, TITLE_PRELUDE_WORLD=2, TITLE_PRELUDE_PLAYERS=4,
    TITLE_PRELUDE_DECOR=8, TITLE_PRELUDE_TICK=16, TITLE_DEMO_INIT=32,
    TITLE_DEMO_TICK=64, TITLE_CLEAR_SPRITES=128,
    TITLE_CLEAR_E6=256, TITLE_CLEAR_8B=512, TITLE_SET_BUSY=1024,
    TITLE_RESET_SUBSTATES=2048, TITLE_RESET_0B=4096
};
typedef struct { u16 flags, commands[8]; u8 count; } ArcadeTitleEvent;
/* One non-frozen SOURCE update, not one display refresh. Zero initialize at
 * source state entry, preserving shown_credits across calls. Call purchase
 * after this function, matching 9296; after a purchase synchronize shown_credits.
 * Caller implements world/player/decor setup and queue completion. DEMO_TICK
 * executes before CLEAR on timeout; queued commands retain source order.
 * title.c owns prelude/boards; its demo callback runs after game_update. */
void arcade_title_step(ArcadeTitleFlow *, const ArcadeTitleInput *, ArcadeTitleEvent *);
#endif

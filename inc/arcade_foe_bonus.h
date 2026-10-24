#ifndef ARCADE_FOE_BONUS_H
#define ARCADE_FOE_BONUS_H
#include "arcade_foes.h"
#include "arcade_bonus_art.h"
#define ARCADE_BONUS_POSE 0x8000
#define ARCADE_BONUS_OWNER2 0x4000
/* Source shared FF state, including family offscreen and purge wrappers. */
void arcade_foe_bonus_step(ArcadeFoe *actor,u16 camera,u8 purge,u8 *next,u8 limit);
#endif

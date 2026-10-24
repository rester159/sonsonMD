#ifndef ARCADE_DIFFICULTY_H
#define ARCADE_DIFFICULTY_H
#include "arcade_types.h"
/* Source $5A is the scenery base; $96:$97 is a separate combat accumulator. */
extern u16 g_arcadeCombatDifficulty;
u16 arcade_difficulty_add(u16 current, u16 amount);
u16 arcade_difficulty_seam(u8 *base, u16 combat);
#endif

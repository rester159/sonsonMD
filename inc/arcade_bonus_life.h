#ifndef ARCADE_BONUS_LIFE_H
#define ARCADE_BONUS_LIFE_H
#include "arcade_types.h"
/* FC2C: score threshold is three packed decimal bytes (hundreds of points).
 * phase is the source byte at 005D/0061, not an award count. */
typedef struct {u32 threshold;u8 phase;} ArcadeBonusLife;
void arcade_bonus_life_begin(ArcadeBonusLife *state,u8 dip);
u8 arcade_bonus_life_step(ArcadeBonusLife *state,u8 dip,u8 clock,u8 owner,
                         u8 live,u8 out,u32 score);
#endif

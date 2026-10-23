#ifndef ARCADE_PRELUDE_HUD_H
#define ARCADE_PRELUDE_HUD_H
#include "arcade_types.h"
/* Absolute tile words, PAL0; score8cells, credit/prompt full32cell rows.
 * Explicit press_start is lifecycle-selected, not inferred from balance. */
void arcade_prelude_hud(u32 high,u16 credits,u8 free_play,u8 press_start,
                       u16 score[8],u16 credit[32],u16 prompt[32]);
#endif

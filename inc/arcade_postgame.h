#ifndef ARCADE_POSTGAME_H
#define ARCADE_POSTGAME_H
#include "arcade_types.h"
/* Hand off held Start to the existing controller-specific title purchase owner. */
extern u8 arcade_postgame_start;
void arcade_title_ranking_draw(u8 total);
void arcade_postgame_run(u32 p1_score,u32 p2_score,u8 two_players);
#endif

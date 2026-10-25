#ifndef ARCADE_TITLE_MAP_H
#define ARCADE_TITLE_MAP_H
#include "arcade_types.h"
typedef struct {u8 score[4],name[8];} ArcadeTitleRankEntry;
/* Original 32x32 cells and relative art indices, before viewport adaptation.
 * Entry score is four source packed-BCD bytes, names are raw source tile codes.
 * Individual ranking uses name[0..3]; total uses all8 with the original gap.
 * Caller supplies exactly five entries and owns clearing/VDP publication. */
void arcade_title_map_label(u16 *map,u8 command);
void arcade_title_map_ranking(u16 *map,u8 total,const ArcadeTitleRankEntry entries[5]);
#endif

#ifndef ARCADE_RESULTS_ART_H
#define ARCADE_RESULTS_ART_H
#include "arcade_types.h"
/* Own scene: tile indices are relative to the caller-selected load base.
 * Group0 title,1 P1 bonus,2 P1 marker,3 P2 bonus,4 P2 marker. */
typedef struct {u16 cell,tile;} ArcadeResultsCell;
typedef struct {u16 first,count;} ArcadeResultsGroup;
#define ARCADE_RESULTS_ART_TILES 25
#define ARCADE_RESULTS_ART_CELLS 54
extern const u32 arcade_results_pixels[ARCADE_RESULTS_ART_TILES*8];
extern const ArcadeResultsCell arcade_results_cells[ARCADE_RESULTS_ART_CELLS];
extern const ArcadeResultsGroup arcade_results_groups[5];
/* Digits0..9, leading blank10, scene-clear blank11. */
extern const u16 arcade_results_digits[12];
#endif

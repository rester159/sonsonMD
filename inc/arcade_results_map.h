#ifndef ARCADE_RESULTS_MAP_H
#define ARCADE_RESULTS_MAP_H
#include "arcade_results.h"
/* Native32x32 tile map, relative to results art load base. Existing HUD rows
 * are preserved. Caller owns publication and buffer lifetime through DMA. */
void arcade_results_map_clear(u16 *map);
void arcade_results_map_event(u16 *map,u8 number,const ArcadeResultsEvent *event);
#endif

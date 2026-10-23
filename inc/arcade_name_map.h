#ifndef ARCADE_NAME_MAP_H
#define ARCADE_NAME_MAP_H
#include "arcade_name_entry.h"
#include "arcade_cell_sat.h"
void arcade_name_map_begin(u16 *map,u8 players);
void arcade_name_map_update(u16 *map,u8 player,const ArcadeNameEntry *,u8 paint);
/* Caller terminates the full SAT. Returns emitted count or65535 on overflow. */
u16 arcade_name_cursor_sat(const ArcadeNameEntry *,u8 player,u16 tile_base,
                           const ArcadeSpriteView *,u16 *sat,u16 first,u16 limit);
#endif

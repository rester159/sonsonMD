#ifndef ARCADE_NAME_ENTRY_H
#define ARCADE_NAME_ENTRY_H
#include "arcade_types.h"
/* Original0380/038C records, with retained bytes matching9F37. */
typedef struct {u8 raw[12],name[4];} ArcadeNameEntry;
typedef struct {u16 deadline;u8 phase;} ArcadeNameIdle;
void arcade_name_init(ArcadeNameEntry *);
/* Source44/46 current/previous input bits. Returns the name-cell redraw mask.
 * Includes9FF9's cursor-hole correction after9F75, but not sprite publication. */
u8 arcade_name_step(ArcadeNameEntry *,u8 current,u8 previous);
/* 9E37: directional activity only; return1 one call AFTER deadline equality. */
u8 arcade_name_idle_step(ArcadeNameIdle *,u16 clock,u8 p1,u8 p2,u8 two_players);
#endif

#ifndef ARCADE_PRELUDE_TERRAIN_H
#define ARCADE_PRELUDE_TERRAIN_H
#include "arcade_player.h"
extern const u8 arcade_prelude_terrain[128];
/* Camera is zero in93BA. Source ring8A is owned by the world lifecycle;
 * 907D copies collision bytes but does not reset that ring. Do not silently
 * substitute ring0 on a later prelude. */
typedef struct {u8 ring;} ArcadePreludeTerrain;
s16 arcade_prelude_floor(u16 x,u8 y,void *context);
#endif

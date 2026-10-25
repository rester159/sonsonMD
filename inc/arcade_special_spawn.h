#ifndef ARCADE_SPECIAL_SPAWN_H
#define ARCADE_SPECIAL_SPAWN_H
#include "arcade_types.h"
typedef struct { u8 active,phase,placement,row; } ArcadeSpecialSpawn;
/* A native spawn/background event, not a source-board command buffer. */
typedef struct { u8 write,row,codes[2],attributes[2],spawn; } ArcadeSpecialSpawnEvent;
void arcade_special_spawn_step(ArcadeSpecialSpawn *s,u8 blocked,u8 ready,u8 advance,
                               ArcadeSpecialSpawnEvent *event);
#endif

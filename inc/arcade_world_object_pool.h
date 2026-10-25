#ifndef ARCADE_WORLD_OBJECT_POOL_H
#define ARCADE_WORLD_OBJECT_POOL_H
#include "arcade_world_objects.h"
typedef struct {
    ArcadeWorldObject objects[20];
    u8 block,quarter;
} ArcadeWorldObjectPool;
/* New game at segment0: 8FC5 clears active flags, 54B0 preloads16 records.
 * Inactive tail records retain other fields, as on the source. */
void arcade_world_object_begin(ArcadeWorldObjectPool *pool);
/* 54E7: replace four records, advance the source block and wrap the five
 * destination quarters. No free-slot search; end of source leaves all intact. */
u8 arcade_world_object_load(ArcadeWorldObjectPool *pool);
#endif

#ifndef ARCADE_BARRIER_ART_H
#define ARCADE_BARRIER_ART_H
#include "arcade_types.h"
#define ARCADE_BARRIER_ART_TILES 220
extern const u32 arcade_barrier_pixels[ARCADE_BARRIER_ART_TILES*8];
extern const u16 arcade_barrier_tiles[36][5][4];
/* Immutable resident tile or65535: needs dynamic allocation before publication. */
extern const u16 arcade_barrier_resident[ARCADE_BARRIER_ART_TILES];
#endif

#ifndef ARCADE_FINISH_ART_H
#define ARCADE_FINISH_ART_H
#include "arcade_types.h"
/* F700..F7FF, after SAT and before F800 barrier arena. */
#define ARCADE_FINISH_TILE_BASE 1976
extern const u32 arcade_finish_pixels[8*8];
extern const u16 arcade_finish_tiles[2][4];
void arcade_finish_background_reset(void);
void arcade_finish_background_write(u16 camera,u8 phase,u8 pattern);
void arcade_finish_background_render(void);
#endif

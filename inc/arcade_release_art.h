#ifndef ARCADE_RELEASE_ART_H
#define ARCADE_RELEASE_ART_H
#include "arcade_types.h"
#define ARCADE_RELEASE_TILE_BASE 2032
#define ARCADE_RELEASE_TILE_COUNT 14
extern const u32 arcade_release_pixels[ARCADE_RELEASE_TILE_COUNT*8];
extern const u16 arcade_release_tiles[8][4];
void arcade_release_background_reset(void);
void arcade_release_background_begin(void);
void arcade_release_background_step(u16 camera,u8 clock,u8 plane_phase);
void arcade_release_background_render(void);
#endif

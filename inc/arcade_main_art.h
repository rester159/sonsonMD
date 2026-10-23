#ifndef ARCADE_MAIN_ART_H
#define ARCADE_MAIN_ART_H
#include "arcade_types.h"
/* Overwrites retired scene patterns only; borrowed food/barrier tiles protected. */
#define ARCADE_MAIN_TILE_BASE 61
#define ARCADE_MAIN_TILE_COUNT 72
extern const u32 arcade_main_pixels[ARCADE_MAIN_TILE_COUNT*8];
u16 arcade_main_tile(u8 code,u8 attr);
#endif

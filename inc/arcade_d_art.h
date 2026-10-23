#ifndef ARCADE_D_ART_H
#define ARCADE_D_ART_H
#include "arcade_types.h"
#define ARCADE_D_ART_CELLS 6
#define ARCADE_D_TILE_BASE 1921
#define ARCADE_D_TILE_COUNT 24
/* F020..F31F uses unused plane-scroll table space. Requires HSCROLL_PLANE,
 * HScroll baseF000 and SAT baseF400. Explicit cold-load ownership only. */
extern const u16 arcade_d_art_slots[ARCADE_D_ART_CELLS];
extern const u32 arcade_d_pixels[ARCADE_D_ART_CELLS][32];
u16 arcade_d_tile(u8 code,u8 attr);
#endif

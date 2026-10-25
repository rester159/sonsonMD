#ifndef ARCADE_C_ART_H
#define ARCADE_C_ART_H
#include "arcade_types.h"
#define ARCADE_C_ART_CELLS 9
/* Four tiles per cell. Load only after the captured scene; cold-load only. */
extern const u16 arcade_c_art_slots[ARCADE_C_ART_CELLS];
extern const u32 arcade_c_pixels[ARCADE_C_ART_CELLS][32];
u16 arcade_c_tile(u8 code,u8 attr);
#endif

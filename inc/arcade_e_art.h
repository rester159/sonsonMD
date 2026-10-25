#ifndef ARCADE_E_ART_H
#define ARCADE_E_ART_H
#include "arcade_types.h"
#define ARCADE_E_ART_CELLS 12
#define ARCADE_E_DYNAMIC 0x8000
/* Resident tile, ARCADE_E_DYNAMIC|cell, or65535 for unknown. Flip bits ignored. */
u16 arcade_e_art(u8 code,u8 attr);
extern const u32 arcade_e_pixels[ARCADE_E_ART_CELLS][32];
#endif

#ifndef ARCADE_F_MOTION_H
#define ARCADE_F_MOTION_H
#include "arcade_types.h"
typedef struct {s32 dx;s16 dy;} ArcadeFVector;
extern const ArcadeFVector arcade_f_entry_vectors[9];
extern const s32 arcade_f_cruise_vectors[9];
extern const u8 arcade_f_sine[128];
/* DFF0..E02F: advance phase, toggle half-cycle, and replace integer Y.
 * Uses the original table bytes, including its asymmetries. */
u8 arcade_f_wave_y(u16 *phase,u8 *half,u8 origin,u8 amplitude);
#endif

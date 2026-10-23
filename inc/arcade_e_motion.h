#ifndef ARCADE_E_MOTION_H
#define ARCADE_E_MOTION_H
#include "arcade_types.h"
/* C44D: turn by one direction step using the source direction-specific axis. */
u8 arcade_e_turn(u8 direction,u16 x,u8 y,u16 target_x,u8 target_y);
typedef struct {s32 dx;s16 dy;} ArcadeEVector;
extern const ArcadeEVector arcade_e_vectors[2][17];
#endif

#ifndef ARCADE_D_MOTION_H
#define ARCADE_D_MOTION_H
#include "arcade_types.h"
/* C0AD/C0DB: one of16 direction indices, using source unsigned comparisons,
 * low-byte absolute X distance, byte Y distance and C29C division semantics. */
u8 arcade_d_aim(u16 x,u8 y,u16 target_x,u8 target_y);
typedef struct {s16 dx,dy;} ArcadeDVector;
extern const ArcadeDVector arcade_d_vectors[2][17];
#endif

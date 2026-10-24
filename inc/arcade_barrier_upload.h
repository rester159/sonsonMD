#ifndef ARCADE_BARRIER_UPLOAD_H
#define ARCADE_BARRIER_UPLOAD_H
#include "arcade_types.h"
/* Dedicated [F800,FE00), above the64x32 layout's SAT. Twelve4-tile slots. */
#define ARCADE_BARRIER_VRAM_BASE 1984
#define ARCADE_BARRIER_UPLOAD_SLOTS 12
extern const u8 arcade_barrier_slot[20];
extern const u32 arcade_barrier_pose_pixels[180][32];
void arcade_barrier_upload_reset(void);
u8 arcade_barrier_upload(u8 slot,u8 pose,u16 reserve_bytes,u16 reserve_ops);
#endif

#ifndef ARCADE_POWERUP_UPLOAD_H
#define ARCADE_POWERUP_UPLOAD_H
#include "arcade_powerup_art.h"
/* Three source special-object slots, one 16x16 cell each. [268,280). */
#define ARCADE_POWERUP_VRAM_BASE 268
#define ARCADE_POWERUP_UPLOAD_CELLS 3
void arcade_powerup_upload_reset(void);
/* Call after the previous frame's queue flush. 65535 means unused. Reserve
 * subsequent map/SAT transfer bytes and descriptors. Publish new SAT poses
 * only on success. Immutable ROM sources survive the deferred transfer. */
u8 arcade_powerup_upload(const u16 cells[3],u16 reserve_bytes,u16 reserve_ops);
#endif

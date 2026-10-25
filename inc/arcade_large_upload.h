#ifndef ARCADE_LARGE_UPLOAD_H
#define ARCADE_LARGE_UPLOAD_H
#include "arcade_large_art.h"
/* Three actors (four cells each), then the four shared projectile cells.
 * Dedicated range [1308,1372); no sprite-engine allocator may own it. */
#define ARCADE_LARGE_VRAM_BASE 1308
#define ARCADE_LARGE_UPLOAD_CELLS 16
void arcade_large_upload_reset(void);
/* Call once per presentation, after the previous queue has been flushed.
 * 65535 means unused. All changed cells are admitted together. Caller reserves
 * remaining SAT/map bytes/descriptors and only publishes this pose on success.
 * ROM source pointers remain immutable until the normal VBlank flush. */
u8 arcade_large_upload(const u16 frames[16],u16 reserve_bytes,u16 reserve_ops);
#endif

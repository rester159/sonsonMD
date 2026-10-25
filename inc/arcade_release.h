#ifndef ARCADE_RELEASE_H
#define ARCADE_RELEASE_H
#include "arcade_types.h"
/* A893 / 0F48. Pose events preserve the last write after the actor retires. */
typedef struct {u8 active,phase,remaining,frame;} ArcadeRelease;
extern const u8 arcade_release_patterns[8][8];
extern const u16 arcade_release_destinations[6];
void arcade_release_begin(ArcadeRelease *s);
/* Returns 0..7 for a new background pose, or 255 for no write. */
u8 arcade_release_step(ArcadeRelease *s,u8 clock);
#endif

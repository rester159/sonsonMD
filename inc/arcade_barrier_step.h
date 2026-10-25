#ifndef ARCADE_BARRIER_STEP_H
#define ARCADE_BARRIER_STEP_H
#include "arcade_food_step.h"
extern const u8 arcade_barrier_frames[4][9][4];
extern const u8 arcade_barrier_patterns[36][8];
/* Cursor is a native index: ten slots per variant, ninth is a loop marker.
 * draw.write=4 means only the four palette attributes change to flash. */
void arcade_barrier_step(ArcadeWorldObject *o,u8 *cursor,u16 camera,u8 advance,
                        u8 clock,ArcadeFoodDraw *draw,u8 *flash);
u8 arcade_barrier_next_phase(u8 cursor);
#endif

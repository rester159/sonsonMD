#ifndef ARCADE_FINISH_H
#define ARCADE_FINISH_H
#include "arcade_types.h"
/* A7F0 state2: timer at009F, flashing map write A807, then result request.
 * The caller retains stopped scrolling and008B throughout this update. */
typedef struct {u8 timer;} ArcadeFinish;
typedef struct {u8 write,pattern,results;} ArcadeFinishEvent;
extern const u8 arcade_finish_patterns[2][8];
void arcade_finish_step(ArcadeFinish *s,u8 clock,ArcadeFinishEvent *event);
#endif

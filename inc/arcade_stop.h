#ifndef ARCADE_STOP_H
#define ARCADE_STOP_H
#include "arcade_world_object_pool.h"
typedef struct {u8 timer,grade;} ArcadeStop;
typedef struct {u8 release,bonus,variant;} ArcadeStopEvent;
/* A787, stopped state1. Caller applies release to scroll and0F48, and bonus
 * to descriptor0550 / ordinary pickup0708. Does not implement state2 ending. */
void arcade_stop_step(ArcadeStop *s,const ArcadeWorldObjectPool *pool,
                      u16 camera,u8 clock,ArcadeStopEvent *event);
#endif

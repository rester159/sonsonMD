#ifndef ARCADE_SHOTS_H
#define ARCADE_SHOTS_H
#include "arcade_types.h"
typedef struct {
    u16 x,origin;
    u8 xf,y,yf,active,state,phase,facing,timer,anim_timer,cursor,pose;
} ArcadePlayerShot;
void arcade_player_shot_step(ArcadePlayerShot *shot,u16 camera,u8 *available);
typedef struct {u16 x;u8 y,facing,state,kind,available,input,previous;} ArcadeFireInput;
typedef struct {u16 x;u8 y,facing;} ArcadeShotBirth;
u8 arcade_player_shot_fire(const ArcadeFireInput *input,u8 first_active,ArcadeShotBirth *birth);
#endif

#ifndef ARCADE_PLAYER_H
#define ARCADE_PLAYER_H
#include "arcade_types.h"
/* Typed gameplay state: no emulated registers, address space or CPU interpreter.
 * Coordinates preserve the source's upward Y byte and fractional carry. */
typedef struct {
    u16 x;
    u8 xf,y,yf,state,phase,timer,stamp,facing,previous,active,attached;
} ArcadePlayer;
typedef struct {u8 owner,cursor,timer,code,attr;} ArcadePlayerAnimation;
void arcade_player_animation_step(ArcadePlayerAnimation *animation);
/* 9094: session-preserving player setup used by course startup. Out flags
 * are source88/89; two_players is99. Leaves untouched fractions/history. */
void arcade_players_course_begin(ArcadePlayer players[2],ArcadePlayerAnimation animations[2],
    u16 camera,u8 two_players,const u8 out[2]);
/* Returns the original A847 push-out, or ARCADE_NO_FLOOR. */
#define ARCADE_NO_FLOOR (-32767)
typedef struct {u16 x;u8 y,facing;} ArcadeFirePose;
typedef s16 (*ArcadeFloor)(u16 x,u8 y,void *context);
/* 940A title motion record; camera zero and section-stop enabled. Caller
 * owns initial actor state, source input, floor data and player order. */
void arcade_player_step_prelude(ArcadePlayer *,ArcadePlayer *,u8 input,
    s16 (*floor)(u16,u8,void*),void *context,ArcadeFirePose *fire,ArcadePlayerAnimation *anim);
/* A9C5 skips both idle drift and its animation when section state is nonzero. */
void arcade_player_step_stopped(ArcadePlayer *p,ArcadePlayer *other,u8 input,u16 camera,u8 stopped,
    ArcadeFloor floor,void *context,ArcadeFirePose *fire,ArcadePlayerAnimation *anim);
void arcade_player_step_animated(ArcadePlayer *p,ArcadePlayer *other,u8 input,u16 camera,
    s16 (*floor)(u16,u8,void*),void *context,ArcadeFirePose *fire,ArcadePlayerAnimation *anim);
void arcade_players_separate(ArcadePlayer *p,ArcadePlayer *q);
void arcade_player_step_events(ArcadePlayer *p,ArcadePlayer *other,u8 input,u16 camera,
                               ArcadeFloor floor,void *context,ArcadeFirePose *fire);
void arcade_player_step(ArcadePlayer *p,ArcadePlayer *other,u8 input,u16 camera,
                        ArcadeFloor floor,void *context);
#endif

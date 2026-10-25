#ifndef ARCADE_D_ACTOR_H
#define ARCADE_D_ACTOR_H
#include "arcade_d_motion.h"
typedef struct {u16 next;u8 code,attr,timer;} ArcadeDFrame;
extern const ArcadeDFrame arcade_d_frames[];
extern const u16 arcade_d_poses[16],arcade_d_death[2],arcade_d_score[2];
extern const u8 arcade_d_wait[4];
typedef struct {
    u16 x,target_offset,animation;
    u8 xf,y,yf,active,state,phase,delay,kind,direction,anim_timer,timer,owner,bonus_kind,code,attr,target;
} ArcadeDActor;
/* BF30..C128. slot0..3; stopped is verified source9D domain0/1.
 * Caller supplies AD53's selected player and owns live/bonus counters. */
void arcade_d_actor_step(ArcadeDActor *actor,u8 slot,u16 camera,u8 clock,u8 difficulty,
    u8 stopped,u8 purge,u16 target_x,u8 target_y,u8 target,
    u8 *live,u8 *bonus_next,u8 bonus_limit);
#endif

#ifndef ARCADE_E_ACTOR_H
#define ARCADE_E_ACTOR_H
#include "arcade_e_motion.h"
typedef struct {u16 next;u8 code,attr,timer;} ArcadeEFrame;
extern const ArcadeEFrame arcade_e_frames[];
extern const u16 arcade_e_poses[16],arcade_e_death[2];
typedef struct {
    u16 x,animation;
    u8 xf,y,yf,active,state,phase,delay,turns,direction,anim_timer,timer,owner,bonus_kind,code,attr,target;
} ArcadeEActor;
/* C2D5..C4C7. slot0/1; source9D stopped domain0/1. eligible is AD53 carry clear.
 * available is sourceA4, live isA9; neither is inferred from visible sprites. */
void arcade_e_actor_step(ArcadeEActor *actor,u8 slot,u16 camera,u8 clock,u8 stopped,
    u8 purge,u16 target_x,u8 target_y,u8 target,u8 eligible,
    u8 *live,u8 *available,u8 *bonus_next,u8 bonus_limit);
#endif

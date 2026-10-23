#ifndef ARCADE_LIFE_H
#define ARCADE_LIFE_H
#include "arcade_types.h"
#include "arcade_player.h"
/* Source death states0..6. State7's input-sensitive reentry is separate. */
typedef struct {
    u16 x;
    u8 xf,y,yf,active,state,phase,kind,owner,facing,timer,reentry_lock;
    u8 cursor,anim_timer,code,attr;
} ArcadeLife;
void arcade_life_animation_step(ArcadeLife *actor);
/* Returns zero for state7+, without changing the actor. Event FFFF means none. */
u8 arcade_life_step(ArcadeLife *p,u16 camera,u8 stopped,u8 *reserves,u8 *gate,u16 *event,u8 *sound);
/* Returns whether AD10 firing was enabled; fire pose precedes scroll drift. */
u8 arcade_life_reentry(ArcadeLife *p,u8 *path,u8 clock,u8 stopped,
                       u8 input,u8 previous,ArcadeFirePose *fire);
/* Resident tile for verified death/idle/reentry poses; zero for invalid pose. */
u16 arcade_life_tile(u8 code,u8 attr);
#endif

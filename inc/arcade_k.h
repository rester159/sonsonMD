#ifndef ARCADE_K_H
#define ARCADE_K_H
#include "arcade_types.h"
#include "arcade_shots.h"
/* Kind2 in the shared CBC8 child pool, emitted by D74A. Origin's low byte
 * doubles as the six-update startup countdown, matching the source lifetime. */
typedef struct {
    u16 x,origin;
    u8 xf,y,yf,active,state,phase,facing,death_timer,cursor,anim_timer,code,attr;
} ArcadeKProjectile;
void arcade_k_projectile_step(ArcadeKProjectile *shot,u16 camera,u8 stopped,u8 purge);
u8 arcade_k_shot_target(const ArcadeKProjectile pool[4],u16 x,u8 y,u8 active);
u8 arcade_k_contact_target(const ArcadeKProjectile pool[4],u16 x,u8 y,u8 active);
/* EE44 ordinary shot cancellation; returns the source score event. */
u16 arcade_k_cancel(ArcadeKProjectile *enemy,ArcadePlayerShot *shot,u8 owner);
#endif

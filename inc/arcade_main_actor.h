#ifndef ARCADE_MAIN_ACTOR_H
#define ARCADE_MAIN_ACTOR_H
#include "arcade_types.h"
#include "arcade_shots.h"
#include "arcade_rewards.h"
typedef struct {u16 next;u8 code,attr,timer;} ArcadeMainFrame;
extern const ArcadeMainFrame arcade_main_frames[];
extern const u16 arcade_main_walk[4],arcade_main_death[2],arcade_main_wounded;
extern const s16 arcade_main_dx[5],arcade_main_dy[5];
typedef struct {
    u16 x,origin,cursor;
    u8 xf,y,yf,active,state,phase,direction,delay,variant,hp,hit,owner,timer,anim_timer,code,attr;
} ArcadeMainActor;
/* B9BA ordinary states0..5. Caller handles state255 bonus conversion.
 * slot is original0..15; preserves fractional and unrelated fields on entry. */
void arcade_main_actor_step(ArcadeMainActor *a,u8 slot,u16 camera,u8 clock,u8 stopped,u8 purge,u8 *live);
void arcade_main_actor_post(ArcadeMainActor *a,u16 camera,u8 purge);
void arcade_main_bonus_step(ArcadeMainActor *a,u16 camera,u8 purge,u8 *next,u8 limit);
typedef struct {u16 points,event,combat;u8 sound_hit,sound_kill;} ArcadeMainHit;
/* E373/E3A1: first eligible slot; byte Y offset includes enemy+8/shot+6. */
u8 arcade_main_shot_target(const ArcadeMainActor actors[16],u16 x,u8 y,u8 active);
void arcade_main_hit(ArcadeMainActor *a,ArcadePlayerShot *shot,u8 owner,u16 combat,ArcadeMainHit *hit);
u8 arcade_main_contact_target(const ArcadeMainActor actors[16],u16 x,u8 y,u8 active);
u8 arcade_main_bonus_collect(ArcadeMainActor *a,u8 owner,u16 combat,ArcadeBonusAward *award);
#endif

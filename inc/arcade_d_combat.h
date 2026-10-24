#ifndef ARCADE_D_COMBAT_H
#define ARCADE_D_COMBAT_H
#include "arcade_d_actor.h"
#include "arcade_shots.h"
#include "arcade_rewards.h"
typedef struct {u16 points,event,bonus_points,bonus_event,combat;u8 sound,bonus_sound;} ArcadeDHit;
/* E4CA/E4F8: four dedicated slots, first eligible overlap. */
u8 arcade_d_shot_target(const ArcadeDActor actors[4],u16 x,u8 y,u8 active);
/* E514: remaining is source B1, independent of the live actor count. */
void arcade_d_hit(ArcadeDActor *a,ArcadePlayerShot *shot,u8 owner,u8 *remaining,u16 combat,ArcadeDHit *hit);
u8 arcade_d_contact_target(const ArcadeDActor actors[4],u16 x,u8 y,u8 active);
u8 arcade_d_bonus_collect(ArcadeDActor *a,u8 owner,u16 combat,ArcadeBonusAward *award);
#endif

#ifndef ARCADE_C_COMBAT_H
#define ARCADE_C_COMBAT_H
#include "arcade_c_actor.h"
#include "arcade_shots.h"
#include "arcade_rewards.h"
typedef struct {u16 points,event,bonus_points,bonus_event,combat;u8 sound,bonus_sound;} ArcadeCHit;
/* E424/E452: six dedicated slots, first eligible overlap. */
u8 arcade_c_shot_target(const ArcadeCActor actors[6],u16 x,u8 y,u8 active);
/* E46E: remaining is source B0, independent of the live actor count. */
void arcade_c_hit(ArcadeCActor *a,ArcadePlayerShot *shot,u8 owner,u8 *remaining,u16 combat,ArcadeCHit *hit);
u8 arcade_c_contact_target(const ArcadeCActor actors[6],u16 x,u8 y,u8 active);
u8 arcade_c_bonus_collect(ArcadeCActor *a,u8 owner,u16 combat,ArcadeBonusAward *award);
#endif

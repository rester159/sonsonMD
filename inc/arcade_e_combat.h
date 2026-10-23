#ifndef ARCADE_E_COMBAT_H
#define ARCADE_E_COMBAT_H
#include "arcade_e_actor.h"
#include "arcade_shots.h"
#include "arcade_rewards.h"
typedef struct {u16 points,event,combat;u8 sound;} ArcadeEHit;
/* E761 checks the shot once, then scans both slots even after consuming it.
 * Return a bit mask, not the first target. Apply hits in ascending slot order. */
u8 arcade_e_shot_targets(const ArcadeEActor actors[2],u16 x,u8 y,u8 active);
void arcade_e_hit(ArcadeEActor *actor,ArcadePlayerShot *shot,u8 owner,u16 combat,ArcadeEHit *hit);
/* ED49/ED5B/ECE9: first overlap, including converted bonuses. */
u8 arcade_e_contact_target(const ArcadeEActor actors[2],u16 x,u8 y,u8 active);
u8 arcade_e_bonus_collect(ArcadeEActor *actor,u8 owner,u16 combat,ArcadeBonusAward *award);
#endif

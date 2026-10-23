#ifndef ARCADE_F_COMBAT_H
#define ARCADE_F_COMBAT_H
#include "arcade_f_actor.h"
#include "arcade_shots.h"
#include "arcade_rewards.h"
typedef struct {u16 points,event,bonus_points,bonus_event,combat;u8 sound,bonus_sound;} ArcadeFHit;
/* EB97/EBC5: four dedicated slots, first eligible overlap. */
u8 arcade_f_shot_target(const ArcadeFActor actors[4],u16 x,u8 y,u8 active);
/* EBE1: remaining is source B6, independent of the live actor count. */
void arcade_f_hit(ArcadeFActor *a,ArcadePlayerShot *shot,u8 owner,u8 *remaining,u16 combat,ArcadeFHit *hit);
u8 arcade_f_contact_target(const ArcadeFActor actors[4],u16 x,u8 y,u8 active);
u8 arcade_f_bonus_collect(ArcadeFActor *a,u8 owner,u16 combat,ArcadeBonusAward *award);
#endif

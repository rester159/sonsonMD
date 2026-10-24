#ifndef ARCADE_REWARDS_H
#define ARCADE_REWARDS_H
#include "arcade_types.h"
#include "arcade_foes.h"
#include "arcade_powerup.h"
#include "arcade_special_spawn.h"
typedef struct {
    u16 combat,points,event,bonus;
    u8 remaining,phase,owner;
} ArcadeGHKill;
/* Successful G/H hit, after geometry/eligibility. Source E8EA/E993. */
void arcade_gh_kill(ArcadeGHKill *out,u16 combat,u8 remaining,u8 family,u8 owner);
typedef struct {u16 combat,points,event;u8 sound;} ArcadeBonusAward;
/* ED18/E838: successful collection of a converted G/H actor. Valid kinds0..10.
 * Geometry/eligibility belongs to the contact scan. Invalid kinds leave state
 * and output untouched rather than indexing outside the recovered table. */
u8 arcade_gh_bonus_collect(ArcadeFoe *actor,u8 owner,u16 combat,ArcadeBonusAward *out);
/* E7F3/E823: ordinary kinds0..9, shared collection count and FB95 request.
 * Kind10 has a separate variant-dependent award and is not handled here. */
u8 arcade_pickup_collect(ArcadePowerup *actor,u16 x,u8 y,u8 owner,u16 combat,
                        u8 *count,ArcadeSpecialSpawn *spawn,
                        ArcadePowerup *conversion,ArcadeBonusAward *out);
/* E81F/E852: kind10 uses the source slot's separate variant byte (0..7).
 * Its award does not increase combat difficulty. Owner binding is separate. */
u8 arcade_variant_pickup_collect(ArcadePowerup *actor,u8 variant,u16 x,u8 y,
                        u8 owner,u16 combat,u8 *count,ArcadeSpecialSpawn *spawn,
                        ArcadePowerup *conversion,ArcadeBonusAward *out);
#endif

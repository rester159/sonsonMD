#ifndef ARCADE_PRELUDE_H
#define ARCADE_PRELUDE_H
#include "arcade_attract.h"
#include "arcade_player.h"
#include "arcade_bonus_art.h"
#include "arcade_rewards.h"
/* Two fixed B19A objects in the title, not gameplay pickup slots. */
typedef ArcadePowerup ArcadePreludeDecor;
typedef struct {
    ArcadePlayer players[2];
    ArcadePlayerAnimation animation[2];
    ArcadeAttract input;
    ArcadePreludeDecor decor[2];
} ArcadePrelude;
/* Call after world/session setup (9094). Retained fields are deliberate.
 * Source93BA sets actors, initializes scripts, executes A98A once, then
 * installs940A. Source941A installs decorations on the next title update. */
void arcade_prelude_players_begin(ArcadePrelude *,ArcadeFloor,void *,ArcadeFirePose fire[2]);
void arcade_prelude_decor_begin(ArcadePrelude *);
/* Shared source96/7C and FB95 request state remain lifecycle-owned. Event
 * commands are returned in P1/P2 then slot order; title score writes are gated
 * by the caller just as original4CBE checks live-game22. */
typedef struct {
    u16 combat;u8 collected;
    ArcadeSpecialSpawn spawn;ArcadePowerup conversion;
} ArcadePreludeRewards;
u8 arcade_prelude_collect(ArcadePrelude *,ArcadePreludeRewards *,ArcadeBonusAward events[2]);
/* 9463 ordering: scripted input, P1, P2, separation. Fixed decorations do not
 * animate in B23D's positive-kind state2. Caller owns shots, collisions,
 * source clock and all rendering. Prelude shot availability is zero (93D5),
 * so returned fire poses are not authorization to spawn shots. Returns0 if recovered input data is exhausted. */
u8 arcade_prelude_step(ArcadePrelude *,u8 clock,ArcadeFloor,void *,ArcadeFirePose fire[2]);
#endif

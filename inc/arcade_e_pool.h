#ifndef ARCADE_E_POOL_H
#define ARCADE_E_POOL_H
#include "arcade_e_actor.h"
#include "arcade_e_wave.h"
typedef struct {ArcadeEActor actors[2];ArcadeWaveControl wave;u8 live;} ArcadeEPool;
/* Warm reset clears active/live only; caller owns producer/shared A4 reset. */
void arcade_e_pool_reset(ArcadeEPool *pool,u8 cold);
u8 arcade_e_pool_schedule(ArcadeEPool *pool,u8 clock,u8 difficulty,u16 camera);
void arcade_e_pool_update(ArcadeEPool *pool,u16 camera,u8 clock,u8 stopped,u8 purge,
    u16 target_x,u8 target_y,u8 target,u8 eligible,u8 *available,u8 *next,u8 limit);
/* Post-update scratch: valid only until scheduling; not the source live count. */
extern u8 arcade_e_contact_presence;
#endif

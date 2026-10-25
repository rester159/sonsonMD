#ifndef ARCADE_F_POOL_H
#define ARCADE_F_POOL_H
#include "arcade_f_actor.h"
#include "arcade_f_wave.h"
typedef struct {ArcadeFActor actors[4];ArcadeFWave wave;u8 live;} ArcadeFPool;
/* Warm reset clears active/live only; caller owns producer/reservation reset. */
void arcade_f_pool_reset(ArcadeFPool *pool,u8 cold);
u8 arcade_f_pool_schedule(ArcadeFPool *pool,u8 clock,u8 difficulty,u16 camera);
void arcade_f_pool_update(ArcadeFPool *pool,u16 camera,u8 stopped,u8 purge,
    u8 *next,u8 limit);
/* Post-update scratch: valid only until scheduling; not the source live count. */
extern u8 arcade_f_contact_presence;
#endif

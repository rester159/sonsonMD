#ifndef ARCADE_D_POOL_H
#define ARCADE_D_POOL_H
#include "arcade_d_actor.h"
#include "arcade_d_wave.h"
typedef struct {ArcadeDActor actors[4];ArcadeDWave wave;u8 live;} ArcadeDPool;
/* Warm course reset retains actor residuals and producer state. */
void arcade_d_pool_reset(ArcadeDPool *pool,u8 cold);
void arcade_d_pool_update(ArcadeDPool *pool,u16 camera,u8 clock,u8 difficulty,u8 stopped,u8 purge,
    u16 target_x,u8 target_y,u8 target,u8 *next,u8 limit);
/* F3A3 writes every dedicated slot; actor initialization increments live later. */
u8 arcade_d_pool_schedule(ArcadeDPool *pool,u8 clock,u8 difficulty,u16 camera,u16 tracked_x);
/* Post-update scratch: valid only until scheduling; not the source live count. */
extern u8 arcade_d_contact_presence;
#endif

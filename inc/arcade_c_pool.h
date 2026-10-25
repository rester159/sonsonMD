#ifndef ARCADE_C_POOL_H
#define ARCADE_C_POOL_H
#include "arcade_c_actor.h"
#include "arcade_c_wave.h"
typedef struct {ArcadeCActor actors[6];ArcadeCWave wave;u8 live;} ArcadeCPool;
/* Warm course reset retains actor residuals and producer state. */
void arcade_c_pool_reset(ArcadeCPool *pool,u8 cold);
void arcade_c_pool_update(ArcadeCPool *pool,u16 camera,u8 stopped,u8 purge,u8 *next,u8 limit);
/* F2B4 writes every dedicated slot; actor initialization increments live later. */
u8 arcade_c_pool_schedule(ArcadeCPool *pool,u8 clock,u8 difficulty,u16 camera);
/* Post-update scratch: valid only until scheduling; not the source live count. */
extern u8 arcade_c_contact_presence;
#endif

#ifndef ARCADE_MAIN_POOL_H
#define ARCADE_MAIN_POOL_H
#include "arcade_main_actor.h"
#include "arcade_main_wave.h"
typedef struct {ArcadeMainActor actors[16];ArcadeMainWave wave;u8 live;} ArcadeMainPool;
/* Resets actors/live only; caller owns the producer controller reset. */
void arcade_main_pool_reset(ArcadeMainPool *pool,u8 cold);
void arcade_main_pool_update(ArcadeMainPool *pool,u16 camera,u8 clock,u8 stopped,u8 purge,u8 *next,u8 limit);
/* Called after actor updates/collisions in EF51 order. */
u8 arcade_main_pool_schedule(ArcadeMainPool *pool,u8 ready,u8 vsync,u8 clock,u8 difficulty,u16 camera,ArcadeRng *rng);
/* Post-update scratch: valid only until scheduling; not the source live count. */
extern u8 arcade_main_contact_presence;
#endif

#ifndef ARCADE_MAIN_WAVE_H
#define ARCADE_MAIN_WAVE_H
#include "arcade_waves.h"
typedef struct {ArcadeWaveControl control;u8 sequence,cursor,remaining;} ArcadeMainWave;
typedef struct {u16 x;u8 slot,y,variant,delay;} ArcadeMainBirth;
extern const u8 arcade_main_placements[16][12][2];
/* EFED..F0C8. Caller applies births in order: active128,state/phase0,
 * facing0; x/y/variant/delay from birth. Other actor fields survive.
 * vsync is source23, clock is source24; ready is sourceA4, live isA6. */
u8 arcade_main_wave_step(ArcadeMainWave *wave,u8 live,u8 ready,u8 vsync,
    u8 clock,u8 difficulty,u16 camera,const u8 occupied[16],ArcadeRng *rng,
    ArcadeMainBirth births[12]);
#endif

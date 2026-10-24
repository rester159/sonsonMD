#ifndef ARCADE_E_WAVE_H
#define ARCADE_E_WAVE_H
#include "arcade_waves.h"
typedef struct {u16 x;u8 y,delay,direction;} ArcadeEBirth;
extern const u8 arcade_e_placements[8][2][2];
/* F443..F4BB. Countdown uses global clock24 phase0/32, not a local timer.
 * Birth writes two dedicated slots; caller preserves all other actor fields. */
u8 arcade_e_wave_step(ArcadeWaveControl *control,u8 live,u8 clock,u8 difficulty,
                     u16 camera,ArcadeEBirth births[2]);
#endif

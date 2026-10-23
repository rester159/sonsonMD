#ifndef ARCADE_D_WAVE_H
#define ARCADE_D_WAVE_H
#include "arcade_waves.h"
typedef struct {ArcadeWaveControl control;u8 timer,kind,reservation;} ArcadeDWave;
typedef struct {u16 x,target_offset;u8 y,delay,kind,direction;} ArcadeDBirth;
extern const u8 arcade_d_placements[8][4][2];
/* F358..F402. tracked_x is the player chosen by source AD53. The four
 * dedicated actors retain residual fields; their initialization owns live. */
u8 arcade_d_wave_step(ArcadeDWave *wave,u8 live,u8 clock,u8 difficulty,
                     u16 camera,u16 tracked_x,ArcadeDBirth births[4]);
#endif

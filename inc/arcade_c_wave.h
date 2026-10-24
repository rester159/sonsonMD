#ifndef ARCADE_C_WAVE_H
#define ARCADE_C_WAVE_H
#include "arcade_waves.h"
typedef struct {ArcadeWaveControl control;u8 timer,kind,reservation;} ArcadeCWave;
typedef struct {u16 x;u8 y,delay,kind;} ArcadeCBirth;
extern const u8 arcade_c_placements[8][6][2];
/* F269..F2F7. Caller writes six dedicated slots (active128,state/phase0),
 * preserving all fields absent from births; actors increment live on init. */
u8 arcade_c_wave_step(ArcadeCWave *wave,u8 live,u8 clock,u8 difficulty,
                     u16 camera,ArcadeCBirth births[6]);
#endif

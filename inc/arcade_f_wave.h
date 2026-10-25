#ifndef ARCADE_F_WAVE_H
#define ARCADE_F_WAVE_H
#include "arcade_waves.h"
typedef struct {ArcadeWaveControl control;u8 reservation;} ArcadeFWave;
typedef struct {u16 x;u8 y,origin_y,delay,direction;} ArcadeFBirth;
extern const u8 arcade_f_placements[4][8][4][2];
/* F4DC..F579: live is sourceAE; reservation isB6. Source24 clocks wait. */
u8 arcade_f_wave_step(ArcadeFWave *wave,u8 live,u8 clock,u8 difficulty,
    u16 camera,ArcadeFBirth births[4]);
#endif

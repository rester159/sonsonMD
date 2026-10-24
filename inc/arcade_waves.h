#ifndef ARCADE_WAVES_H
#define ARCADE_WAVES_H
#include "arcade_types.h"
#define ARCADE_WAVE_CONTROLLERS 12
#define ARCADE_FORMATION_SIZE 6
/* Typed controllers, not source RAM records. $EF7C/$EFBB arm one class/tick. */
typedef struct {u16 auxiliary;u8 active,state,phase,wait,parameter;} ArcadeWaveControl;
typedef struct {
    u16 mask,placement;
    u8 cached_coarse,state,cursor,remaining,delay;
    ArcadeWaveControl controls[ARCADE_WAVE_CONTROLLERS];
} ArcadeWaves;
typedef struct {u16 seed;u8 last;} ArcadeRng;
typedef struct {u16 x;u8 y,delay,facing;} ArcadeBirth;
/* Source entry states before ordinary G/H combat AI. X has an 8-bit fraction. */
typedef struct {u16 x;u8 xf,y,yf,active,state,phase,delay,facing;} ArcadeFormationActor;
void arcade_formation_enter(ArcadeFormationActor *actor,u8 family,u16 camera,u8 scroll_stopped);
u8 arcade_rng_next(ArcadeRng *rng);
void arcade_waves_reset(ArcadeWaves *waves,u8 cached_coarse);
void arcade_waves_course_begin(ArcadeWaves *waves);
/* Returns the changed controller index, or -1 for load/idle/delay ticks. */
s16 arcade_waves_step(ArcadeWaves *waves,u8 coarse,u8 clock_phase);
/* $F682/$F7F3. family=0:G,1:H. Returns six only on the formation birth tick.
 * live_count includes reserved actors. Caller owns dedicated class slots and
 * invokes controllers after actor updates. tracked_x follows $AD53's selection. */
u8 arcade_formation_step(ArcadeWaveControl *control,u8 family,u8 live_count,
                         u8 clock_phase,u8 stage,u16 camera,u16 tracked_x,
                         ArcadeRng *rng,ArcadeBirth births[ARCADE_FORMATION_SIZE],
                         u8 *reservation);
#endif

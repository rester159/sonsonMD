#ifndef ARCADE_LARGE_H
#define ARCADE_LARGE_H
#include "arcade_foes.h"
#include "arcade_shots.h"
/* Source AD53 returns P1 even when neither player is eligible. */
u8 arcade_large_target(const ArcadeFoeWorld *world,u8 *eligible);
/* D6F3/D706. RNG state is original $78:$79; sample is retained $7A.
 * Caller owns RNG globally and must not pre-advance it. */
u8 arcade_large_steer(u8 y,u8 target_y,u8 hp,u8 previous_direction,
                       u8 clock,u16 *rng,u8 *sample);
typedef struct {u16 next;u8 duration,code[4];} ArcadeLargeFrame;
typedef struct {u16 cursor;u8 timer,code[4],attr[4];} ArcadeLargeAnimation;
extern const ArcadeLargeFrame arcade_large_frames[];
extern const u16 arcade_large_walk[2][8];
extern const u16 arcade_large_entry,arcade_large_exit,arcade_large_death;
void arcade_large_animate(ArcadeLargeAnimation *animation);
/* D924 consumes pending damage; valid HP 0..7 and subtype 0..2. */
void arcade_large_damage_pose(ArcadeLargeAnimation *animation,u8 hp,u8 facing,
                              u8 subtype,u8 *pending);
typedef struct {u16 x;u8 y,facing,slot;} ArcadeLargeShotBirth;
/* D74A checks nonzero active flags, unlike AD53 targeting. Returns a birth
 * only when a slot is free. Caller preserves child fields not named here,
 * sets active=1, state/phase=0 and kind=2, then commits this birth. */
u8 arcade_large_fire(u16 x,u8 y,u8 *countdown,u8 period,
                     const ArcadeFoePlayer players[2],const u8 occupied[4],
                     ArcadeLargeShotBirth *birth);
extern const u16 arcade_large_score[4][3];
typedef struct {
    ArcadeLargeAnimation animation;
    u8 active,state,phase,subtype,variant,owner,hp,timer;
} ArcadeLargeBody;
/* Entire D7B4 death/exit dispatch; outer camera compensation is caller-owned. */
void arcade_large_death_step(ArcadeLargeBody *body);
typedef struct {
    ArcadeLargeBody body;
    u16 x;
    u8 xf,y,yf,direction,facing,hit,lifetime,fire_timer,fire_period,target,retreat;
} ArcadeLargeActor;
extern const ArcadeFoeVector arcade_large_vectors[2][3][5];
extern const u8 arcade_large_thresholds[48],arcade_large_fire_periods[12],arcade_large_lifetimes[12];
/* Complete ordinary D55B actor dispatcher. Returns an optional child birth.
 * World scroll_stopped corresponds to source $9D, difficulty to high $96. */
u8 arcade_large_step(ArcadeLargeActor *actor,const ArcadeFoeWorld *world,
                     u8 difficulty,u16 *rng,u8 *sample,
                     const u8 occupied[4],ArcadeLargeShotBirth *birth);
/* D58B alone: used by the controller without outer camera compensation. */
void arcade_large_initialize(ArcadeLargeActor *actor,u8 difficulty);
/* FAFA: auxiliary is the owned-slot cursor, wait is remaining visits.
 * Returns a one-bit birth mask, or zero. Occupied slots still consume a visit. */
u8 arcade_large_wave_step(ArcadeWaveControl *control,ArcadeLargeActor actors[3],
                          u16 camera,u8 difficulty);
typedef struct {u16 combat,event,points;u8 sound;} ArcadeLargeHit;
u8 arcade_large_shot_overlap(u16 enemy_x,u8 enemy_y,u16 shot_x,u8 shot_y);
/* EE8E accepted hit; caller owns eligibility/scan order and effect delivery. */
void arcade_large_hit(ArcadeLargeActor *enemy,ArcadePlayerShot *shot,u8 owner,
                      u16 combat,ArcadeLargeHit *result);
u8 arcade_large_shot_target(const ArcadeLargeActor actors[3],u16 x,u8 y,u8 active);
u8 arcade_large_contact_target(const ArcadeLargeActor actors[3],u16 x,u8 y,u8 active);
/* Source hardware cells: byte coordinates and original attribute bits.
 * Conversion to Genesis tiles/palette/flip is renderer-owned. */
typedef struct {u8 y,attr,code,x;} ArcadeLargeCell;
u8 arcade_large_cells(const ArcadeLargeActor *actor,u16 camera,u8 *remaining,
                       ArcadeLargeCell cells[4]);
#endif

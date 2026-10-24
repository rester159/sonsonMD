#ifndef ARCADE_I_H
#define ARCADE_I_H
#include "arcade_waves.h"
#include "arcade_rewards.h"
/* Typed class-I controller. Cursor indexes its four owned actor slots. */
typedef struct {ArcadeWaveControl control;u8 cursor,remaining;} ArcadeIWave;
u8 arcade_i_wave_step(ArcadeIWave *wave,u8 live,u8 clock,u16 camera,
                      u8 *reservation,ArcadeBirth *birth);
/* Ordinary class-I hopping actor. Shared $FF bonus states remain separate. */
typedef struct {
    u32 x_speed,y_speed;
    ArcadeFormationActor body;
    u8 cursor,code,attr,anim_timer,hop_timer,hit_style;
} ArcadeIActor;
void arcade_i_actor_step(ArcadeIActor *actor,u16 camera,u8 scroll_stopped,u8 *live);
u8 arcade_i_shot_target(const ArcadeIActor actors[4],u16 shot_x,u8 shot_y);
void arcade_i_kill(ArcadeGHKill *out,u16 combat,u8 remaining,u8 owner);
u16 arcade_i_tile(u8 code,u8 attr);
#endif

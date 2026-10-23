#ifndef ARCADE_C_ACTOR_H
#define ARCADE_C_ACTOR_H
#include "arcade_types.h"
typedef struct {u16 next;u8 code,attr,timer;} ArcadeCFrame;
typedef struct {u16 animation;u8 direction,timer;} ArcadeCScript;
typedef struct {s16 dx,dy;} ArcadeCVector;
extern const ArcadeCFrame arcade_c_frames[];
extern const ArcadeCScript arcade_c_script[];
extern const ArcadeCVector arcade_c_vectors[2][17];
extern const u16 arcade_c_initial,arcade_c_death[2],arcade_c_score[2];
typedef struct {
    u16 x,animation,script;
    u8 xf,y,yf,active,state,phase,delay,kind,direction,script_timer,anim_timer,timer,owner,bonus_kind,code,attr;
} ArcadeCActor;
/* BBDE..BCFE. Recovered normal birth kind0; caller maintains live/bonus state.
 * Motion table selector stopped is source9D in its recovered0/1 domain.
 * Animation65535 is the unused continuation after the terminal script pose. */
void arcade_c_actor_step(ArcadeCActor *actor,u16 camera,u8 stopped,u8 purge,
                         u8 *live,u8 *bonus_next,u8 bonus_limit);
#endif

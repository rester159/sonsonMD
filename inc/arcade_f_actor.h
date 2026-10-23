#ifndef ARCADE_F_ACTOR_H
#define ARCADE_F_ACTOR_H
#include "arcade_f_motion.h"
typedef struct {u16 next;u8 code,attr,timer;} ArcadeFFrame;
extern const ArcadeFFrame arcade_f_frames[];
extern const u16 arcade_f_flight[2],arcade_f_death[2],arcade_f_score[2];
typedef struct {
 u16 x,animation,wave_phase;
 u8 xf,y,yf,active,state,phase,delay,direction,anim_timer,origin_y,half,amplitude,owner,bonus_kind,code,attr;
} ArcadeFActor;
/* DE95..E02F. origin_y is source+8, reused as the death countdown. */
void arcade_f_actor_step(ArcadeFActor *actor,u16 camera,u8 stopped,u8 purge,
 u8 *live,u8 *bonus_next,u8 bonus_limit);
#endif

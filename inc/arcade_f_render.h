#ifndef ARCADE_F_RENDER_H
#define ARCADE_F_RENDER_H
#include "arcade_f_actor.h"
/* Append a resident F source pose; caller owns final SAT termination. */
u16 arcade_f_render_actor(const ArcadeFActor *actor,u16 camera,u16 *sat,u16 first,u16 limit);
#endif

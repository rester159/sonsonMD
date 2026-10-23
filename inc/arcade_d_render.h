#ifndef ARCADE_D_RENDER_H
#define ARCADE_D_RENDER_H
#include "arcade_d_actor.h"
/* Append a resident D source pose; caller owns final SAT termination. */
u16 arcade_d_render_actor(const ArcadeDActor *actor,u16 camera,u16 *sat,u16 first,u16 limit);
#endif

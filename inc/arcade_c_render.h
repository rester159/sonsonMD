#ifndef ARCADE_C_RENDER_H
#define ARCADE_C_RENDER_H
#include "arcade_c_actor.h"
/* Append a resident C source pose; caller owns final SAT termination. */
u16 arcade_c_render_actor(const ArcadeCActor *actor,u16 camera,u16 *sat,u16 first,u16 limit);
#endif

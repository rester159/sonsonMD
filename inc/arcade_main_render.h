#ifndef ARCADE_MAIN_RENDER_H
#define ARCADE_MAIN_RENDER_H
#include "arcade_main_actor.h"
/* Append source poses in stable pool order; caller terminates the SAT chain. */
u16 arcade_main_render(const ArcadeMainActor actors[16],u16 camera,u16 *sat,u16 first,u16 limit);
u16 arcade_main_render_masked(const ArcadeMainActor actors[16],u16 mask,u16 camera,u16 *sat,u16 first,u16 limit);
u16 arcade_main_render_actor(const ArcadeMainActor *actor,u16 camera,u16 *sat,u16 first,u16 limit);
#endif

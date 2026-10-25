#ifndef ARCADE_E_RENDER_H
#define ARCADE_E_RENDER_H
#include "arcade_e_actor.h"
/* Required cache cell0..11, or65535 for resident/hidden/unknown art. */
u16 arcade_e_render_request(const ArcadeEActor *actor);
/* ready_cell must identify the successfully queued/flushed pose in this slot.
 * Caller owns transfer/SAT ordering, two dedicated slots and final SAT link. */
u16 arcade_e_render_actor(const ArcadeEActor *actor,u8 slot,u16 ready_cell,
    u16 camera,u16 *sat,u16 first,u16 limit);
#endif

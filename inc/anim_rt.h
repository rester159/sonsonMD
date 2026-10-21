/* anim_rt.h -- tiny shared ANIMATION RUNTIME for the native SonSon engine.
 *
 * Hand-written (NOT generated). Consumes the generated ss_anim.h (the baked
 * frame tiles + the per-anim {tile,duration} tables) read-only, and drives one
 * actor's animation cursor: pick an anim by state, advance it by the script's
 * per-frame durations (the arcade cadence captured in ss_anim.c), and yield the
 * current frame's absolute VRAM tile. This is the native re-expression of the
 * arcade's sub_9617 anim stepper (countdown the frame timer, then step the
 * cursor; loop or hold-last per the anim's loop flag) -- a table lookup + a
 * counter per actor per frame, so it costs essentially nothing (holds 60fps).
 *
 * Usage:
 *   Anim a;
 *   anim_set(&a, SS_ANIM_PLAYER_RUN);     // (re)start an anim (no-op if same)
 *   ... each frame:
 *   anim_set(&a, selected_id);            // switch on state change
 *   u16 tile = anim_tick(&a);             // advance + get the current frame tile
 *   bool done = anim_finished(&a);        // one-shot anim reached its last frame
 */
#ifndef ANIM_RT_H
#define ANIM_RT_H

#include <genesis.h>
#include "ss_anim.h"

/* Per-actor animation cursor. Small (4 bytes) -- embed in the actor struct. */
typedef struct
{
    u8 id;      /* current SSAnimId                                            */
    u8 frame;   /* current frame index within ss_anims[id].frames             */
    u8 timer;   /* frames left to hold the current frame (counts down to 0)    */
    u8 _pad;
} Anim;

/* (Re)select an animation. If `id` is already the current anim this is a no-op
 * (so calling it every frame with the state-derived id just keeps it running);
 * switching to a different id restarts at frame 0 with that frame's duration. */
static inline void anim_set(Anim *a, u8 id)
{
    if (a->id == id) return;
    a->id    = id;
    a->frame = 0;
    a->timer = ss_anims[id].frames[0].dur;
}

/* Force-restart the current (or a given) anim at frame 0 -- used when an actor
 * re-enters a one-shot anim it may already be "in" (e.g. fire again). */
static inline void anim_restart(Anim *a, u8 id)
{
    a->id    = id;
    a->frame = 0;
    a->timer = ss_anims[id].frames[0].dur;
}

/* Advance the cursor one game-frame and return the CURRENT frame's absolute
 * VRAM tile (SPRITE_SIZE(2,2) base). Looping anims wrap; one-shot anims hold
 * their last frame (anim_finished() then reports true). Mirrors sub_9617:
 * decrement the per-frame timer; on 0 step to the next frame + reload its dur. */
static inline u16 anim_tick(Anim *a)
{
    const SSAnim *an = &ss_anims[a->id];
    if (a->timer != 0) a->timer--;
    if (a->timer == 0)
    {
        u8 next = (u8)(a->frame + 1);
        if (next >= an->count)
        {
            if (an->loop) next = 0;          /* loop back to the start          */
            else          next = (u8)(an->count - 1);  /* hold the last frame   */
        }
        if (next != a->frame)
        {
            a->frame = next;
            a->timer = an->frames[next].dur;
        }
        else
        {
            /* held last frame of a one-shot: keep timer at 0 (finished) */
        }
    }
    return an->frames[a->frame].tile;
}

/* The current frame's tile WITHOUT advancing (for a render that ticked earlier). */
static inline u16 anim_tile(const Anim *a)
{
    return ss_anims[a->id].frames[a->frame].tile;
}

/* True once a one-shot (non-looping) anim is sitting on its last frame. */
static inline bool anim_finished(const Anim *a)
{
    const SSAnim *an = &ss_anims[a->id];
    return (!an->loop) && (a->frame == (u8)(an->count - 1)) && (a->timer == 0);
}

#endif /* ANIM_RT_H */

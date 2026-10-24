/* Whole-frame adapter: the tracer pairs run from one frame start ($EC42)
 * to the next, so the native tick runs first and the next VBlank's pad read
 * (taken from the expected state; IRQ bytes are masked) follows. */
#include "ss2_ram.h"
#include "ss2_logic.h"
extern unsigned char ss2_expected_pad;
void ss2_frame_tick_then_vblank(void)
{
    ss2_game_tick();
    ss2_game_vblank(ss2_expected_pad);
}

/* Coroutine mode: the real scheduler. Created on the first frame, then
 * each frame runs the ready tasks and the VBlank part, like the HuCard. */
#include "ss2_task.h"
void ss2_frame_coro(void)
{
    static int started;
    if (!started) { ss2_game_start(); started = 1; }
    ss2_sched_run();
    ss2_game_vblank(ss2_expected_pad);
    ss2_sched_vblank();
}

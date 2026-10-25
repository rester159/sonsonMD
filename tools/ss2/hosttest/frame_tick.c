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

/* VBlank-bracketed pairs (capture_vbl.py): each pair starts at a VBlank,
 * so the IRQ part comes first and the ready tasks then run to the next. */
void ss2_frame_vbl(void)
{
    static int started;
    if (!started) { ss2_game_start(); started = 1; }
    ss2_game_vblank(ss2_expected_pad);
    ss2_sched_vblank();
    ss2_sched_run();
}

/* Power-on traces (capture_boot.py): the native game boots and runs until
 * its tasks wait (the source's reset code before the first VBlank), then
 * each pair is a VBlank and the tasks after it. */
#include <stdio.h>
#include <stdlib.h>
void ss2_frame_vbl_boot(void)
{
    static int started, n;
    if (!started) { ss2_power_on(); ss2_sched_run(); started = 1; }
    /* SS2_PAD0: the trace had no input (the attract demo writes $16 itself,
     * so the post-state pad is not what the hardware read) */
    ss2_game_vblank(getenv("SS2_PAD0") ? 0 : ss2_expected_pad);
    ss2_sched_vblank();
    ss2_sched_run();
    if (getenv("SS2_DUMPAT") && ++n == atoi(getenv("SS2_DUMPAT")))
    {
        FILE *f = fopen("out/ss2/native_dump.bin", "wb");
        fwrite(ss2_ram, 1, 0x2000, f);
        fclose(f);
    }
}

/* The ending (research/ending/cap.py): the first pair runs from the
 * injection at a frame start (task 0 = $F617 with $69 = 6) to the next
 * VBlank, so the native tasks start there without a VBlank part. */
void ss2_frame_vbl_ending(void)
{
    static int started;
    if (!started)
    {
        u8 a = ZP(0x10), b = ZP(0x11);       /* the trace jumps to $F617: no $E1E2 */
        ss2_game_start();
        ss2_stage_clear_start();
        ZP(0x10) = a; ZP(0x11) = b;
        ss2_sched_run();
        started = 1;
        return;
    }
    ss2_game_vblank(ss2_expected_pad);
    ss2_sched_vblank();
    ss2_sched_run();
}

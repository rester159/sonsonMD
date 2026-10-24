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

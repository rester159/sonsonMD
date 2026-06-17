/* capcom_splash.c -- CAPCOM boot-logo sequence (native boot-flow, Phase 8).
 *
 * Ported from the sibling port's capcom_splash.c (HAND wrapper over the
 * capcom_logo kit, extracted from SF2' Special Champion Edition USA): gold/blue
 * CAPCOM logotype on black, palette fade-in, glint sweep at the jingle's
 * climax, fade-out, with the FM jingle replayed register-exact. ~6.0 s.
 *
 * No old-port dependency: the `palette_black` it references is a BUILT-IN SGDK
 * symbol (pal.h: `extern const u16* const palette_black;`), not an engine
 * global, so this stage is self-contained (SGDK + the kit only).
 *
 * CRITICAL gotcha (project memory, capcom-intro-extraction): the kit replays
 * the jingle by writing the YM2612 directly from the 68k, which is only
 * possible while the 68k OWNS the Z80 bus -- so Z80_requestBus() must bracket
 * the whole sequence (without it the jingle is silently dropped). Preserved
 * verbatim, including the Z80_init() teardown that fixes the per-VBlank
 * bus-protect stall the bare release left behind.
 *
 * Contract (mirrors sega_splash_run): called right after sega_splash_run() in
 * the default H40 mode; tears down to black planes + black palette line 0 and a
 * reset YM with the Z80 returned to SGDK's clean halt state, so game_init can
 * re-initialise the VDP from a known-blank screen.
 */
#include <genesis.h>
#include "frontend.h"
#include "capcom_logo.h"

/* palette_black is provided by SGDK (pal.h) -- a const pointer to 16 zero
 * color words = a fully black palette line. */

void capcom_splash_run(void)
{
    /* Stage dark before drawing (same anti-flash discipline as the SEGA
     * splash): the kit's reset() leaves capcom_logo_cram at the black pre-fade
     * state, but zero line 0 first so the tile upload is invisible even for a
     * frame. */
    PAL_setColors(0, palette_black, 16, CPU);
    VDP_setBackgroundColor(0);

    Z80_requestBus(TRUE);              /* REQUIRED for 68k->YM (kit README) */
    capcom_intro_reset();              /* YM patch snapshot + t=0 palette   */
    PAL_setColors(0, capcom_logo_cram, 16, CPU);
    capcom_logo_draw(TILE_USER_INDEX, 14, 12);   /* centered on H40        */

    while (capcom_intro_step()) {      /* jingle + fade-in/glint/fade-out  */
        VDP_waitVSync();
        PAL_setColors(0, capcom_logo_cram, 16, CPU);
    }

    /* teardown: silence + black, leave the VDP pristine for game_init.
     * Z80 RESTORE (the kit held the bus the whole ~6 s jingle): a bare
     * Z80_releaseBus() leaves the Z80 running with no valid program, and SGDK's
     * per-VBlank DMA flush does a Z80 bus-protect handshake every frame which
     * stalls against that bad state (measured: in-game service ratio collapsed
     * to a flat 0.46). Z80_init() resets the Z80 to SGDK's clean boot/halt
     * state so the per-frame handshake is instant again. */
    YM2612_reset();
    Z80_releaseBus();
    Z80_init();
    PAL_setColors(0, palette_black, 16, CPU);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
}

#include <genesis.h>
#include "game.h"
#include "frontend.h"

/* ===========================================================================
 *  SON SON  --  NATIVE Sega Genesis re-implementation
 * ===========================================================================
 *  BOOT-FLOW STATE MACHINE (Phase 8 -- presentation front-end).
 *
 *  The native build now ships the full owner presentation before the game:
 *
 *      SEGA splash  ->  CAPCOM logo intro  ->  ARCADE/HOME selector  ->  game
 *
 *  The two logo splashes reuse the owner's verified SGDK kits verbatim
 *  (sega_logo / capcom_logo); the selector is a clean native re-creation that
 *  carries the SONSON title + the "PORTED BY RESTER 159, 2026" credit and
 *  returns the chosen mode (stored in g_mode). See inc/frontend.h, ROADMAP.md
 *  Phase 8, and ../sonson/port/FRONTEND_SPEC.md.
 *
 *  Each front-end stage is BLOCKING and runs in the default H40 mode (the
 *  CAPCOM/SEGA cards are laid out for 40 columns), drawing its own tiles +
 *  palette and tearing the VDP/sound hardware back to black on return. Only
 *  AFTER the front-end do we switch to the game's H32 (256px) video mode and
 *  call game_init(), which fully re-initialises the palettes / tiles / planes
 *  -- so the front-end and the game never need to share VRAM or palette state.
 *
 *  The game itself is unchanged from Phase 3/4: a clean native loop --
 *  read input + simulate (game_update), build the SAT + scroll + FPS readout
 *  (game_render), then hand the frame to SGDK (SYS_doVBlankProcess) which
 *  flushes the DMA queue and waits for V-blank (the single 60Hz timebase).
 * =========================================================================== */

int main(bool hardReset)
{
    (void)hardReset;

    /* The front-end stages advance frames with VDP_waitVSync() (a busy-poll on
     * the VDP VBlank status flag), which needs the display enabled + interrupts
     * on. SGDK enables both at init; assert them explicitly here so the boot
     * flow never depends on startup ordering before the first stage draws. */
    VDP_setEnable(TRUE);
    SYS_enableInts();

    /* Pads up for the selector (the splashes don't read input, but the engine
     * is off during the whole front-end, so we own JOY_update there). Both
     * pads are forced to 3-button mode in selector_run() to suppress the
     * 6-button TH-detect phantom presses at power-on. */
    JOY_init();

    /* ---- BOOT FRONT-END (default H40) ---- */
    sega_splash_run();                    /* SEGA opening (Sonic-1 + chant)    */
    capcom_splash_run();                  /* CAPCOM logo + register-exact FM   */
    g_mode = selector_run();              /* ARCADE / HOME + RESTER credit     */

    /* TODO Phase 8b: arcade DIP-switch screen + home options menu.
     * For MODE_ARCADE expose the DIPs (lives/difficulty/bonus/coinage) as a
     * config screen; for MODE_HOME show the title menu + options + credits.
     * Both then feed a game-params struct the native engine reads. For this
     * iteration both modes fall straight through into the game; g_mode is
     * stored so the branch can be added here without touching the boot flow. */
    if (g_mode == MODE_ARCADE) { /* TODO Phase 8b: arcade DIP-switch screen */ }
    else                       { /* TODO Phase 8b: home options menu        */ }

    /* ---- GAME VIDEO MODE (after the front-end) ----
     * SonSon is 256x224 (H32). Plane A = 64x32: the wide level scrolls through
     * it as a 64-col sliding window (column streaming -- see game.c). 64 is the
     * max VDP plane width. (The front-end ran in H40; switch now.) */
    VDP_setScreenWidth256();
    VDP_setPlaneSize(64, 32, TRUE);

    game_init();                          /* re-inits palettes/tiles/planes    */

    while (TRUE)
    {
        game_update();
        game_render();
        SYS_doVBlankProcess();
    }
    return 0;
}

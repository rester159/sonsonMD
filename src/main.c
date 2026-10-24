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
 *      SEGA splash  ->  CAPCOM logo intro  ->  SELECT GAME  ->
 *      ARCADE/HOME selector  ->  game
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
 *  call game_run(), which fully re-initialises the palettes / tiles / planes
 *  -- so the front-end and the game never need to share VRAM or palette state.
 *
 *  RE-ENTRY (Phase 8c game-flow): the game now has STAKES -- lose all your lives
 *  and it's GAME OVER, after which the boot flow returns to the SELECTOR for a
 *  fresh session (selector -> play -> die -> game-over -> selector). This mirrors
 *  the arcade's game-over unwind back to attract (project memory: the front-end
 *  "re-entry for(;;) loop in main()"). So the boot is split:
 *    - SEGA + CAPCOM run ONCE (boot-only branding), then
 *    - a for(;;) SESSION loop: selector -> per-mode config -> game_run().
 *  game_run() returns when the game is over; the loop restores the front-end's
 *  H40 video mode and falls back to the selector. The mode + g_config are
 *  re-chosen from the selector + its config screen on every re-entry, so the
 *  player can switch ARCADE/HOME and re-pick LIVES/difficulty between sessions.
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

    /* ---- BOOT BRANDING (ONCE, default H40) ----
     * SEGA + CAPCOM are the power-on splashes; they play a single time and are
     * NOT part of the re-entry loop (a game-over returns to the selector, not
     * the logos). */
    sega_splash_run();                    /* SEGA opening (Sonic-1 + chant)    */
    capcom_splash_run();                  /* CAPCOM logo + register-exact FM   */

    /* ---- SESSION LOOP: selector -> config -> play -> (game over) -> selector */
    while (TRUE)
    {
        /* Restore the front-end's video mode every iteration. The first pass
         * inherits SGDK's power-on default (H40, plane 64x32), but a returning
         * pass comes back from the game's H32 / 64x32 column-streamed mode, so
         * re-assert H40 + the default plane so the selector + config screens
         * (laid out for 40 columns) render exactly as on the first boot. */
        VDP_setScreenWidth320();
        VDP_setPlaneSize(64, 32, TRUE);

        /* SELECT GAME: SONSON goes on to the ARCADE/HOME selector below.
         * SON SON II has no port in this ROM yet -- picking it stays on the
         * game-select screen until it's wired up. */
        if (game_select_run() != GAME_SONSON)
            continue;

        g_mode = selector_run();          /* ARCADE / HOME + RESTER credit     */

        /* Phase 8b: per-mode config screen. ARCADE shows the authentic arcade
         * DIP bank (Coinage/Lives/Bonus/Difficulty/Demo/Flip/...); HOME shows
         * the console-style options subset (Lives/Bonus/Difficulty/Demo Sounds).
         * Both are blocking, run in the front-end H40 text mode, write g_config
         * (game.h), and tear back to a blank screen before the game video mode.
         * The engine reads g_config (lives/difficulty/...) in game_init. */
        if (g_mode == MODE_ARCADE) dip_screen_run();   /* arcade DIP / service */
        /* HOME: no up-front config screen -- the home title MENU (1 PLAYER /
         * 2 PLAYERS / OPTIONS, in title_attract_run below) provides OPTIONS
         * itself, following the deprecated port's console front-end (home.c). */

        /* ---- GAME VIDEO MODE (after the front-end) ----
         * SonSon is 256x224 (H32). Plane A = 64x32: the wide level scrolls
         * through it as a 64-col sliding window (column streaming -- see game.c).
         * 64 is the max VDP plane width. (The front-end ran in H40; switch now.) */
        VDP_setScreenWidth256();
        VDP_setPlaneSize(64, 32, TRUE);

        /* Arcade TITLE screen + attract: the baked "SONSON" title (RE'd from the
         * real machine) in the game's H32 mode. Blocks until START, then a real
         * game begins. (Idle attract demo: TODO next.) */
        title_attract_run(g_mode);

        /* Play one session. game_run() does game_init() (re-inits palettes /
         * tiles / planes + reads g_config.lives) then runs the update/render/
         * V-blank loop, returning here when the game is over -> back to the
         * selector for the next session. */
        game_run();
    }
    return 0;
}

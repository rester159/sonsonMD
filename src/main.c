#include <genesis.h>
#include "game.h"
#include "frontend.h"
#include "save.h"

/* Source-only distribution: boot branding is excluded. */

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
    save_init();

    /* ---- BOOT BRANDING (ONCE, default H40) ----
     * SEGA + CAPCOM are the power-on splashes; they play a single time and are
     * NOT part of the re-entry loop (a game-over returns to the selector, not
     * the logos). */
    /* Private SEGA/CAPCOM intro modules are omitted from this export. */


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

        g_mode = selector_run();          /* ARCADE / HOME + RESTER credit     */

        /* Phase 8b: per-mode config screen. ARCADE shows the authentic arcade
         * DIP bank (Coinage/Lives/Bonus/Difficulty/Demo/Flip/...); HOME shows
         * the console-style options subset (Lives/Bonus/Difficulty/Demo Sounds).
         * Both are blocking, run in the front-end H40 text mode, write g_config
         * (game.h), and tear back to a blank screen before the game video mode.
         * The engine reads g_config (lives/difficulty/...) in game_init. */
        save_apply_settings(g_mode);
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

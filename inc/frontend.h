/* frontend.h -- SonSon NATIVE boot-flow front-end (Phase 8).
 *
 * Three blocking presentation stages, ported from the verified sibling port
 * (../sonson/port/src) and ADAPTED to run standalone in the native build --
 * i.e. WITHOUT the old transliteration runtime (no ram_lo videoram mirror, no
 * 6809 globals, no port_present_frame). The two logo splashes reuse the owner's
 * verified kits verbatim (sega_logo.{c,h}, capcom_logo.{c,h}); the selector is
 * a clean native re-creation that keeps the SONSON title look + the exact
 * "PORTED BY RESTER159, 2026" credit string.
 *
 * Boot order (main.c): sega_splash_run() -> capcom_splash_run() ->
 * game_select_run() -> (SONSON) mode = selector_run() -> game_init() + the
 * native game loop.
 *
 * Each stage draws its own tiles/palette, runs to completion, then tears the
 * VDP/sound hardware back to a clean state so the next stage (and finally
 * game_init) starts from a known-blank screen.
 */
#ifndef FRONTEND_H
#define FRONTEND_H

#include <genesis.h>

/* Game-select return values (game_select_run). */
#define GAME_SONSON   0
#define GAME_SONSON2  1

/* Selector return values -> stored in g_mode (game.h). */
#define MODE_ARCADE   0
#define MODE_HOME     1

/* SEGA opening: the authentic Sonic-1 SEGA screen (palette reveal) with the
 * "SEEE-GAAA" chant over it. Blocks ~the original on-screen time, then cuts
 * to black and leaves the VDP + sound hardware pristine. */
void sega_splash_run(void);

/* CAPCOM boot logo: gold/blue CAPCOM logotype, fade-in / glint / fade-out with
 * the register-exact FM jingle (68k drives the YM with the Z80 bus held).
 * Blocks ~6 s, then tears down to black + a reset YM, Z80 returned to SGDK's
 * clean halt state. */
void capcom_splash_run(void);

/* SELECT GAME screen: the SONSON and SON SON II logos side by side (same
 * size), arcade-font "SELECT GAME" header. Blocks until the player picks;
 * returns GAME_SONSON or GAME_SONSON2. */
u8 game_select_run(void);

/* ARCADE / HOME mode selector with the SONSON title + the RESTER credit.
 * Blocks until the player picks a mode; returns MODE_ARCADE or MODE_HOME. */
u8 selector_run(void);

/* Arcade TITLE screen + ATTRACT loop (RE'd from the real machine): the baked
 * "SONSON" title (logo + score/HIGH header + deposit-coin prompt + flanking
 * characters). Runs in the game's H32 video mode. Blocks until START. */
void title_attract_run(u8 mode);
/* Arcade title owns the demo lifetime while its blocking gameplay renderer runs. */
extern u8 arcade_title_demo_owned;
u8 arcade_title_demo_update(u8 game_done);

/* ARCADE branch (Phase 8b): the authentic arcade DIP-switch / service screen.
 * Presents SonSon's real DIP bank (Coinage, Lives, Bonus, Difficulty, Demo
 * Sounds, Flip, ...) as a scrollable toggle list; Up/Down move, Left/Right
 * change the value, Start confirms. Writes g_config (game.h). Blocks until
 * confirmed, then clears the screen for game_init. H40 text mode. */
void dip_screen_run(void);

/* HOME branch (Phase 8b): the console-style options menu (re-created from the
 * sibling port's home.c OPTIONS page: LIVES / BONUS / DIFFICULTY / DEMO SOUNDS
 * + START). Matches the selector's native text style + debounced input. Writes
 * g_config (game.h). Blocks until confirmed, then clears the screen. */
void options_run(void);

#endif /* FRONTEND_H */

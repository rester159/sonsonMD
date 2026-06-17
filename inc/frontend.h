/* frontend.h -- SonSon NATIVE boot-flow front-end (Phase 8).
 *
 * Three blocking presentation stages, ported from the verified sibling port
 * (../sonson/port/src) and ADAPTED to run standalone in the native build --
 * i.e. WITHOUT the old transliteration runtime (no ram_lo videoram mirror, no
 * 6809 globals, no port_present_frame). The two logo splashes reuse the owner's
 * verified kits verbatim (sega_logo.{c,h}, capcom_logo.{c,h}); the selector is
 * a clean native re-creation that keeps the SONSON title look + the exact
 * "PORTED BY RESTER 159, 2026" credit string.
 *
 * Boot order (main.c): sega_splash_run() -> capcom_splash_run() ->
 * mode = selector_run() -> game_init() + the native game loop.
 *
 * Each stage draws its own tiles/palette, runs to completion, then tears the
 * VDP/sound hardware back to a clean state so the next stage (and finally
 * game_init) starts from a known-blank screen.
 */
#ifndef FRONTEND_H
#define FRONTEND_H

#include <genesis.h>

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

/* ARCADE / HOME mode selector with the SONSON title + the RESTER credit.
 * Blocks until the player picks a mode; returns MODE_ARCADE or MODE_HOME. */
u8 selector_run(void);

#endif /* FRONTEND_H */

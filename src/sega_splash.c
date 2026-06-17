/* sega_splash.c -- SEGA opening splash (native boot-flow, Phase 8).
 *
 * The authentic Sonic-1 (REV00) SEGA screen: the white card with the SEGA
 * logotype revealed by the classic left-to-right CRAM shimmer that settles the
 * letters white->blue, with the "SEEE-GAAA!" chant played over it. This wraps
 * the owner's verified sega_logo kit (sega_logo.{c,h}, GENERATED from the ROM --
 * never hand-edit the kit) following its documented "Minimal use" sequence.
 *
 * This is the STANDARD SEGA logo (Sonic-1), per the Phase-8 directive default
 * (the sibling port's shipped sega_splash.c used the Shinobi-logo v7 variant;
 * the native build uses the standard one unless the spec says otherwise, so the
 * Shinobi kit is intentionally NOT carried over).
 *
 * Self-contained: depends only on SGDK + the kit. No old-port engine state.
 *
 * Contract: called once at boot in the default H40 mode, right after VDP_init.
 * Loads the PCM driver for the chant; on return unloads it, resets the YM, and
 * clears both planes so the next stage (capcom_splash_run) starts from black.
 */
#include <genesis.h>
#include "frontend.h"
#include "sega_logo.h"

/* The chant is ~1.7 s @ 16 kHz; the palette reveal is SEGA_PALCYCLE_FRAMES
 * (~75 frames). Hold the lit logo a touch past the reveal so the chant is
 * heard out and the screen reads as the arcade-era SEGA card (~the original's
 * on-screen time). 75 reveal + 105 hold ~= 3.0 s on the lit logo. */
#define SEGA_HOLD_FRAMES   105

void sega_splash_run(void)
{
    u16 i;

    /* Stage dark: zero CRAM (all 64 entries the kit uses) before any drawing,
     * so the tile/driver upload never flashes a default palette. */
    for (i = 0; i < 64; i++) sega_logo_cram[i] = 0x0000;
    PAL_setColors(0, sega_logo_cram, 64, CPU);
    VDP_setBackgroundColor(0);

    /* Tiles -> VRAM + both plane maps (B = letter color cells behind A's card).
     * TILE_USER_INDEX keeps the kit clear of the SGDK font/sprite-engine tiles. */
    sega_logo_draw(TILE_USER_INDEX);

    /* Reveal starts from an all-white card (sega_palcycle_reset fills the CRAM
     * mirror white); push it, then load the PCM driver for the chant. */
    sega_palcycle_reset();
    PAL_setColors(0, sega_logo_cram, 64, CPU);
    SND_PCM_loadDriver(TRUE);

    /* The reveal: the 6-color shimmer band sweeps CRAM 16-63 left->right, then a
     * few settle steps fade the letters white->blue (all in the kit). Push the
     * CRAM mirror after each advancing frame. The chant kicks in on frame 0 so
     * "SEEE-GAAA" lands as the letters appear (matches the arcade timing). */
    SND_PCM_startPlay(sega_chant_pcm, sizeof sega_chant_pcm,
                      SOUND_PCM_RATE_16000, SOUND_PAN_CENTER, FALSE);

    while (sega_palcycle_step())
    {
        VDP_waitVSync();
        PAL_setColors(0, sega_logo_cram, 64, CPU);
    }

    /* Hold the fully-revealed logo while the chant rings out. */
    for (i = 0; i < SEGA_HOLD_FRAMES; i++)
        VDP_waitVSync();

    /* Cut to black + teardown: leave the VDP + sound hardware pristine for the
     * CAPCOM stage (PCM driver unloaded, YM reset, both planes cleared). */
    for (i = 0; i < 64; i++) sega_logo_cram[i] = 0x0000;
    PAL_setColors(0, sega_logo_cram, 64, CPU);
    VDP_waitVSync();
    SND_PCM_stopPlay();
    SND_PCM_unloadDriver();
    YM2612_reset();
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
}

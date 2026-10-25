/* sega_splash.c -- SEGA opening splash (native boot-flow, Phase 8).
 *
 * The Revenge of Shinobi (1989, REV01) SEGA screen -- OWNER-CHOSEN (2026-06-17):
 * black background, the small blue-gradient SEGA logotype + (R) with the classic
 * rolling vertical sheen. Wraps the owner's verified sega_logo_shinobi kit
 * (sega_logo_shinobi.{c,h}, GENERATED from the Shinobi ROM -- never hand-edit the
 * kit; regen via sega_intro_shinobi/gen_shinobi_kit.py). This is the SAME SEGA
 * screen the sibling port ships as "splash v7".
 *
 * The original Shinobi screen is SILENT (verified); per the owner's standing
 * "with the sounds" spec the Sonic-1 "SEEE-GAAA!" chant plays over it. The
 * sega_logo (Sonic-1) kit stays linked ONLY for sega_chant_pcm -- its unused
 * tile/map data is dropped by -fdata-sections + --gc-sections. (Drop the SND_PCM
 * lines for the authentic silent version.)
 *
 * Self-contained: depends only on SGDK + the two kits. No old-port engine state.
 *
 * Contract: called once at boot in the default H40 mode, right after VDP_init.
 * Loads the PCM driver for the chant; on return unloads it, resets the YM, and
 * clears both planes so the next stage (capcom_splash_run) starts from black.
 */
#include <genesis.h>
#include "frontend.h"
#include "sega_logo_shinobi.h"             /* the Shinobi SEGA screen (visual)   */
#include "sega_logo.h"                     /* sega_chant_pcm only (audio)        */

/* START skips the boot splashes (both, from the SEGA one). A press must be
 * seen on 3 consecutive reads, and the first 8 splash frames are
 * ignored: the selectors' guard against the 6-button TH-detect phantom
 * presses. Called once per splash frame (the engine is off: we read pads). */
bool g_splash_skipped;

bool splash_skip_check(void)
{
    static u16 frames;
    static u8 held;
    JOY_update();
    if (frames < 8) { frames++; return FALSE; }
    if ((JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2)) & BUTTON_START)
    {
        if (++held >= 3) g_splash_skipped = TRUE;
    }
    else held = 0;
    return g_splash_skipped;
}

void sega_splash_run(void)
{
    u16 i;

    /* Stage dark: zero palette line 0 before any drawing, so the tile/PCM-driver
     * upload never flashes a default palette. (The Shinobi kit uses CRAM 0..15.) */
    for (i = 0; i < 16; i++) shinobi_logo_cram[i] = 0x0000;
    PAL_setColors(0, shinobi_logo_cram, 16, CPU);
    VDP_setBackgroundColor(0);

    /* Shinobi logo: tiles -> VRAM + the 12x4 logo map on BG_A at tile (14,10).
     * TILE_USER_INDEX keeps the kit clear of the SGDK font/sprite-engine tiles. */
    shinobi_logo_draw(TILE_USER_INDEX);
    SND_PCM_loadDriver(TRUE);

    /* Pop in (the original cuts straight to the lit logo -- no fade): reset the
     * sheen to step 0, then push the lit palette. */
    shinobi_palroll_reset();
    VDP_waitVSync();
    PAL_setColors(0, shinobi_logo_cram, 16, CPU);

    /* "SEEE-GAAA!" chant over the rolling sheen (Shinobi is silent originally;
     * the chant is the owner's standing spec). Kicks in on frame 0. */
    SND_PCM_startPlay(sega_chant_pcm, sizeof sega_chant_pcm,
                      SOUND_PCM_RATE_16000, SOUND_PAN_CENTER, FALSE);

    /* Hold ~3.5 s (the original on-screen time): the sheen rolls down the letters,
     * CRAM 2..10 sliding through the 20-entry brightness wave every 4th frame. */
    for (i = 0; i < SHINOBI_LOGO_HOLD_FRAMES; i++)
    {
        VDP_waitVSync();
        if (shinobi_palroll_step())        /* TRUE every 4th frame -> push CRAM  */
            PAL_setColors(0, shinobi_logo_cram, 16, CPU);
        if (splash_skip_check()) break;    /* START: skip the intro            */
    }

    /* Cut to black + teardown: leave the VDP + sound hardware pristine for the
     * CAPCOM stage (PCM driver unloaded, YM reset, both planes cleared). */
    for (i = 0; i < 16; i++) shinobi_logo_cram[i] = 0x0000;
    PAL_setColors(0, shinobi_logo_cram, 16, CPU);
    VDP_waitVSync();
    SND_PCM_stopPlay();
    SND_PCM_unloadDriver();
    YM2612_reset();
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
}

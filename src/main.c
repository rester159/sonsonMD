#include <genesis.h>

/* ===========================================================================
 *  SON SON  --  NATIVE Sega Genesis re-implementation
 * ===========================================================================
 *  WHY THIS PROJECT EXISTS
 *  The sibling project sonson/port is a faithful 6809->68000 TRANSLITERATION:
 *  it runs the original arcade ROM and, every frame, re-translates the arcade's
 *  screen (arcade videoram/spriteram + an indirect-palette PROM) into Genesis
 *  format. That per-frame translation costs ~7 of the ~55 frame-budget units
 *  (a software palette lookup the arcade did in hardware), capping it at ~87%.
 *
 *  This project removes that tax entirely by being a NATIVE Genesis game:
 *    - arcade graphics are converted to Genesis 4bpp tiles + palettes ONCE,
 *      at build time (the asset pipeline in tools/);
 *    - the game writes the Genesis nametable / sprite table DIRECTLY;
 *    - no per-frame scan, no software palette resolution, no reformatting.
 *  Target: full 60fps (GF ~55, arcade-faithful look + feel).
 *
 *  STATUS: Phase 0 -- project skeleton. Proves the toolchain builds + boots.
 *  See ROADMAP.md for the plan; reuse map of the existing RE is in README.md.
 * =========================================================================== */

int main(bool hardReset)
{
    VDP_setBackgroundColor(0);

    VDP_drawText("SON SON", 16, 8);
    VDP_drawText("NATIVE GENESIS BUILD", 10, 10);
    VDP_drawText("Phase 0: skeleton OK", 10, 13);
    VDP_drawText("build-time gfx conversion,", 7, 15);
    VDP_drawText("direct VRAM, target 60fps", 7, 16);

    while (TRUE)
    {
        SYS_doVBlankProcess();
    }
    return 0;
}

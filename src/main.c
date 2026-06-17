#include <genesis.h>
#include "game.h"

/* ===========================================================================
 *  SON SON  --  NATIVE Sega Genesis re-implementation
 * ===========================================================================
 *  PHASE 3 milestone: the core native game LOOP.
 *
 *  Phase 2 proved the asset pipeline + direct render path by drawing ONE static
 *  captured scene. Phase 3 turns that into a running game: a controllable player
 *  sprite, a scrolling camera that follows the player, and an on-screen FPS
 *  readout -- all in CLEAN NATIVE C (real structs + locals, direct VDP writes),
 *  NOT a transliteration of the arcade 6809. This is the foundation the rest of
 *  the game (entities, collision, scoring, flow, audio) builds on. See
 *  ROADMAP.md Phase 3 and src/game.c.
 *
 *  Loop: read input + simulate (game_update), build the SAT + scroll + FPS
 *  (game_render), then hand the frame to SGDK (SYS_doVBlankProcess) which
 *  flushes the DMA queue and waits for V-blank -- the single 60Hz timebase.
 * =========================================================================== */

int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth256();              /* SonSon is 256x224 (H32) */
    /* Plane A = 64x32: the wide level scrolls through it as a 64-col sliding
     * window (column streaming -- see game.c). 64 is the max VDP plane width. */
    VDP_setPlaneSize(64, 32, TRUE);

    game_init();

    while (TRUE)
    {
        game_update();
        game_render();
        SYS_doVBlankProcess();
    }
    return 0;
}

#include <genesis.h>
#include "ss_gfx.h"

/* ===========================================================================
 *  SON SON  --  NATIVE Sega Genesis re-implementation
 * ===========================================================================
 *  PHASE 2 milestone: render ONE captured arcade gameplay scene NATIVELY.
 *
 *  The tiles + palettes in ss_gfx.c were converted from the arcade ROMs ONCE,
 *  offline (tools/gen_assets.py), and verified byte-identical to the port's
 *  build_char/build_sprite. Here the game writes Genesis VRAM (tiles), the
 *  Plane-A nametable, and the sprite-attribute-table DIRECTLY -- no per-frame
 *  scan, no software palette lookup, no reformatting. That is the entire point:
 *  the same scene the transliteration port draws with a ~7fps tax, drawn here
 *  for free.
 *
 *  A static scene is trivially full-speed; this proves the asset pipeline +
 *  native render path end-to-end before the dynamic engine (Phase 3+).
 * =========================================================================== */

int main(bool hardReset)
{
    VDP_setScreenWidth256();              /* SonSon is 256x224 (H32) */
    VDP_setPlaneSize(32, 32, TRUE);

    /* Two fixed arcade palettes (PAL0 chars, PAL1 sprites), verbatim. */
    PAL_setColors(PAL0 * 16, ss_pal_char,   16, CPU);
    PAL_setColors(PAL1 * 16, ss_pal_sprite, 16, CPU);
    VDP_setBackgroundColor(PAL0 * 16 + 0);

    /* Baked scene tiles -> VRAM at the documented absolute base. */
    VDP_loadTileData(ss_scene_tiles, SS_SCENE_TILE_BASE, SS_SCENE_TILE_COUNT, DMA);

    /* BG: the captured 32x32 arcade tilemap, written straight to Plane A. */
    for (u16 row = 0; row < 32; row++)
        VDP_setTileMapDataRow(BG_A, &ss_scene_nametable[row * 32], row, 0, 32, DMA);

    /* Sprites: build the hardware SAT directly (same layout as the port's
     * present_sprites: [y+0x80 | size<<8|link | attr | x+0x80], 16x16). */
    static u16 sat[SS_SCENE_SPRITE_COUNT * 4];
    u16 *sb = sat;
    for (u16 i = 0; i < SS_SCENE_SPRITE_COUNT; i++)
    {
        const SSSprite *s = &ss_scene_sprites[i];
        sb[0] = (u16)((0x80 + s->y) & 0x3FF);
        sb[1] = (u16)((SPRITE_SIZE(2, 2) << 8) | ((i + 1) & 0xFF));
        sb[2] = (u16)TILE_ATTR_FULL(PAL1, 0, s->flipV, s->flipH, s->tile);
        sb[3] = (u16)((0x80 + s->x) & 0x1FF);
        sb += 4;
    }
    sat[(SS_SCENE_SPRITE_COUNT - 1) * 4 + 1] &= 0xFF00;   /* terminate link chain */
    DMA_queueDma(DMA_VRAM, sat, VDP_getSpriteListAddress(), SS_SCENE_SPRITE_COUNT * 4, 2);
    DMA_flushQueue();

    while (TRUE)
    {
        SYS_doVBlankProcess();
    }
    return 0;
}

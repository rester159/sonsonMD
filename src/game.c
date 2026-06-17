#include <genesis.h>
#include "game.h"
#include "ss_gfx.h"

/* ===========================================================================
 *  SON SON  --  NATIVE engine, Phase 3 skeleton (game loop + player + camera)
 * ===========================================================================
 *  CLEAN NATIVE C. Real structs + locals, world coords in pixels, direct VDP
 *  writes. This is explicitly NOT a transliteration of the arcade 6809 -- there
 *  is no source-CPU RAM array, no per-frame palette/tile re-scan. The Phase-2
 *  baked scene (ss_gfx.c: fixed PAL0/PAL1 palettes, tiles at VRAM base 0, a
 *  32x32 nametable, 24 scene sprites) is consumed read-only; we just drive a
 *  player around it and scroll the camera, at full 60fps.
 *
 *  Phase 3b will replace the placeholder movement with the faithful arcade
 *  run-along-lanes + climb-between-lanes mechanic (see the TODO in game_update).
 * =========================================================================== */

GameState g_game;

/* The player's sprite tile: a recognizable 16x16 actor from the baked scene
 * set (SPRITE_SIZE(2,2) = 4 col-major 8x8 tiles). ss_scene_sprites[0] is the
 * first arcade spriteram slot; we borrow its tile as a stand-in player frame.
 * A single static frame is fine for Phase 3 -- faithful animation is later. */
#define PLAYER_TILE   (ss_scene_sprites[0].tile)

/* FPS-readout text palette. The SGDK system font lives at a high VRAM tile
 * index (TILE_FONT_INDEX ~= 1376), so it never collides with the scene tiles
 * at 0..134. We give the font its own palette (PAL3) so the readable ramp does
 * not fight the arcade art palettes (PAL0 chars / PAL1 sprites). */
#define FPS_PAL       PAL3

/* ---------------------------------------------------------------------------
 *  game_init -- one-time scene + player setup.
 * ------------------------------------------------------------------------- */
void game_init(void)
{
    /* Two fixed arcade palettes (verbatim from the verified port): PAL0 chars,
     * PAL1 sprites. (main.c sets screen width / plane size before calling us.) */
    PAL_setColors(PAL0 * 16, ss_pal_char,   16, CPU);
    PAL_setColors(PAL1 * 16, ss_pal_sprite, 16, CPU);
    VDP_setBackgroundColor(PAL0 * 16 + 0);

    /* FPS-readout font palette: a small dark->white ramp on PAL3 so the text is
     * legible over the scene without disturbing the art palettes. Index 0 is
     * transparent (backdrop shows through); the font draws in index 15. */
    {
        u16 fontPal[16];
        u16 i;
        for (i = 0; i < 16; i++)
        {
            u16 c = (u16)((i * 0xE) / 15);        /* 0..14 per channel ramp   */
            fontPal[i] = (u16)((c << 8) | (c << 4) | c);   /* BBB GGG RRR gray */
        }
        PAL_setColors(FPS_PAL * 16, fontPal, 16, CPU);
        VDP_setTextPalette(FPS_PAL);
    }

    /* Baked scene tiles -> VRAM at the documented absolute base (0). The
     * nametable words + sprite tile indices below are absolute (include base). */
    VDP_loadTileData(ss_scene_tiles, SS_SCENE_TILE_BASE, SS_SCENE_TILE_COUNT, DMA);

    /* Paint the captured 32x32 arcade tilemap straight into Plane A (the BG). */
    {
        u16 row;
        for (row = 0; row < 32; row++)
            VDP_setTileMapDataRow(BG_A, &ss_scene_nametable[row * 32], row, 0, 32, DMA);
    }

    /* Spawn the player at a sensible on-screen start: lower-centre of the
     * screen, a plausible standing position for a SonSon lane. */
    {
        Player *p = &g_game.player;
        p->x         = (LEVEL_W - PLAYER_W) / 2;   /* centred horizontally     */
        p->y         = LEVEL_H - PLAYER_H - 32;    /* near the bottom lane      */
        p->vx        = 0;
        p->vy        = 0;
        p->facing    = FACING_RIGHT;
        p->anim      = 0;
        p->animTimer = PLAYER_ANIM_PERIOD;
        p->tile      = PLAYER_TILE;
    }

    g_game.cameraX = 0;
}

/* ---------------------------------------------------------------------------
 *  game_update -- one frame of input + simulation.
 *
 *  TODO Phase 3b: faithful lane/climb movement from the RE. The arcade player
 *  is the run-along-horizontal-lanes + climb-between-lanes mechanic. The active
 *  player updater is handler $B1F3 ("Player -- active run/jump/fire", class
 *  players/$0708 idx 1) in ../sonson/census_semantic.toml; its behavioural
 *  reference is the port's lifted sub_B1F3 (the prompt's ~sub_A98A pointer is
 *  the neighbourhood -- the player-class handlers cluster at $B1BC..$B286).
 *  Replace the simple px/frame D-pad model below with that lane state machine
 *  (gravity, climb-up/down between lanes, run accel, fire into $0E28).
 * ------------------------------------------------------------------------- */
void game_update(void)
{
    Player *p = &g_game.player;
    u16 pad = JOY_readJoypad(JOY_1);

    /* --- horizontal: D-pad LEFT/RIGHT = run --- */
    p->vx = 0;
    if (pad & BUTTON_LEFT)
    {
        p->vx     = -PLAYER_SPEED_X;
        p->facing = FACING_LEFT;
    }
    else if (pad & BUTTON_RIGHT)
    {
        p->vx     = PLAYER_SPEED_X;
        p->facing = FACING_RIGHT;
    }

    /* --- vertical: D-pad UP/DOWN = move between lanes (placeholder) --- */
    p->vy = 0;
    if (pad & BUTTON_UP)        p->vy = -PLAYER_SPEED_Y;
    else if (pad & BUTTON_DOWN) p->vy =  PLAYER_SPEED_Y;

    /* --- integrate + clamp to level bounds --- */
    p->x += p->vx;
    p->y += p->vy;

    if (p->x < 0)                    p->x = 0;
    if (p->x > LEVEL_W - PLAYER_W)   p->x = LEVEL_W - PLAYER_W;
    if (p->y < 0)                    p->y = 0;
    if (p->y > LEVEL_H - PLAYER_H)   p->y = LEVEL_H - PLAYER_H;

    /* --- walk-cycle anim timer (cosmetic; single frame drawn for now) --- */
    if (p->vx != 0 || p->vy != 0)
    {
        if (--p->animTimer == 0)
        {
            p->animTimer = PLAYER_ANIM_PERIOD;
            p->anim = (u8)((p->anim + 1) & 0x03);
        }
    }
    else
    {
        p->anim = 0;                 /* idle frame                            */
        p->animTimer = PLAYER_ANIM_PERIOD;
    }

    /* --- camera follows the player, kept roughly centred, clamped to level --- */
    {
        s16 target = (s16)(p->x + PLAYER_W / 2 - SCREEN_W / 2);
        s16 maxCam = (s16)(LEVEL_W - SCREEN_W);
        if (maxCam < 0) maxCam = 0;        /* level no wider than screen       */
        if (target < 0)      target = 0;
        if (target > maxCam) target = maxCam;
        g_game.cameraX = target;
    }
}

/* ---------------------------------------------------------------------------
 *  game_render -- direct hardware SAT build + scroll + FPS readout.
 *
 *  Mirrors the verified port's present_sprites direct-SAT layout exactly:
 *    word0 = y + 0x80           (sprite Y, +0x80 SAT bias)
 *    word1 = SPRITE_SIZE(2,2)<<8 | link
 *    word2 = TILE_ATTR_FULL(PAL1, prio, flipV, flipH, tile)
 *    word3 = x + 0x80           (sprite X, +0x80 SAT bias)
 *  We re-emit the static scene sprites (so the scene still populates) and add
 *  the player as the front-most sprite, then DMA the whole SAT in one transfer.
 * ------------------------------------------------------------------------- */
void game_render(void)
{
    /* SAT: player (slot 0) + the 24 baked scene sprites. */
    static u16 sat[(1 + SS_SCENE_SPRITE_COUNT) * 4];
    u16 *sb = sat;
    u16 link = 0;
    u16 i;

    const Player *p = &g_game.player;

    /* The player is drawn in SCREEN space: world position minus the camera.
     * (The BG scrolls via VDP_setHorizontalScroll below; sprites are not
     * auto-scrolled by the VDP, so we offset them by the camera here.) */
    s16 px = (s16)(p->x - g_game.cameraX);
    s16 py = p->y;

    /* --- player sprite (front of the link chain) --- */
    sb[0] = (u16)((0x80 + py) & 0x3FF);
    sb[1] = (u16)((SPRITE_SIZE(2, 2) << 8) | (++link));
    sb[2] = (u16)TILE_ATTR_FULL(PAL1, 0, 0, p->facing, p->tile);
    sb[3] = (u16)((0x80 + px) & 0x1FF);
    sb += 4;

    /* --- re-emit the baked scene sprites so the captured scene still shows.
     * They are static world objects, so they scroll with the camera too. --- */
    for (i = 0; i < SS_SCENE_SPRITE_COUNT; i++)
    {
        const SSSprite *s = &ss_scene_sprites[i];
        s16 sx = (s16)(s->x - g_game.cameraX);
        sb[0] = (u16)((0x80 + s->y) & 0x3FF);
        sb[1] = (u16)((SPRITE_SIZE(2, 2) << 8) | (++link));
        sb[2] = (u16)TILE_ATTR_FULL(PAL1, 0, s->flipV, s->flipH, s->tile);
        sb[3] = (u16)((0x80 + sx) & 0x1FF);
        sb += 4;
    }

    /* Terminate the link chain (last sprite links to 0). */
    sat[(1 + SS_SCENE_SPRITE_COUNT - 1) * 4 + 1] &= 0xFF00;

    /* One DMA of the whole SAT to the hardware sprite list. */
    DMA_queueDma(DMA_VRAM, sat, VDP_getSpriteListAddress(),
                 (1 + SS_SCENE_SPRITE_COUNT) * 4, 2);

    /* BG horizontal scroll: shift Plane A left by the camera. */
    VDP_setHorizontalScroll(BG_A, (s16)(-g_game.cameraX));

    /* --- FPS readout (top-left corner) ---
     * SYS_getFPS() returns serviced frames/sec; ~60 = full speed. Drawn on the
     * BG plane with the PAL3 font palette set in game_init. */
    {
        char buf[8];
        u32 fps = SYS_getFPS();
        if (fps > 99) fps = 99;            /* keep it a tidy 2-digit field     */
        intToStr((s32)fps, buf, 2);
        VDP_drawText("FPS", 1, 1);
        VDP_drawText(buf, 5, 1);
    }
}

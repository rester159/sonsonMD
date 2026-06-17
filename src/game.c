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

    /* Spawn the player standing on the bottom lane (lane 0), pinned at the left
     * edge of the arcade screen band -- the SonSon hero starts low-left and runs
     * right into the scrolling level. */
    {
        Player *p = &g_game.player;
        p->lane      = 0;
        p->y         = LANE_BASE_Y;                /* lane 0 world Y            */
        p->x         = BAND_LEFT;                  /* pinned at the band's left */
        p->xSub      = 0;
        p->vx        = 0;
        p->laneTimer = 0;
        p->climbing  = 0;
        p->facing    = FACING_RIGHT;
        p->anim      = 0;
        p->animTimer = PLAYER_ANIM_PERIOD;
        p->tile      = PLAYER_TILE;
    }

    g_game.cameraX = 0;
}

/* ---------------------------------------------------------------------------
 *  game_update -- one frame of FAITHFUL SonSon locomotion.
 *
 *  Re-expressed cleanly from the verified arcade player code (the oracle):
 *  ../sonson/port/src/{sub_A98A, sub_A9A7, sub_A9C5, sub_AB52, sub_AD10,
 *  sub_AE16, sub_9617}.c. The mechanic and every constant are documented on the
 *  LOCOMOTION CONSTANTS block in game.h. In one sentence: the hero is pinned in
 *  a fixed horizontal screen band and runs by scrolling the world (LEFT/RIGHT),
 *  and steps between 8px-quantised lanes (UP/DOWN) whose field wraps vertically.
 *
 *  NOT yet faithful (scope-limited, TODO): jump / fire / attack (arcade B1F3 ->
 *  B25A spawns a shot into $0E28) and terrain/enemy collision. The lane field
 *  here is free (no per-lane platform gating). The anim-state var is driven
 *  correctly so real walk/climb frames drop in trivially once baked.
 * ------------------------------------------------------------------------- */
void game_update(void)
{
    Player *p = &g_game.player;
    u16 pad = JOY_readJoypad(JOY_1);

    /* ----- HORIZONTAL: run = commanded velocity + the constant forward drift.
     * Arcade ($A9C5/$A9A7): worldX-fraction gets += $008D every frame (perpetual
     * forward creep) plus the input-gated velocity word; the integer worldX is
     * the carry-out. We accumulate in 1/256-px (xSub) and carry into p->x. The
     * forward drift is folded into the rightward run / idle so the hero always
     * eases forward, exactly as in the arcade. ----- */
    p->vx = 0;
    if (pad & BUTTON_RIGHT)
    {
        p->vx     = (s16)(RUN_VEL_SUBPX + RUN_DRIFT_SUBPX);  /* run with the creep */
        p->facing = FACING_RIGHT;
    }
    else if (pad & BUTTON_LEFT)
    {
        p->vx     = (s16)(-RUN_VEL_SUBPX + RUN_DRIFT_SUBPX); /* push back vs creep  */
        p->facing = FACING_LEFT;
    }
    else
    {
        p->vx     = (s16)RUN_DRIFT_SUBPX;                    /* idle: just the creep */
    }

    /* integrate sub-pixel X -> world X (signed carry, like addd/inc -$B,x) */
    {
        s32 acc = (s32)p->xSub + (s32)p->vx;
        p->x   = (s16)(p->x + (acc >> 8));
        p->xSub = (u16)(acc & 0xFF);
    }

    /* ----- VERTICAL: lane stepping with the SonSon vertical WRAP.
     * Arcade Y is the single byte at -$8 (wraps mod 256 => the vertical loop),
     * nudged in 8px sub-steps. We model discrete lanes (LANE_PITCH apart) and
     * wrap the lane INDEX: climbing past the top lane reappears at the bottom,
     * and dropping below the bottom reappears at the top. One step per hold-tick
     * (LANE_STEP_PERIOD) so a held press climbs at a steady cadence. ----- */
    if (p->laneTimer != 0) p->laneTimer--;

    p->climbing = 0;
    if (pad & (BUTTON_UP | BUTTON_DOWN))
    {
        p->climbing = 1;
        if (p->laneTimer == 0)
        {
            if (pad & BUTTON_UP)
                p->lane = (u8)((p->lane + 1) % LANE_COUNT);          /* climb up   */
            else
                p->lane = (u8)((p->lane + LANE_COUNT - 1) % LANE_COUNT); /* down   */
            p->laneTimer = LANE_STEP_PERIOD;
        }
    }
    /* derive world Y from the lane index (lane 0 = bottom, higher = up-screen) */
    p->y = (s16)(LANE_BASE_Y - (s16)p->lane * LANE_PITCH);

    /* ----- CAMERA + horizontal screen-band clamp (arcade sub_AB52).
     * The hero is pinned to the band [BAND_LEFT..BAND_RIGHT] in SCREEN space;
     * the world scrolls to keep him there. We move the camera so screenX stays
     * inside the band, then clamp the camera to the level and re-clamp the
     * player's *world* X to the band edge when the camera can't scroll further
     * (level start / end) -- exactly the arcade's "push worldX back to the
     * clamp" behaviour. ----- */
    {
        s16 maxCam  = (s16)(LEVEL_W - SCREEN_W);
        s16 screenX;
        if (maxCam < 0) maxCam = 0;                 /* level no wider than screen */

        /* scroll the camera to keep the player inside the band */
        screenX = (s16)(p->x - g_game.cameraX);
        if (screenX < BAND_LEFT)        g_game.cameraX = (s16)(p->x - BAND_LEFT);
        else if (screenX > BAND_RIGHT)  g_game.cameraX = (s16)(p->x - BAND_RIGHT);

        if (g_game.cameraX < 0)      g_game.cameraX = 0;
        if (g_game.cameraX > maxCam) g_game.cameraX = maxCam;

        /* with the camera clamped, re-pin the player's world X to the band so he
         * never escapes the visible window at the level edges */
        screenX = (s16)(p->x - g_game.cameraX);
        if (screenX < BAND_LEFT)
        {
            p->x = (s16)(g_game.cameraX + BAND_LEFT);
            p->xSub = 0;
        }
        else if (screenX > BAND_RIGHT)
        {
            p->x = (s16)(g_game.cameraX + BAND_RIGHT);
            p->xSub = 0;
        }
    }

    /* ----- ANIM cadence (arcade sub_9617: per-frame countdown then step).
     * Advance the walk OR climb cycle while moving; hold the idle frame at rest.
     * anim is the cell index a future baked frame-set will key off. ----- */
    if ((pad & (BUTTON_LEFT | BUTTON_RIGHT)) || p->climbing)
    {
        if (--p->animTimer == 0)
        {
            p->animTimer = PLAYER_ANIM_PERIOD;
            p->anim = (u8)((p->anim + 1) & 0x03);   /* 4-frame walk/climb cycle  */
        }
    }
    else
    {
        p->anim = 0;                                /* idle frame                */
        p->animTimer = PLAYER_ANIM_PERIOD;
    }
    /* TODO: bake + swap player anim frames (walk N-frame + climb cycle). The
     * sprite cell is currently the single baked PLAYER_TILE; p->anim/p->climbing
     * already carry the state to select the right frame once tiles exist. */
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

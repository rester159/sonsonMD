#include <genesis.h>
#include "game.h"
#include "ss_gfx.h"
#include "ss_level.h"

/* ===========================================================================
 *  SON SON  --  NATIVE engine, Phase 4 (wide SCROLLING level)
 * ===========================================================================
 *  CLEAN NATIVE C. Real structs + locals, world coords in pixels, direct VDP
 *  writes. NOT a transliteration of the arcade 6809 -- no source-CPU RAM array,
 *  no per-frame palette/tile re-scan.
 *
 *  Phase 4 replaces the single static screen with a WIDE baked level
 *  (ss_level.c: SS_LEVEL_COLS x 32 BG cells, ~5 screens, baked from the arcade's
 *  scrolling level-1). The 64-col Genesis Plane A is driven as a COLUMN-STREAMED
 *  sliding window over that level (ring buffer; see stream_column / bg_stream):
 *  as the player's perpetual forward creep advances the camera, newly-revealed
 *  columns are decoded + DMA'd in at the seam (1 column/frame, tear-free) and
 *  the BG hardware-scrolls -- so the forward run travels through the whole level
 *  at full 60fps. The player keeps its screen-band pin and a baked ss_gfx sprite
 *  tile; the FPS readout stays on the unscrolled Plane B.
 * =========================================================================== */

GameState g_game;

/* Mode chosen at the front-end selector (set by main.c after selector_run).
 * Defaults to MODE_ARCADE (0). The game can read this for mode-specific
 * behaviour (arcade DIP-driven params vs home options) -- wired in Phase 8b. */
u8 g_mode = 0;

/* Game parameters set by the Phase-8b config screens (dipswitch.c / options.c)
 * before game_init. Initialised to SonSon's authentic power-on DIP defaults
 * (3 lives, Normal difficulty, 20K/80K/100K bonus, demo sounds on, no flip) so
 * the engine has sane values even if a screen is skipped. The screens overwrite
 * this; the engine reads g_config.lives / .difficulty / ... as game-flow lands. */
GameConfig g_config =
{
    DSW1_DEFAULT, DSW2_DEFAULT,   /* raw arcade DIP banks (0xDF / 0xCB)         */
    3,            /* lives      = 3                                             */
    1,            /* difficulty = Normal (0=Easy..3=VeryHard)                   */
    1,            /* bonusLife  = idx1 = 20K 80K 100K (DSW2 default)            */
    1,            /* demoSound  = on                                            */
    0             /* flipScreen = off                                           */
};

/* The player's sprite tile: a recognizable 16x16 actor from the baked scene
 * set (SPRITE_SIZE(2,2) = 4 col-major 8x8 tiles). ss_scene_sprites[0] is the
 * first arcade spriteram slot; we borrow its tile as a stand-in player frame.
 * A single static frame is fine for Phase 3 -- faithful animation is later. */
#define PLAYER_TILE   (ss_scene_sprites[0].tile)

/* FPS-readout text palette. The SGDK system font lives at a high VRAM tile
 * index (TILE_FONT_INDEX ~= 1376), so it never collides with the scene tiles
 * at 0..134 or the level tiles at 135..(135+SS_LEVEL_TILE_COUNT). We give the
 * font its own palette (PAL3) so the readable ramp does not fight the arcade
 * art palettes (PAL0 chars / PAL1 sprites). */
#define FPS_PAL       PAL3

/* ---------------------------------------------------------------------------
 *  Column streaming -- write ONE wide-level column into the 64-col Plane A.
 *
 *  The wide level (ss_level_map, ROW-MAJOR, SS_LEVEL_COLS wide) is shown through
 *  the 64-col plane as a ring buffer: level column L maps to plane column
 *  (L & PLANE_COL_MASK). To reveal a column we copy its 32 nametable words
 *  (one per BG row, strided by SS_LEVEL_COLS in the source) into a contiguous
 *  scratch buffer and DMA it down the plane column (VRAM stride = plane width
 *  = 64 words). Off-level columns (L<0 or L>=SS_LEVEL_COLS) are written blank
 *  so the seam never shows stale ring data.
 *
 *  DMA_QUEUE (not immediate): queued transfers are flushed inside V-blank by
 *  SYS_doVBlankProcess, so a column written THIS frame lands before the next
 *  active display -- no tearing at the scroll seam (same discipline the sibling
 *  port uses for its scroll-seam column). One strided DMA per revealed column.
 * ------------------------------------------------------------------------- */
static void stream_column(s16 levelCol, TransferMethod tm)
{
    static u16 colbuf[SS_LEVEL_ROWS];          /* one plane column (32 words)  */
    u16 planeCol = (u16)(levelCol & PLANE_COL_MASK);
    u16 row;

    if (levelCol < 0 || levelCol >= SS_LEVEL_COLS)
    {
        for (row = 0; row < SS_LEVEL_ROWS; row++) colbuf[row] = 0;   /* blank   */
    }
    else
    {
        const u16 *src = &ss_level_map[(u16)levelCol];     /* row 0 of this col */
        for (row = 0; row < SS_LEVEL_ROWS; row++)
            colbuf[row] = src[row * SS_LEVEL_COLS];        /* stride a whole row */
    }

    /* Strided VRAM write: 32 words down one plane column. VDP_setTileMapDataColumnFast
     * (plane, data, column, y, h, tm) takes our pre-arranged contiguous buffer and
     * writes plane column `planeCol`, rows 0..31. The streaming caller uses
     * DMA_QUEUE (flushed in the next V-blank -> tear-free at the scroll seam);
     * the cold-fill uses immediate DMA so the whole first window is resident
     * before the loop starts.  NOTE colbuf is reused per call: with DMA_QUEUE the
     * transfer reads colbuf at flush time, so we issue at most ONE queued column
     * per frame (the camera advances <1 tile/frame), keeping the buffer valid. */
    VDP_setTileMapDataColumnFast(BG_A, colbuf, planeCol, 0, SS_LEVEL_ROWS, tm);
}

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

    /* FPS readout goes on Plane B. The wide scrolling level lives on Plane A;
     * Plane B is otherwise unused and we never scroll it (its H/V scroll stays
     * 0), so text drawn there stays pinned at the top while the level scrolls
     * underneath. Plane B priority/transparency: its blank cells are transparent
     * so the level shows through everywhere except the readout glyphs. */
    VDP_setTextPlane(BG_B);
    VDP_clearTextArea(0, 0, 40, 4);

    /* VRAM tile layout (no overlap, all below the SGDK font/sprite-engine tiles
     * up near ~1376):
     *   scene tiles  [0 .. SS_SCENE_TILE_COUNT)            -- only the player
     *                                                         sprite is used now
     *   level tiles  [SS_LEVEL_TILE_BASE .. +SS_LEVEL_TILE_COUNT)  (base = 135)
     * The scene's static BG is dropped; the WIDE level replaces it. We still load
     * the scene tiles because the player sprite borrows ss_scene_sprites[0].tile. */
    VDP_loadTileData(ss_scene_tiles, SS_SCENE_TILE_BASE, SS_SCENE_TILE_COUNT, DMA);
    VDP_loadTileData(ss_level_tiles, SS_LEVEL_TILE_BASE, SS_LEVEL_TILE_COUNT, DMA);

    /* Cold-fill the 64-col Plane A window with the first 64 level columns
     * (columns 0..63 of the wide level). game_render streams new columns in as
     * the camera advances past each tile boundary. */
    {
        s16 col;
        for (col = 0; col < PLANE_COLS; col++)
            stream_column(col, DMA);           /* immediate: resident before loop */
    }
    g_game.loadedMinCol = 0;
    g_game.loadedMaxCol = PLANE_COLS - 1;

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
 *  bg_stream -- keep the wide level resident in the 64-col Plane A window.
 *
 *  The 64-col plane is a ring buffer over the SS_LEVEL_COLS-wide level: level
 *  column L lives at plane column L & PLANE_COL_MASK. We must keep every column
 *  the visible 256px window can show resident. The window shows level columns
 *  [cameraX/8 .. cameraX/8 + 32]; the 64-wide plane gives 32 cols of slack, so
 *  we extend the loaded range to keep a healthy lead column ahead of the right
 *  edge. As the camera advances past a tile boundary we stream the newly-needed
 *  column(s) -- typically 0 or 1 per frame (the forward creep is < 1 tile/frame),
 *  so streaming touches just the new seam column, never a full repaint -> 60fps.
 *
 *  SEAM SAFETY: a freshly-streamed level column L overwrites plane column
 *  L & 63, which previously held level column L-64 -- already 32+ cols off the
 *  LEFT of the visible window, so the overwrite is never visible. New columns
 *  are queued (DMA_QUEUE) and flushed in V-blank before they scroll into view.
 * ------------------------------------------------------------------------- */
static void bg_stream(void)
{
    /* Lead column = rightmost level column we want resident: the visible right
     * edge (cameraX + SCREEN_W)/8 plus a 1-col lead so it's down before shown. */
    s16 leadCol = (s16)((g_game.cameraX + SCREEN_W) / 8 + 1);
    /* Don't run past the ring's capacity behind the current left edge. */
    s16 firstVisCol = (s16)(g_game.cameraX / 8);

    /* Extend forward: stream any columns between loadedMaxCol+1 .. leadCol.
     * Bounded by PLANE_COLS new columns max (cold safety); steady state = 0/1. */
    while (g_game.loadedMaxCol < leadCol)
    {
        s16 newCol = (s16)(g_game.loadedMaxCol + 1);
        stream_column(newCol, DMA_QUEUE);
        g_game.loadedMaxCol = newCol;
        /* keep the resident span within one plane width (64 cols) */
        if (g_game.loadedMaxCol - g_game.loadedMinCol >= PLANE_COLS)
            g_game.loadedMinCol = (s16)(g_game.loadedMaxCol - PLANE_COLS + 1);
        /* never strand the visible left edge (paranoia; leadCol math keeps it) */
        if (firstVisCol < g_game.loadedMinCol)
            g_game.loadedMinCol = firstVisCol;
    }
}

/* ---------------------------------------------------------------------------
 *  game_render -- column-stream the BG + direct SAT + scroll + FPS readout.
 *
 *  The player SAT uses the verified port's direct-SAT word layout:
 *    word0 = y + 0x80           (sprite Y, +0x80 SAT bias)
 *    word1 = SPRITE_SIZE(2,2)<<8 | link
 *    word2 = TILE_ATTR_FULL(PAL1, prio, flipV, flipH, tile)
 *    word3 = x + 0x80           (sprite X, +0x80 SAT bias)
 *  The wide level replaces the old static scene BG, so we no longer re-emit the
 *  captured scene sprites; we keep the player (its tile borrowed from the baked
 *  ss_gfx sprite set) as the single front sprite.
 * ------------------------------------------------------------------------- */
void game_render(void)
{
    static u16 sat[1 * 4];                      /* just the player for now      */
    const Player *p = &g_game.player;

    /* The player is drawn in SCREEN space: world position minus the camera.
     * (The BG scrolls via VDP_setHorizontalScroll below; sprites are not
     * auto-scrolled by the VDP, so we offset them by the camera here.) */
    s16 px = (s16)(p->x - g_game.cameraX);
    s16 py = p->y;

    /* Stream any newly-revealed level columns into the plane window FIRST, so
     * the queued column DMAs flush in the same V-blank as the scroll update. */
    bg_stream();

    /* --- player sprite (single-entry link chain) --- */
    sat[0] = (u16)((0x80 + py) & 0x3FF);
    sat[1] = (u16)((SPRITE_SIZE(2, 2) << 8) | 0);    /* link 0 = chain end       */
    sat[2] = (u16)TILE_ATTR_FULL(PAL1, 0, 0, p->facing, p->tile);
    sat[3] = (u16)((0x80 + px) & 0x1FF);

    /* One DMA of the SAT to the hardware sprite list. */
    DMA_queueDma(DMA_VRAM, sat, VDP_getSpriteListAddress(), 1 * 4, 2);

    /* BG horizontal scroll: shift Plane A left by the camera. The 64-col plane
     * is 512px wide and the HScroll register wraps mod 512, which matches the
     * ring layout (level col L at plane col L&63) -- so -cameraX aligns the
     * resident columns under the visible window for any cameraX. */
    VDP_setHorizontalScroll(BG_A, (s16)(-g_game.cameraX));

    /* --- FPS readout (top-left corner) ---
     * SYS_getFPS() returns serviced frames/sec; ~60 = full speed. Drawn on the
     * unscrolled Plane B (pinned) with the PAL3 font palette; it shows through
     * the blanked HUD band (level rows 0-4 are transparent on Plane A). */
    {
        char buf[8];
        u32 fps = SYS_getFPS();
        if (fps > 99) fps = 99;            /* keep it a tidy 2-digit field     */
        intToStr((s32)fps, buf, 2);
        VDP_drawText("FPS", 1, 1);
        VDP_drawText(buf, 5, 1);
    }
}

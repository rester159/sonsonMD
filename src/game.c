#include <genesis.h>
#include "game.h"
#include "entity.h"
#include "ss_gfx.h"
#include "ss_level.h"
#include "ss_collision.h" /* per-column 6-tier terrain collision (native sub_A847) */
#include "ss_anim.h"     /* baked player/enemy animation frames + table */
#include "sound.h"       /* faithful arcade BGM + SFX (Z80/V-Int-driven)  */

/* ===========================================================================
 *  SON SON  --  NATIVE engine, Phase 4 (wide SCROLLING level)
 * ===========================================================================
 *  CLEAN NATIVE C. Real structs + locals, world coords in pixels, direct VDP
 *  writes. NOT a transliteration of the arcade 6809 -- no source-CPU RAM array,
 *  no per-frame palette/tile re-scan.
 *
 *  Phase 4 replaced the single static screen with a WIDE baked level; the SonSon
 *  ribbon step concatenates SS_LEVEL_COUNT DISTINCT baked levels (ss_level.c:
 *  ss_levels[], ~5 screens each, baked from REAL arcade gameplay -- md=02, the CLEAN
 *  playfields) into ONE SEAMLESS LOOPING RIBBON (tools/LEVEL_END_SPEC.md). All 8
 *  tilesets are CO-RESIDENT in VRAM (each at its own SsLevel.tileBase, loaded once in
 *  game_init), so the 64-col Genesis Plane A is driven as a COLUMN-STREAMED sliding
 *  window over the whole ribbon (ring buffer; see stream_column / ribbon_locate /
 *  bg_stream): as the player's perpetual forward creep advances the camera, newly-
 *  revealed columns are decoded + DMA'd in at the seam (1 column/frame, tear-free) --
 *  including the level-to-level seams, with NO transition/cold-fill -- and the BG
 *  hardware-scrolls. The ribbon wraps after the last segment back to the first
 *  (cameraX wrap + ribbon_refill). The player keeps its screen-band pin and a baked
 *  ss_gfx sprite tile; the FPS readout stays on the unscrolled Plane B.
 * =========================================================================== */

GameState g_game;

/* SEAMLESS SINGLE-RIBBON model (replaces the per-round discrete-level swap).
 * The 8 baked levels are concatenated into ONE endless LOOPING ribbon of
 * SS_RIBBON_COLS columns (tools/LEVEL_END_SPEC.md: the arcade is one looping
 * ribbon). All 8 tilesets are CO-RESIDENT in VRAM (each at its own SsLevel.tileBase,
 * loaded once in game_init), so the scroll streams continuously across level
 * boundaries with NO tile reload and NO cold-fill pause -- inherently tear-free.
 *
 * g_curLevel  -- the level (ribbon SEGMENT) currently UNDER THE CAMERA (its tiles
 *                are resident alongside all the others). Tracked each frame from
 *                cameraX; read by code that wants "which scenery is on screen now".
 * g_levelW    -- the RIBBON pixel width (SS_RIBBON_COLS*8). Kept under this name so
 *                the existing harnesses (verify_autoscroll.lua etc.) still read a
 *                nonzero "in-game" width here; it stays constant during play now.
 * g_levelIdx  -- index of g_curLevel (the on-camera segment); tier_solid() resolves
 *                collision against whatever segment a given ribbon column lands in,
 *                so the player/enemies keep working across every seam. */
const SsLevel *g_curLevel = &ss_levels[0];
/* volatile so it survives LTO as the documented "in-game" probe word (the headless
 * harnesses gate on g_levelW != 0; see tools/play_check.lua / verify_autoscroll.lua).
 * It is set once per session to the ribbon pixel width and read by no C code now. */
volatile u16   g_levelW   = 0;
static u8      g_levelIdx = 0;

/* ATTRACT demo state (see game.h). */
volatile u8    g_attract = 0;
volatile u8    g_attract_started = 0;

/* Player count for the next game (1 or 2), set by the front-end. SonSon is
 * simultaneous co-op (tools/TWO_PLAYER_SPEC.md); game_init copies this into
 * g_game.numPlayers. Defaults to 1 so a skipped front-end still boots a 1P game. */
volatile u8    g_numPlayers = 1;

/* WORLD SCROLL (arcade global auto-scroll, sub_A697 $A697): the camera advances by
 * a CONSTANT forward creep every frame, INDEPENDENT of the players -- $0091 (frac)
 * += $008D; on carry $0090 (int) += 1. Players are each clamped to the visible
 * screen band [8..208] (sub_AB52), roaming freely between the edges. This replaces
 * the old "camera follows the one band-pinned player" model and is what makes
 * simultaneous co-op work (and is more arcade-faithful for 1P too). The creep rate
 * equals the player's idle forward drift (RUN_DRIFT_SUBPX) so an idle player holds
 * a constant screen X (rides the scroll), exactly as before. */
static u16     g_scrollSub = 0;       /* scroll sub-pixel accumulator ($0091)       */
#define ATTRACT_MAX_FRAMES  2400      /* ~40s demo cap (else ends on demo game-over) */

/* RIBBON GEOMETRY (px). Computed once in game_init from ss_ribbon_col0[]. The
 * ribbon LOOPS at g_ribbonW: cameraX (and the player / entities / spawn cursor)
 * are wrapped by this so a long session never overflows s16 -- the wrap is
 * invisible because column (ribbonCol % SS_RIBBON_COLS) is identical either side. */
static s16     g_ribbonW = 0;       /* SS_RIBBON_COLS * 8 (ribbon pixel width)     */

/* ribbon_locate -- map a (wrapped) ribbon COLUMN to its level index + local column.
 * ribbon col rc in [0..SS_RIBBON_COLS) -> the level i with
 * ss_ribbon_col0[i] <= rc < ss_ribbon_col0[i+1], local col = rc - ss_ribbon_col0[i].
 * Linear scan over 8 segments (cheap, called once per streamed column / collision
 * test). Returns the level index; writes the local column through *localCol. */
static u8 ribbon_locate(u16 rc, u16 *localCol)
{
    u8 i;
    for (i = 0; i < SS_LEVEL_COUNT; i++)
    {
        if (rc < ss_ribbon_col0[i + 1])
        {
            *localCol = (u16)(rc - ss_ribbon_col0[i]);
            return i;
        }
    }
    /* rc >= SS_RIBBON_COLS should never happen (callers wrap first); clamp to the
     * last column of the last level as a safe fallback. */
    *localCol = (u16)(ss_levels[SS_LEVEL_COUNT - 1].cols - 1);
    return (u8)(SS_LEVEL_COUNT - 1);
}

/* Wrap a ribbon column into [0..SS_RIBBON_COLS). Negatives wrap up (the band
 * clamp keeps cameraX>=0, but the lead/streamer can probe a few cols either side). */
static inline u16 ribbon_wrap_col(s16 c)
{
    while (c < 0)                       c = (s16)(c + SS_RIBBON_COLS);
    while (c >= (s16)SS_RIBBON_COLS)    c = (s16)(c - SS_RIBBON_COLS);
    return (u16)c;
}

/* Free-running game-loop tick: incremented once per game_update(). Used by the
 * headless FPS probe (tools/entity_fps.lua) to measure the actual game-frame
 * rate -- if the loop holds 60fps this advances at 60Hz; any slowdown shows as
 * a lower rate (the loop runs less often than the 60Hz V-blank). Harmless in
 * the shipped game (a single counter). */
volatile u32 g_frameCounter = 0;

/* Co-op GAME OVER beat: frames left in the final all-players-out overlay before
 * game_update returns to the front-end. 0 = not started. Set when every active
 * player has reached FLOW_GAME_OVER (in 1P that is just P1). */
static u16 g_gameOverBeat = 0;

/* Autopilot pad for the attract demo: walk forward (RIGHT), fire on a cadence,
 * and weave tiers (Up/Down) so the demo navigates like the arcade's. Driven by
 * g_frameCounter so it's deterministic. */
static u16 attract_pad(void)
{
    u16 pad = BUTTON_RIGHT;
    u32 t = g_frameCounter;
    if ((t % 16) < 3)        pad |= BUTTON_B;     /* steady fire stream     */
    if ((t % 200) < 24)      pad |= BUTTON_UP;    /* climb to a higher tier */
    else if ((t % 200) < 48) pad |= BUTTON_DOWN;  /* drop back down         */
    return pad;
}

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

/* The player now ANIMATES: its sprite tile each frame comes from the baked
 * faithful animation set (ss_anim.c -- run/climb/fire/death), driven by the
 * Anim cursor p->av. PLAYER_TILE is just the cold-start frame (run frame 0). */
#define PLAYER_TILE   (ss_anims[SS_ANIM_PLAYER_RUN].frames[0].tile)

/* ===========================================================================
 *  EXTRA-LIFE (BONUS) thresholds -- arcade DSW2 0x18, RE-sourced (see game.h).
 * ===========================================================================
 *  Per g_config.bonusLife (0..3) a fixed list of score thresholds (decimal points)
 *  at which a bonus life is granted. A 0 entry terminates a list shorter than
 *  BONUS_LIST_MAX (idx2/idx3 award exactly one). Values verified against the DIP
 *  labels (dipswitch.c) + the BCD threshold constants in the arcade ROM
 *  (rom/ss.01e ~$4E60-$4EBC: 20K/30K/80K/90K/100K/120K all present). */
static const u32 k_bonus_thresholds[4][BONUS_LIST_MAX] =
{
    {  30000,  90000, 120000 },   /* idx0 "30K 90K 120K"                          */
    {  20000,  80000, 100000 },   /* idx1 "20K 80K 100K" (default)                */
    {  30000,      0,      0 },   /* idx2 "30K ONLY"                              */
    {  20000,      0,      0 }    /* idx3 "20K ONLY"                              */
};

/* check_extra_life -- award bonus lives for every bonus threshold the score has
 * crossed since the last check (arcade DSW2 0x18). Walks the active threshold list
 * from g_game.bonusNextIdx; each crossed threshold grants one life (capped at
 * LIFE_MAX), advances the index, updates the HUD (lives is re-read each render),
 * and -- if SND_CMD_1UP is defined -- plays a 1-up cue. Called once per frame from
 * game_update while the score can change. Cheap: a couple of u32 compares. */
static void check_extra_life(Player *p)
{
    u8 bl = g_config.bonusLife;
    const u32 *list;
    if (bl > 3) bl = 3;
    list = k_bonus_thresholds[bl];

    while (p->bonusNextIdx < BONUS_LIST_MAX)
    {
        u32 thr = list[p->bonusNextIdx];
        if (thr == 0) break;                       /* list terminator (idx2/idx3)  */
        if (p->score < thr) break;                 /* not reached the next one yet */

        /* crossed this threshold: award a life (capped) + advance */
        if (p->lives < LIFE_MAX) p->lives++;
        p->bonusNextIdx++;
#if defined(SND_CMD_1UP) && defined(SND_1UP_ENABLE)
        /* Play the VERIFIED 1-up cue (cmd 0x04) LAYERED over the BGM. snd_play_1up()
         * drives the jingle on a dedicated channel rendered to the YM2612's second FM
         * bank (ch3-5) -- the spare voices the BGM (PSG) + SFX (YM ch0-2) never use --
         * so the music keeps playing uninterrupted, mirroring the arcade's spare-
         * channel layer. (The old snd_command(SND_CMD_1UP) routed it to the single
         * music slot and REPLACED the BGM; see sound.h.) */
        snd_play_1up();
#endif
    }
}

/* FPS-readout text palette. The SGDK system font lives at a high VRAM tile
 * index (TILE_FONT_INDEX ~= 1376), so it never collides with the scene tiles
 * at 0..134 or the level tiles at 135..(135+SS_LEVEL_TILE_COUNT). We give the
 * font its own palette (PAL3) so the readable ramp does not fight the arcade
 * art palettes (PAL0 chars / PAL1 sprites). */
#define FPS_PAL       PAL3

/* ---------------------------------------------------------------------------
 *  Column streaming -- write ONE RIBBON column into the 64-col Plane A.
 *
 *  The 8 levels form ONE logical ribbon (SS_RIBBON_COLS wide) shown through the
 *  64-col plane as a ring buffer: ribbon column rc maps to plane column
 *  (rc & PLANE_COL_MASK). To reveal a column we wrap rc into [0..SS_RIBBON_COLS),
 *  find which level (ribbon SEGMENT) + local column it lands in (ribbon_locate),
 *  copy that level's 32 nametable words (one per BG row, strided by the level's
 *  width) into a contiguous scratch buffer, and DMA it down the plane column
 *  (VRAM stride = plane width = 64 words). The map words are ABSOLUTE to that
 *  level's OWN VRAM base and ALL 8 tilesets are co-resident (game_init), so every
 *  column references a loaded tile -- no per-seam tile reload, no blank/garbage
 *  column at a level boundary. The ribbon wraps after the last level back to the
 *  first, so there are no off-ribbon columns.
 *
 *  DMA_QUEUE (not immediate): queued transfers are flushed inside V-blank by
 *  SYS_doVBlankProcess, so a column written THIS frame lands before the next
 *  active display -- no tearing at the scroll seam (the same discipline used at
 *  every column boundary, now including the level-to-level seams). One strided
 *  DMA per revealed column. ------------------------------------------------- */
static void stream_column(s16 ribbonCol, TransferMethod tm)
{
    static u16 colbuf[SS_LEVEL_ROWS];          /* one plane column (32 words)  */
    u16 planeCol = (u16)(ribbonCol & PLANE_COL_MASK);
    u16 rc       = ribbon_wrap_col(ribbonCol); /* wrap into [0..SS_RIBBON_COLS) */
    u16 localCol;
    u8  lvl      = ribbon_locate(rc, &localCol);   /* which level + local col   */
    const SsLevel *L = &ss_levels[lvl];
    u16 cols     = L->cols;
    u16 row;

    {
        /* Read that level's map column. The map words are ABSOLUTE to that level's
         * own VRAM base (all 8 tilesets co-resident), so no rebasing is needed --
         * the tile referenced is always loaded. The ribbon never has off-level
         * columns (it wraps), so every plane column shows real scenery. */
        const u16 *src = &L->map[localCol];        /* row 0 of this col          */
        for (row = 0; row < SS_LEVEL_ROWS; row++)
            colbuf[row] = src[row * cols];         /* stride a whole row         */
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
 *  ribbon_track_camera -- update which ribbon SEGMENT (baked level) is under the
 *  camera, for collision + per-segment difficulty/scenery readout.
 *
 *  In the seamless-ribbon model there is no per-round tile swap: all 8 tilesets
 *  are co-resident (loaded once in game_init). This just keeps g_curLevel /
 *  g_levelIdx pointing at the level occupying the camera's LEFT edge, so
 *  tier_solid() and entity.c's per-level difficulty resolve to the on-screen
 *  scenery. Cheap (one ribbon_locate per frame). g_levelW stays the RIBBON width. */
static void ribbon_track_camera(void)
{
    u16 localCol;
    u16 rc  = ribbon_wrap_col((s16)(g_game.cameraX >> 3));   /* camera left col   */
    u8  idx = ribbon_locate(rc, &localCol);
    g_curLevel = &ss_levels[idx];
    g_levelIdx = idx;
}

/* ---------------------------------------------------------------------------
 *  level_restart -- cold-fill the ribbon window + place the player at the start.
 *
 *  Called ONCE from game_init (the seamless ribbon never re-fills mid-session --
 *  the scroll is continuous). Cold-fill the 64-col Plane A window with the first
 *  64 RIBBON columns (immediate DMA so it's resident before the loop streams the
 *  rest in), reset the column-streaming range, park the camera at the ribbon
 *  start, and spawn the player low-left pinned at the band -- the SonSon hero's
 *  start pose. `keepPose` is retained for signature compatibility (always FALSE
 *  now). Does NOT touch score / lives / round / flow -- the caller's policy. */
static void level_restart(bool keepPose)
{
    ribbon_track_camera();                 /* set g_curLevel/idx for the start col */

    /* Cold-fill the 64-col Plane A window with the first 64 RIBBON columns
     * (columns 0..63 of the concatenated ribbon). game_render streams new columns
     * in as the camera advances past each tile boundary. */
    {
        s16 col;
        for (col = 0; col < PLANE_COLS; col++)
            stream_column(col, DMA);           /* immediate: resident before loop */
    }
    g_game.loadedMinCol = 0;
    g_game.loadedMaxCol = PLANE_COLS - 1;

    {
        Player *p = &g_game.players[0];
        /* The hero is always band-pinned, so re-pin his world X to the band's left
         * edge as the scroll resets to the new level's start (cameraX = 0 below). */
        p->x    = BAND_LEFT;
        p->xSub = 0;
        p->vx   = 0;
        if (!keepPose)
        {
            /* FRESH game (game_init): full cold-start pose. The arcade hero SPAWNS
             * ON THE MIDDLE TIER (measured heroY ($0488) = 135 at game start,
             * arc_align3.lua) = tier index 2 (SS_TIER_Y[2]=136), facing right, on
             * the platform (no float), in the run cycle. */
            p->tier      = PLAYER_START_TIER;
            p->vstate    = PV_RUN;
            p->y         = SS_TIER_Y[PLAYER_START_TIER];   /* seated on the tier   */
            p->ySub      = 0;
            p->vy        = 0;
            p->climbing  = 0;
            p->facing    = FACING_RIGHT;
            p->which     = 0;                           /* P1 = SonSon (bank 0)    */
            p->anim      = 0;
            p->animTimer = PLAYER_ANIM_PERIOD;
            p->firePrev  = 0;
            p->fireAnim  = 0;
            anim_restart(&p->av, SS_ANIM_PLAYER_RUN);   /* start in the run cycle  */
            p->tile      = PLAYER_TILE;
        }
        /* keepPose (round_advance): the hero KEEPS his tier / facing / anim across
         * the scenery swap -- the arcade's continuous scroll never re-poses him at a
         * segment boundary, so the climb carries over to the next level seamlessly.
         * Re-seat his Y on his current tier in case the new level differs. */
        else
        {
            if (p->tier >= SS_COLLISION_TIERS) p->tier = PLAYER_START_TIER;
            p->vstate = PV_RUN;
            p->y      = SS_TIER_Y[p->tier];
            p->ySub   = 0; p->vy = 0;
        }
    }

    /* P2 (TonTon) cold-start, co-op only: spawn alongside P1 on the start tier,
     * offset right so they don't stack (the overlap resolve keeps them apart once
     * moving). PLACEHOLDER visual -- reuses P1's baked tiles for now; Phase 2b bakes
     * TonTon's own tiles ($AFD0 set, color bank 1). Only on a fresh game (keepPose
     * FALSE = the sole caller); the scroll/camera reset below applies to both. */
    if (g_game.numPlayers > 1)
    {
        Player *q = &g_game.players[1];
        q->x        = (s16)(BAND_LEFT + 24);
        q->xSub     = 0;
        q->vx       = 0;
        q->tier     = PLAYER_START_TIER;
        q->vstate   = PV_RUN;
        q->y        = SS_TIER_Y[PLAYER_START_TIER];
        q->ySub     = 0;
        q->vy       = 0;
        q->climbing = 0;
        q->facing   = FACING_RIGHT;
        q->which    = 1;                                /* P2 = TonTon (bank 1)    */
        q->anim     = 0;
        q->animTimer= PLAYER_ANIM_PERIOD;
        q->firePrev = 0;
        q->fireAnim = 0;
        anim_restart(&q->av, SS_ANIM_PLAYER2_RUN);
        q->tile     = ss_anims[SS_ANIM_PLAYER2_RUN].frames[0].tile;
    }

    g_game.cameraX  = 0;
}

/* ---------------------------------------------------------------------------
 *  ribbon_refill -- rebuild the full 64-col Plane A window at the current camera.
 *
 *  Called once per full ribbon loop, the frame game_update wraps cameraX by a whole
 *  ribbon. The wrap shifts the 64-col ring phase (a full ribbon is not a multiple of
 *  64 cols), so we re-stream every plane column from the post-wrap coordinates and
 *  reset the resident range. Tear-free because the pre- and post-wrap scenery are
 *  identical (the wrap is exactly one ribbon) -- each plane cell is rewritten with
 *  the same tile it already held. Streams the window covering the camera plus the
 *  one-column lead, matching bg_stream's resident span. */
static void ribbon_refill(void)
{
    s16 firstCol = (s16)(g_game.cameraX >> 3);          /* visible left column     */
    s16 col;
    for (col = firstCol; col < (s16)(firstCol + PLANE_COLS); col++)
        stream_column(col, DMA);                        /* immediate: this frame   */
    g_game.loadedMinCol = firstCol;
    g_game.loadedMaxCol = (s16)(firstCol + PLANE_COLS - 1);
}

/* ---------------------------------------------------------------------------
 *  world_scroll_tick -- advance the GLOBAL auto-scroll one frame (arcade sub_A697).
 *
 *  The arcade scroll is a CONSTANT forward creep, independent of the players:
 *  $0091 (frac) += $008D each frame; on carry $0090 (int) += 1 (sub_A697 $A697).
 *  We accumulate RUN_DRIFT_SUBPX (= $008D) in g_scrollSub and carry whole pixels
 *  into cameraX. Called once per frame from game_update (BEFORE the per-player band
 *  clamp). The ribbon wrap + segment tracking in game_update consume cameraX as
 *  before. The creep equals the player's idle drift, so an idle player holds a
 *  constant screen X. The left clamp only matters on the first frames (creep is
 *  positive, so cameraX only grows thereafter). ------------------------------- */
static void world_scroll_tick(void)
{
    u32 acc = (u32)g_scrollSub + (u32)RUN_DRIFT_SUBPX;
    g_game.cameraX = (s16)(g_game.cameraX + (s16)(acc >> 8));
    g_scrollSub    = (u16)(acc & 0xFF);
    if (g_game.cameraX < 0) g_game.cameraX = 0;       /* ribbon-start left clamp */
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

    /* VRAM tile layout for the SEAMLESS RIBBON (no overlap, all below the SGDK
     * font/sprite-engine tiles up near ~1376):
     *   scene tiles  [0 .. SS_SCENE_TILE_COUNT)            -- only the player
     *                                                         sprite is used now
     *   anim tiles   [SS_ANIM_TILE_BASE .. +SS_ANIM_TILE_COUNT)   (base = 346)
     *   level tiles  [SS_LEVEL_TILE_BASE .. SS_LEVEL_TILE_TOP)    (base = 562)
     *                  -- ALL 8 levels CO-RESIDENT, each at its own SsLevel.tileBase
     *                     (~645 tiles total, 562..1206). Loaded ONCE here; the ribbon
     *                     streams continuously across seams with no reload.
     * The scene's static BG is dropped; the wide RIBBON replaces it. We still load
     * the scene tiles because the player sprite borrows ss_scene_sprites[0].tile. */
    VDP_loadTileData(ss_scene_tiles, SS_SCENE_TILE_BASE, SS_SCENE_TILE_COUNT, DMA);
    /* Faithful player + enemy animation frames at SS_ANIM_TILE_BASE (346); the
     * baked anim table's per-frame tile indices are ABSOLUTE (include the base). */
    VDP_loadTileData(ss_anim_tiles, SS_ANIM_TILE_BASE, SS_ANIM_TILE_COUNT, DMA);
    /* ALL 8 ribbon segments' tilesets, co-resident -- each at its OWN VRAM base so
     * its map words (absolute) reference loaded tiles for the whole session. This
     * is what makes the level-to-level seams tear-free with no runtime DMA. */
    {
        u8 i;
        for (i = 0; i < SS_LEVEL_COUNT; i++)
            VDP_loadTileData(ss_levels[i].tiles, ss_levels[i].tileBase,
                             ss_levels[i].tileCount, DMA);
    }

    /* RIBBON geometry: one endless looping ribbon of SS_RIBBON_COLS columns. The
     * ribbon LOOPS at g_ribbonW; g_levelW carries the ribbon width (kept under this
     * name so the in-game harness probes still read a nonzero width here). */
    g_ribbonW = (s16)(SS_RIBBON_COLS * 8);
    g_levelW  = (u16)g_ribbonW;

    /* Round progression: a NEW game always starts at round 1 (the HUD "RND" + the
     * scenery cycle base) with the difficulty stage at 0. The round now tracks the
     * ribbon segment under the camera (advanced seamlessly as you travel). */
    g_game.round = 1;
    g_game.stage = 0;                 /* difficulty counter ($005A) starts at 0     */

    /* Player count for this game (1 or 2), from the front-end. Clamp to 1..MAX_PLAYERS.
     * SonSon is simultaneous co-op; numPlayers>1 enables P2 (TonTon). The attract
     * demo is always single-player (the arcade attract plays SonSon solo). */
    g_game.numPlayers = (u8)((g_numPlayers >= 2 && !g_attract) ? 2 : 1);
    g_scrollSub = 0;                  /* reset the global auto-scroll accumulator    */

    /* Build the ribbon window + place the player at the start (level_restart cold-
     * fills Plane A from the first 64 ribbon columns, resets the stream range +
     * camera, and spawns the hero low-left). FALSE = full cold-start pose. */
    level_restart(FALSE);

    /* PER-PLAYER game state (co-op): each hero starts with its own score=0, the
     * configured lives (arcade DSW2 0x03 decoded by sync_from_dsw -> 3/4/5/7, the
     * STARTING count incl. the current life), no bonus thresholds reached, ALIVE
     * and clear of any death/hurt state. level_restart set up the run-state above. */
    {
        u8 i;
        for (i = 0; i < MAX_PLAYERS; i++)
        {
            Player *pp = &g_game.players[i];
            pp->score        = 0;
            pp->lives        = g_config.lives;
            pp->bonusNextIdx = 0;
            pp->flow         = FLOW_ALIVE;
            pp->flowTimer    = 0;
            pp->deadReq      = 0;
            pp->invuln       = 0;
            pp->hurt         = 0;
        }
    }
    g_gameOverBeat = 0;               /* final all-out overlay countdown (game_update)*/

    /* Spawn the Phase-4 entity wave (ground enemies + a couple of fruit). */
    entity_init();

    /* AUDIO: bring the sound subsystem up HERE -- after the CAPCOM splash (which
     * drives + resets the YM directly and returns the Z80 to SGDK's halt) and the
     * front-end, so snd_init re-parks the Z80 with the bus held + re-installs the
     * V-Blank replay tick.  Then start the main in-game BGM (arcade cmd 0x18).
     * The replay is advanced from the HW V-Blank (snd.c), so the music keeps
     * tempo no matter how busy the scene gets -- decoupled from this game loop. */
    snd_init();
    snd_play_bgm();
    /* Round-intro music cue (arcade plays 0x16 then 0x10 at a round start). This
     * is the "round 1 begins" flourish layered over the main BGM start; both are
     * verified command numbers. (Routed to the music slot, so it does not disturb
     * the SFX channel.) Each ribbon segment seam re-arms this via segment_advance(). */
    snd_play_round();
}

/* ---------------------------------------------------------------------------
 *  player_respawn -- re-place the player at his start position (arcade B1C2).
 *
 *  The arcade respawn (sub_B1C2) re-inits the player record relative to the
 *  current scroll (worldX = scroll + $F8, Y parked at $F8) with a fresh grace
 *  timer (+$D = $20). We re-express that as: put the hero back on his start lane
 *  pinned at the screen band's left (the native equivalent of "scroll-relative
 *  re-placement" -- the camera is wherever the death left it), reset his motion,
 *  and hand control back. The i-frame window is owned by the flow (RESPAWNING).
 * ------------------------------------------------------------------------- */
static void player_respawn(Player *p)
{
    p->tier      = PLAYER_START_TIER;               /* back on the middle tier   */
    p->vstate    = PV_RUN;
    p->y         = SS_TIER_Y[PLAYER_START_TIER];    /* seated on the tier        */
    p->ySub      = 0;
    p->vy        = 0;
    /* RESPAWN at the RIGHT band edge -- arcade sub_B1C2 ($B1CD) sets the hero's
     * worldX = scroll($90) + $F8, i.e. screenX 248 which the band-clamp pulls to
     * BAND_RIGHT (208): the hero re-enters at the FRONT (right), where he was
     * pushing, not restarting from the back/left. (Arcade respawn Y = b<<2 is
     * data-dependent/murky; the mid-tier default above is a faithful-enough Y.) */
    p->x         = (s16)(g_game.cameraX + BAND_RIGHT);
    p->xSub      = 0;
    p->vx        = 0;
    p->climbing  = 0;
    p->facing    = FACING_RIGHT;
    p->anim      = 0;
    p->animTimer = PLAYER_ANIM_PERIOD;
    p->firePrev  = 0;
    p->fireAnim  = 0;
    anim_restart(&p->av, p->which ? SS_ANIM_PLAYER2_RUN
                                  : SS_ANIM_PLAYER_RUN);   /* respawn in run cycle */
}

/* ---------------------------------------------------------------------------
 *  segment_advance -- cross a ribbon SEGMENT (level) boundary, SEAMLESSLY.
 *
 *  The arcade is one endless LOOPING auto-scroll ribbon (tools/LEVEL_END_SPEC.md):
 *  there is NO finish line, NO cold-fill, NO transition screen -- the scroll just
 *  continues and a saturating stage counter ($005A) ramps difficulty as you travel.
 *  The native ribbon reproduces that exactly: all 8 sceneries are concatenated and
 *  streamed continuously, so when the camera crosses from one segment into the next
 *  there is nothing to rebuild -- the scenery already flowed in column-by-column.
 *
 *  This is called (from game_update) the frame the camera's segment index changes.
 *  It does the per-boundary BOOKKEEPING ONLY -- NO plane rebuild, NO camera reset,
 *  NO pose change, NO entity flush:
 *    - HUD "round" tracks the segment now under the camera (1-based, cycles 1..8);
 *    - the saturating difficulty stage ticks up (arcade $005A, cap STAGE_MAX);
 *    - the wave scheduler re-arms for the new segment's per-level program +
 *      difficulty group (entity_schedule_init -- it does NOT clear live enemies, so
 *      the marching wave carries across the seam too);
 *    - the boundary music cue fires (arcade fires a sound command at segment
 *      boundaries: 0x16 then 0x10). */
static void segment_advance(u8 newSegIdx)
{
    g_game.round = (u8)(newSegIdx + 1);           /* 1-based scenery # for the HUD  */
    if (g_game.stage < STAGE_MAX) g_game.stage++; /* saturating difficulty ($005A)  */

    entity_schedule_init();                       /* re-arm waves for the new seg   */

    /* Boundary music cue (arcade segment-boundary flourish: 0x16 then 0x10). */
    snd_play_round();
}

/* ===========================================================================
 *  TERRAIN COLLISION -- the native sub_A847 ($A847), RIBBON-AWARE.
 * ===========================================================================
 *  The arcade A847 maps the hero's worldX (-$B) + scroll ($90) to a 32-col ring
 *  column, then tests the per-column 4-byte solidity mask at RAM $0100 with a row
 *  bit (Y>>3). We re-express that on the per-column collision tables
 *  (ss_collision[segment], one 6-bit mask/column; bit t = tier t solid). In the
 *  SEAMLESS-RIBBON model a world column belongs to whichever SEGMENT (baked level)
 *  occupies that part of the ribbon, so we map worldX -> ribbon column -> (segment,
 *  local col) and index that segment's collision table -- collision tracks the
 *  scenery on screen across every seam, exactly like the BG streamer does. */

/* Is tier `tier` (0=bottom..5=top) SOLID at world X `worldX`? worldX is wrapped
 * into the ribbon (the ribbon loops, so there are no off-world columns). */
static u8 tier_solid(s16 worldX, u8 tier)
{
    u16 localCol;
    u16 rc;
    u8  seg;
    if (tier >= SS_COLLISION_TIERS) return 0;
    rc  = ribbon_wrap_col((s16)(worldX >> 3));  /* world col = px/8, wrapped       */
    seg = ribbon_locate(rc, &localCol);         /* which segment + local column    */
    /* localCol is always in range (ribbon_locate guarantees it); index that
     * segment's per-column collision mask. */
    return (u8)((ss_collision[seg][localCol] >> tier) & 1);
}

/* The hero's collision column uses his sprite mid-X (+8), like A847 (worldX+8). */
static u8 tier_solid_at_hero(const Player *p, u8 tier)
{
    return tier_solid((s16)(p->x + 8), tier);
}

/* ---------------------------------------------------------------------------
 *  player_vertical -- the faithful vertical state machine (native sub_A847 +
 *  the run / climb-up / climb-down / fall states, $A9F7 handlers 1/2/3/5).
 *
 *  RE map (cited, ../sonson/port/src):
 *    state 1 RUN  (sub_AA32 @ $AA32): on a tier. UP ($10, sub_AACF/AAD2 @ $AAD4
 *      -> state $0200) climbs up; DOWN ($20, sub_AAD2 @ $AAE4, gated cmpb #$28 @
 *      $AAEA -> state $0300) climbs down; if the column under the hero is a GAP
 *      (A847 carry-clear -> sub_AAF4 @ $AAF7 -> state $0300/$0500) he falls.
 *    state 2 CLIMB_UP (sub_AB82/AB97 @ $AB97: Y += climb vel; on reaching the
 *      tier -> back to run).  state 3 CLIMB_DOWN (sub_AC38/AC79). state 5 FALL
 *      (sub_ACDC -> sub_ACEA: Y += until it lands -> seat on the tier below).
 *  Direction bits confirmed by reading raw $40 (=~hw_read $3002) on the arcade:
 *    UP=$10  DOWN=$20  LEFT=$04  RIGHT=$08  FIRE=$01 (arc_bits.lua).
 *
 *  Native re-expression: continuous Y between the 6 SS_TIER_Y tiers (smooth climb
 *  at CLIMB_SPEED_SUBPX; the arcade climb is anim-coupled + near-instant on a press
 *  edge -- we make it readable). The hero is SEATED on his tier each RUN frame
 *  (the A847 push-out); a column gap drops him to the tier below (FALL); UP/DOWN
 *  move him a tier when one exists in that direction at his column. p->tier is the
 *  tier he is on (RUN/FALL target) or heading to (CLIMB). ------------------------ */
static void player_vertical(Player *p, u16 pad)
{
    switch ((PlayerVState)p->vstate)
    {
        case PV_RUN:
        {
            p->climbing = 0;
            p->vy = 0;
            /* Seat on the current tier (the A847 reseat: snap Y to the tier's
             * standing Y). p->tier is the tier the hero stands on. */
            p->y    = SS_TIER_Y[p->tier];
            p->ySub = 0;

            /* GAP under the hero's column? -> FALL (arcade AAF4 carry-clear path).
             * Only when no UP/DOWN climb is being commanded into a neighbour. */
            if (!tier_solid_at_hero(p, p->tier))
            {
                /* fall toward the next SOLID tier below; if none, the bottom */
                p->vstate   = PV_FALL;
                p->vy       = 0;
                p->climbing = 1;
                break;
            }

            /* UP ($10): climb to the next tier up if there IS one above (tier<5).
             * (Arcade gates climb on the structure; off the top there is none.) */
            if ((pad & BUTTON_UP) && p->tier < (SS_COLLISION_TIERS - 1))
            {
                p->tier    = (u8)(p->tier + 1);   /* target = tier above        */
                p->vstate  = PV_CLIMB_UP;
                p->vy      = -(s16)CLIMB_SPEED_SUBPX;
                p->climbing = 1;
            }
            /* DOWN ($20): descend to the next tier down if one exists (tier>0)
             * (arcade $AAEA cmpb #$28 / bls skip -> only when not at the top). */
            else if ((pad & BUTTON_DOWN) && p->tier > 0)
            {
                p->tier    = (u8)(p->tier - 1);   /* target = tier below        */
                p->vstate  = PV_CLIMB_DOWN;
                p->vy      = (s16)CLIMB_SPEED_SUBPX;
                p->climbing = 1;
            }
            break;
        }

        case PV_CLIMB_UP:
        {
            /* move Y up toward SS_TIER_Y[tier] (smaller Y = up-screen) */
            s16 target = SS_TIER_Y[p->tier];
            s32 acc = (s32)p->ySub + (s32)p->vy;
            p->y    = (s16)(p->y + (acc >> 8));
            p->ySub = (u16)(acc & 0xFF);
            p->climbing = 1;
            if (p->y <= target)                   /* arrived at the upper tier  */
            {
                p->y = target; p->ySub = 0; p->vy = 0;
                p->vstate = PV_RUN;
            }
            break;
        }

        case PV_CLIMB_DOWN:
        {
            s16 target = SS_TIER_Y[p->tier];
            s32 acc = (s32)p->ySub + (s32)p->vy;
            p->y    = (s16)(p->y + (acc >> 8));
            p->ySub = (u16)(acc & 0xFF);
            p->climbing = 1;
            if (p->y >= target)                   /* arrived at the lower tier  */
            {
                p->y = target; p->ySub = 0; p->vy = 0;
                p->vstate = PV_RUN;
            }
            break;
        }

        case PV_FALL:
        default:
        {
            /* accelerate downward (arcade fall state); LAND when the hero CROSSES a
             * solid tier's surface this frame (was above it, now at/below it) --
             * a crossing test so a fast fall can't skip past a tier between frames. */
            s16 prevY = p->y;
            s32 acc;
            p->climbing = 1;
            p->vy = (s16)(p->vy + FALL_GRAVITY_SUBPX);
            if (p->vy > (s16)FALL_MAX_SUBPX) p->vy = (s16)FALL_MAX_SUBPX;
            acc = (s32)p->ySub + (s32)p->vy;
            p->y    = (s16)(p->y + (acc >> 8));
            p->ySub = (u16)(acc & 0xFF);

            {
                u8  t;
                s16 landY = 0x7FFF;
                u8  landT = 0xFF;
                /* highest solid tier whose surface lies in (prevY .. newY] at the
                 * hero's column = the one he just fell onto. */
                for (t = 0; t < SS_COLLISION_TIERS; t++)
                {
                    if (!tier_solid_at_hero(p, t)) continue;
                    if (SS_TIER_Y[t] > prevY && SS_TIER_Y[t] <= p->y &&
                        SS_TIER_Y[t] < landY)
                    {
                        landY = SS_TIER_Y[t]; landT = t;
                    }
                }
                if (landT != 0xFF)
                {
                    p->tier = landT; p->y = landY; p->ySub = 0; p->vy = 0;
                    p->vstate = PV_RUN;
                }
                else if (p->y > SS_TIER_Y[0])   /* fell past the bottom -> wrap   */
                {
                    /* reappear ABOVE the top tier (the arcade Y byte wraps mod 256
                     * -- the vertical loop) and keep falling so the crossing test
                     * can catch the top tiers on the way back down. */
                    p->y = (s16)(SS_TIER_Y[SS_COLLISION_TIERS - 1] - 24);
                    p->ySub = 0;
                }
            }
            break;
        }
    }
}

/* ---------------------------------------------------------------------------
 *  player_control -- one frame of FAITHFUL SonSon locomotion (player only).
 *
 *  Re-expressed cleanly from the verified arcade player code (the oracle):
 *  ../sonson/port/src/{sub_A98A, sub_A9A7, sub_A9C5, sub_AB52, sub_AD10,
 *  sub_A847, sub_AA32, sub_AAF4, sub_9617}.c. The mechanic and every constant are
 *  documented on the VERTICAL MODEL + LOCOMOTION CONSTANTS blocks in game.h. In
 *  one sentence: the hero is pinned in a fixed horizontal screen band and runs by
 *  scrolling the world (LEFT/RIGHT), and stands on / climbs between / falls through
 *  the 6 platform TIERS (UP/DOWN) per the per-column terrain collision.
 *
 *  `pad` is the joypad for this frame (0 = no input, so the player coasts) --
 *  game_update passes a forced-zero pad while the player is DYING / in GAME OVER
 *  so the hero stops responding during those beats, exactly like the arcade
 *  where the death handler removes him from the input-driven active state. The
 *  scrolling level + entities keep running underneath regardless.
 * ------------------------------------------------------------------------- */
static void player_control(Player *p, u16 pad)
{
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
    else if (pad & (BUTTON_UP | BUTTON_DOWN))
    {
        /* CLIMBING: the arcade PAUSES the forward creep while a vertical direction
         * is held (sub_A9C5 @ $A9D3: andb #$3C gate suppresses the $008D add when
         * any up/down/left/right bit is set). Measured: heroX stays put while
         * climbing (arc_dir.lua). So no drift this frame. */
        p->vx = 0;
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

    /* ----- VERTICAL: faithful tier collision (the native sub_A847 + the run/
     * climb/fall states). The hero stands ON a platform tier, climbs UP/DOWN
     * between tiers, and falls through a column GAP onto the tier below -- driven
     * by the per-column collision (ss_collision[]) instead of a free lane index.
     * player_vertical() owns the state machine + Y; see it below. ----- */
    player_vertical(p, pad);

    /* ----- horizontal screen-band clamp (arcade sub_AB52, $AB52).
     * The camera AUTO-SCROLLS globally at the constant forward creep (world_scroll_tick,
     * called once per frame in game_update -- the arcade's player-independent $008D
     * scroll, sub_A697); it does NOT follow any one player. Each player just keeps his
     * screenX inside the band [BAND_LEFT..BAND_RIGHT]=[8..208]: he roams freely between
     * the edges (run L/R moves him across the screen) and is CARRIED by the scroll at an
     * edge. An idle player drifts forward by RUN_DRIFT_SUBPX -- exactly the camera creep
     * -- so he holds a constant screenX (rides the scroll), as before. This per-player
     * clamp (no camera move) is what lets BOTH co-op players share one screen. ----- */
    {
        s16 screenX = (s16)(p->x - g_game.cameraX);
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

    /* ----- FIRE: spawn a player shot on the press edge (arcade B1F3 -> B25A).
     * SonSon fires with B/C. The arcade gates fire on a "fire-once" flag so a
     * held button emits a single shot per press; we reproduce that with a
     * press-edge debounce (firePrev). The shot leaves from the player's
     * mid-body in the current facing direction (entity_player_fire). ----- */
    {
        u8 fire = (u8)((pad & (BUTTON_B | BUTTON_C)) != 0);
        if (fire && !p->firePrev)
        {
            /* mid-body launch point (sprite is 16x16). The arcade allows only ONE
             * player shot on screen at a time ($0E28 single slot, sub_B25A), so
             * entity_player_fire returns FALSE if a shot is still live -- a press
             * while one is in flight does nothing: no shot, no SFX, no recoil
             * (faithful, and it kills the old fire-spam slowdown). */
            if (entity_player_fire(p->which, (s16)(p->x + 4), (s16)(p->y + 4),
                                   p->facing))
            {
                /* arcade player-shot SFX (verified cmd 0x0B), YM2612 SFX voice. */
                snd_sfx_fire();
                /* show the fire/recoil pose for the anim's full length (per-player
                 * anim set: TonTon uses SS_ANIM_PLAYER2_FIRE). */
                {
                    u8 af = p->which ? SS_ANIM_PLAYER2_FIRE : SS_ANIM_PLAYER_FIRE;
                    p->fireAnim = (u8)(ss_anims[af].frames[0].dur +
                                       ss_anims[af].frames[1].dur);
                }
            }
        }
        p->firePrev = fire;
    }
}

/* ---------------------------------------------------------------------------
 *  player_animate -- pick the player's animation from its state and advance it.
 *
 *  Selection priority (matches the arcade's anim-state precedence): DEATH while
 *  the flow is DYING (the death beat) > FIRE for the brief recoil window after a
 *  shot > CLIMB while a lane change is in progress > RUN otherwise. The chosen
 *  anim is ticked by its baked per-frame durations (the captured arcade cadence)
 *  and the resulting absolute VRAM tile is stored in p->tile, which game_render
 *  feeds straight into the SAT. Facing still drives flipH at render time.
 *
 *  `dying` is TRUE only on the FLOW_DYING beat (passed from game_update after the
 *  flow logic), so the death tumble plays while the hero is disabled + blinking.
 * ------------------------------------------------------------------------- */
static void player_animate(Player *p, u8 dying)
{
    /* Pick the per-player anim set: P1 (SonSon) uses SS_ANIM_PLAYER_*; P2 (TonTon)
     * uses SS_ANIM_PLAYER2_* (its own baked bank-1 tiles). The four states line up
     * 1:1 in both sets (RUN/CLIMB/FIRE/DEATH), so we select the set by p->which. */
    u8 a_run   = p->which ? SS_ANIM_PLAYER2_RUN   : SS_ANIM_PLAYER_RUN;
    u8 a_climb = p->which ? SS_ANIM_PLAYER2_CLIMB : SS_ANIM_PLAYER_CLIMB;
    u8 a_fire  = p->which ? SS_ANIM_PLAYER2_FIRE  : SS_ANIM_PLAYER_FIRE;
    u8 a_death = p->which ? SS_ANIM_PLAYER2_DEATH : SS_ANIM_PLAYER_DEATH;
    u8 id;

    if (p->fireAnim) p->fireAnim--;

    if (dying)                 id = a_death;
    else if (p->fireAnim)      id = a_fire;
    else if (p->climbing)      id = a_climb;
    else                       id = a_run;

    /* anim_set restarts only on a CHANGE of id; FIRE/DEATH are one-shots we want
     * to (re)start cleanly when newly entered. */
    if (id == a_death && p->av.id != a_death)
        anim_restart(&p->av, id);
    else
        anim_set(&p->av, id);

    p->tile = anim_tick(&p->av);
}

/* ---------------------------------------------------------------------------
 *  players_separate -- co-op overlap resolve (arcade sub_A98A $A945).
 *
 *  If the two players are within the overlap box -- ~16px vertical (the arcade's
 *  +8 then <0x10 test on the Y byte) AND ~24px horizontal (+0xC then <0x18 on the
 *  X word) -- push them apart horizontally by (0xC - |dx|)/2 each, so they settle
 *  ~12px apart. The RIGHT player (larger x) is pushed right, the LEFT player left,
 *  exactly like the arcade exg-on-bmi. Keeps the two heroes from stacking into one
 *  blob while both share the screen. ------------------------------------------ */
static void players_separate(void)
{
    Player *p1 = &g_game.players[0];
    Player *p2 = &g_game.players[1];
    s16 dy = (s16)(p1->y - p2->y);
    s16 dx, adx, half;
    Player *hi, *lo;

    if (dy < -8 || dy > 7)  return;             /* vertical gap too big (±8 -> 16px) */
    dx = (s16)(p1->x - p2->x);
    if (dx < -12 || dx > 11) return;            /* horizontal gap too big (±12->24px)*/

    if (dx < 0) { adx = (s16)(-dx); hi = p2; lo = p1; }  /* p1 left -> p2 is right   */
    else        { adx = dx;         hi = p1; lo = p2; }
    half = (s16)((12 - adx) >> 1);
    hi->x = (s16)(hi->x + half); hi->xSub = 0;  /* push the right player right       */
    lo->x = (s16)(lo->x - half); lo->xSub = 0;  /* push the left player left         */
}

/* ---------------------------------------------------------------------------
 *  game_update -- one frame of simulation + the death/respawn/game-over flow.
 *
 *  Flow (GameFlow state machine; constants in game.h, semantics from the arcade
 *  RE -- player death sub_B286 / respawn sub_B1C2 / the front-end game-over
 *  unwind):
 *
 *    FLOW_ALIVE      normal play. After the entity pass, a lethal enemy contact
 *                    (g_player_dead_request, raised by entity.c only when the
 *                    player is NOT invulnerable) costs one life. If a life
 *                    remains -> DYING; else -> GAME_OVER.
 *    FLOW_DYING      the hero is disabled (forced-zero pad) + blinks ~1s; he is
 *                    invulnerable so the still-marching enemies can't re-trigger.
 *                    On timeout he respawns (arcade B1C2 re-place) -> RESPAWNING.
 *    FLOW_RESPAWNING controllable again but invulnerable + blinking for the
 *                    i-frame grace window (~2.5s). On timeout -> ALIVE.
 *    FLOW_GAME_OVER  input frozen, GAME OVER overlay shown ~2.5s; the scene
 *                    keeps scrolling underneath. On timeout game_update returns
 *                    TRUE so game_run() leaves the loop and main.c re-enters the
 *                    selector.
 *
 *  Returns TRUE only on the frame the GAME OVER beat finishes (caller should
 *  exit the game loop); FALSE otherwise.
 *
 *  The scrolling level + the entity pool ALWAYS update (so the world stays alive
 *  during death/respawn/game-over); only the PLAYER's input-driven control is
 *  gated by the flow. g_player_invuln is published to the entity collision pass
 *  so contact is ignored during DYING + RESPAWNING.
 * ------------------------------------------------------------------------- */
bool game_update(void)
{
    g_frameCounter++;          /* headless FPS probe tick (see g_frameCounter) */

    /* GLOBAL auto-scroll: advance the camera by the constant forward creep BEFORE the
     * players move/clamp, so each player's band clamp this frame sees the new camera.
     * This is the arcade's player-independent scroll (sub_A697); the camera no longer
     * follows any one player (that is what lets co-op players share the screen). */
    world_scroll_tick();

    /* Player control + anim, for EACH active player (co-op: P1=JOY_1, P2=JOY_2).
     * Live only while ALIVE or RESPAWNING (players move during the i-frames);
     * DYING / GAME_OVER coast on a zero pad. The attract demo drives P1 only
     * (game_init forces 1P in attract). NOTE: for now both players share the
     * single game flow + P1-driven death; per-player life/flow lands in Phase 3. */
    {
        u8 i;
        for (i = 0; i < g_game.numPlayers; i++)
        {
            Player *pp = &g_game.players[i];
            u16 pad = 0;
            /* per-player invuln published to the collision pass: no hit while this
             * hero is dying, in respawn i-frames, or out (game over). */
            pp->invuln = (u8)(pp->flow == FLOW_DYING ||
                              pp->flow == FLOW_RESPAWNING ||
                              pp->flow == FLOW_GAME_OVER);
            /* controllable only while ALIVE or RESPAWNING; DYING/out coast on 0. */
            if (pp->flow == FLOW_ALIVE || pp->flow == FLOW_RESPAWNING)
                pad = g_attract ? (i == 0 ? attract_pad() : 0)
                                : JOY_readJoypad(i == 0 ? JOY_1 : JOY_2);
            player_control(pp, pad);
            player_animate(pp, (u8)(pp->flow == FLOW_DYING));
        }
        /* 2-player overlap resolve (arcade sub_A98A $A945): if the two players are
         * within ~12px H and ~16px V, push them apart horizontally. (Skip when
         * either is out -- an out player isn't on the field.) */
        if (g_game.numPlayers > 1 &&
            g_game.players[0].flow != FLOW_GAME_OVER &&
            g_game.players[1].flow != FLOW_GAME_OVER)
            players_separate();
    }

    /* Entities always run (scene stays alive through every flow phase). */
    entity_update_all();

    /* ===== SEAMLESS LOOPING RIBBON bookkeeping (runs EVERY phase so the world
     * keeps scrolling during death/respawn/game-over, exactly like the arcade). =====
     *
     * (1) WRAP the coordinate system at the ribbon end. The camera free-runs right
     *     forever; once it passes ONE FULL RIBBON width, subtract a whole ribbon from
     *     the camera, the player, every live entity, and the spawn cursor. Because the
     *     wrap is EXACTLY one ribbon, the scenery at cameraX and cameraX-ribbonW is
     *     IDENTICAL (same column % SS_RIBBON_COLS) and every on-screen position
     *     (x - cameraX) is preserved -- so the wrap is invisible. It keeps cameraX
     *     (s16) bounded over an endless looping session.
     *
     *     The 64-col ring maps plane col = streamCol & 63, and a full ribbon (1418
     *     cols) is NOT a multiple of 64, so the wrap shifts the ring phase. We absorb
     *     that by REFILLING the whole 64-col window from the post-wrap coordinates in
     *     this one frame (ribbon_refill). It is tear-free precisely because the pre-
     *     and post-wrap scenery are identical -- every plane cell ends up holding the
     *     same tile it already showed. One refill per full loop (~75-220s of travel). */
    if (g_ribbonW > 0 && g_game.cameraX >= g_ribbonW)
    {
        s16 dx = (s16)(-g_ribbonW);
        u8  i;
        g_game.cameraX     = (s16)(g_game.cameraX + dx);
        for (i = 0; i < g_game.numPlayers; i++)
            g_game.players[i].x = (s16)(g_game.players[i].x + dx);
        entity_shift_worldx(dx);                 /* shift all entities + spawn cursor */
        ribbon_refill();                         /* rebuild the 64-col window, no tear */
    }

    /* (2) SEGMENT (scenery) tracking: keep g_curLevel/g_levelIdx pointing at the
     *     segment under the camera, and when that segment INDEX changes (a level-to-
     *     level seam just scrolled in), do the per-boundary bookkeeping (HUD round,
     *     saturating difficulty stage, wave re-arm, music cue) WITHOUT any pause or
     *     rebuild. Only the index change matters; the scroll already flowed across. */
    {
        u8 prevSeg = g_levelIdx;
        ribbon_track_camera();                   /* updates g_curLevel + g_levelIdx   */
        if (g_levelIdx != prevSeg)
            segment_advance(g_levelIdx);
    }

    /* --- PER-PLAYER flow state machine (co-op: each hero has its own life cycle).
     * A lethal contact (pp->deadReq, raised by the collision pass only when that
     * player is not invulnerable) costs THAT player one life: ALIVE -> DYING (life
     * remains) or -> GAME_OVER (out). An out player stays out (no sprite, no
     * control) while the other keeps playing. The GAME ends only when ALL active
     * players are out (the all-out beat below). lives is "in hand incl. current". */
    {
        u8 i;
        u8 allOut = TRUE;
        for (i = 0; i < g_game.numPlayers; i++)
        {
            Player *pp = &g_game.players[i];
            switch (pp->flow)
            {
                case FLOW_ALIVE:
                    if (pp->deadReq)
                    {
                        if (pp->lives > 0) pp->lives--;
                        snd_sfx_death();           /* arcade death SFX (cmd 0x14) */
                        if (pp->lives == 0)
                            pp->flow = FLOW_GAME_OVER;     /* this hero is out     */
                        else
                        {
                            pp->flow      = FLOW_DYING;
                            pp->flowTimer = DEATH_FRAMES;
                        }
                    }
                    break;

                case FLOW_DYING:
                    if (--pp->flowTimer == 0)
                    {
                        player_respawn(pp);        /* arcade B1C2 re-place        */
                        pp->flow      = FLOW_RESPAWNING;
                        pp->flowTimer = RESPAWN_IFRAMES;
                    }
                    break;

                case FLOW_RESPAWNING:
                    if (--pp->flowTimer == 0)
                        pp->flow = FLOW_ALIVE;     /* i-frames over, back to play */
                    break;

                case FLOW_GAME_OVER:
                default:
                    break;                          /* out: stays out             */
            }

            /* drop any stray request (overlap that began as the flow changed). */
            pp->deadReq = 0;

            /* EXTRA LIFE (arcade DSW2 0x18) for THIS player's own score. */
            check_extra_life(pp);

            if (pp->flow != FLOW_GAME_OVER) allOut = FALSE;
        }

        /* GAME OVER when every active player is out: run the final overlay beat,
         * then return to the front-end. (In 1P this is just P1's game-over.) */
        if (allOut)
        {
            if (g_gameOverBeat == 0) g_gameOverBeat = GAMEOVER_FRAMES;
            if (--g_gameOverBeat == 0) return TRUE;
        }
    }
    /*
     * SEAMLESS LOOPING RIBBON: the 8 baked sceneries are concatenated into ONE
     * endless looping ribbon (SS_RIBBON_COLS), all tilesets co-resident, streamed
     * continuously by bg_stream/stream_column. The perpetual creep carries the camera
     * forward forever; the scenery flows from one segment straight into the next with
     * NO transition/cold-fill (the seam is just another streamed column), and the
     * ribbon wraps after the last segment back to the first (the cameraX wrap above,
     * tear-free via ribbon_refill). The per-segment bookkeeping (HUD round, saturating
     * difficulty stage, wave re-arm, music cue) fires the moment a seam scrolls in
     * (segment tracking above) -- faithful to the arcade's one looping ribbon whose
     * saturating $005A stage ramps difficulty as you travel (tools/LEVEL_END_SPEC.md).
     * See stream_column / ribbon_locate / segment_advance / ribbon_refill. */
    return FALSE;
}

/* ---------------------------------------------------------------------------
 *  bg_stream -- keep the RIBBON resident in the 64-col Plane A window.
 *
 *  The 64-col plane is a ring buffer over the ribbon: stream column S lives at
 *  plane column S & PLANE_COL_MASK (S is a MONOTONIC counter in the camera frame --
 *  it is NOT wrapped to the ribbon width here; stream_column wraps the SOURCE column
 *  by SS_RIBBON_COLS internally, so the ribbon's own scenery loop -- last segment
 *  back to the first -- is seamless regardless of the ring phase). We keep every
 *  column the visible 256px window can show resident: the window shows columns
 *  [cameraX/8 .. cameraX/8 + 32]; the 64-wide plane gives 32 cols of slack, so we
 *  extend the loaded range a lead column ahead of the right edge. As the camera
 *  advances past a tile boundary we stream the newly-needed column(s) -- typically
 *  0 or 1 per frame (the forward creep is < 1 tile/frame), so streaming touches just
 *  the new seam column, never a full repaint -> 60fps. (Once per full ribbon loop,
 *  ribbon_refill rebuilds the whole window when the cameraX wrap shifts the ring.)
 *
 *  SEAM SAFETY: a freshly-streamed column S overwrites plane column S & 63, which
 *  previously held column S-64 -- already 32+ cols off the LEFT of the visible
 *  window, so the overwrite is never visible. This holds at LEVEL-TO-LEVEL seams
 *  too (the next segment's columns stream in exactly like any other). New columns
 *  are queued (DMA_QUEUE) and flushed in V-blank before they scroll into view. */
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
    /* SAT for the player (slot 0) + the entity pool (slots 1..) + the score-popup
     * glyph sprites. Sized for the player plus ENTITY_SAT_MAX entity bodies plus
     * ENTITY_POPUP_GLYPH_MAX popup glyphs; well under the 80-sprite hardware
     * ceiling. */
    static u16 sat[(MAX_PLAYERS + ENTITY_SAT_MAX + ENTITY_POPUP_GLYPH_MAX) * 4];
    u8  np = g_game.numPlayers;
    u16 nEnt;
    u16 totalSprites;
    u8  k;

    /* Stream any newly-revealed level columns into the plane window FIRST, so
     * the queued column DMAs flush in the same V-blank as the scroll update. */
    bg_stream();

    /* --- entity pool first: append visible entities at slots np.. (after the
     * player slots) and get the count back so we can chain the players to them. --- */
    nEnt = entity_render_all(sat, np);

    /* --- player sprites at slots 0..np-1 (P1=SonSon, P2=TonTon) ---
     * Each player is drawn in SCREEN space (world position minus the camera; the
     * VDP doesn't auto-scroll sprites). Blink during a transient life-cycle phase:
     *   - the brief on-contact hurt-flash (g_player_hurt, P1 for now), and
     *   - the death beat + respawn i-frame window (flow DYING / RESPAWNING),
     * by parking the sprite off-screen on alternate frames (link kept so the chain
     * to the entities stays intact). The chain runs P1 -> P2 -> first entity.
     * NOTE: P2 currently borrows P1's tiles + PAL1 (placeholder); Phase 2b gives
     * TonTon its own baked tiles + color bank. */
    for (k = 0; k < np; k++)
    {
        const Player *pp = &g_game.players[k];
        s16 px = (s16)(pp->x - g_game.cameraX);
        s16 py = pp->y;
        u16 nextLink;
        u8  blink = 0;

        /* per-player blink/hide: an OUT (game-over) hero is gone entirely; a DYING/
         * RESPAWNING hero blinks on its own flowTimer; a freshly-hit hero flashes on
         * its own hurt window. (co-op: each player's beat is independent.) */
        if (pp->flow == FLOW_GAME_OVER)
            blink = 1;                                  /* out -> sprite removed   */
        else if (pp->flow == FLOW_DYING || pp->flow == FLOW_RESPAWNING)
            blink = (u8)((pp->flowTimer & BLINK_MASK) != 0);
        else if (pp->hurt)
            blink = (u8)((pp->hurt & BLINK_MASK) != 0);

        if (blink)
        {
            sat[k * 4 + 0] = (u16)((0x80 + 240) & 0x3FF);   /* parked off-screen */
            sat[k * 4 + 3] = (u16)0x80;
        }
        else
        {
            sat[k * 4 + 0] = (u16)((0x80 + py) & 0x3FF);
            sat[k * 4 + 3] = (u16)((0x80 + px) & 0x1FF);
        }

        /* link to the next player, or (for the last) to the first entity if any. */
        nextLink = (u16)((k + 1 < np) ? (k + 1) : (nEnt ? np : 0));
        sat[k * 4 + 1] = (u16)((SPRITE_SIZE(2, 2) << 8) | nextLink);
        sat[k * 4 + 2] = (u16)TILE_ATTR_FULL(PAL1, 0, 0, pp->facing, pp->tile);
    }

    /* One DMA of the whole SAT (players + entities) to the hardware sprite list. */
    totalSprites = (u16)(np + nEnt);
    DMA_queueDma(DMA_VRAM, sat, VDP_getSpriteListAddress(),
                 (u16)(totalSprites * 4), 2);

    /* BG horizontal scroll: shift Plane A left by the camera. The 64-col plane
     * is 512px wide and the HScroll register wraps mod 512, which matches the
     * ring layout (level col L at plane col L&63) -- so -cameraX aligns the
     * resident columns under the visible window for any cameraX. */
    VDP_setHorizontalScroll(BG_A, (s16)(-g_game.cameraX));

    /* --- CO-OP HUD readout (Plane B, the transparent top band; H32 = 32 cols) ---
     * Faithful to the arcade's two-player score header (RE'd from the machine's
     * own display test: "SONSON ... HIGH ... TONTON"): P1's score under SONSON on
     * the left, the high score under HIGH in the middle, P2's (TonTon's) score on
     * the right -- each player keeps its OWN score (the whole point of the co-op
     * split). TONTON's column shows only in a 2-player game. Row 2 carries the
     * native dev RND/FPS readout + each player's remaining lives.
     *   Row 0:  SONSON(0..5)        HIGH(13..16)      TONTON(26..31)
     *   Row 1:  <p1 6dig>(0..5)     <high 6dig>(12..17)  <p2 6dig>(26..31)
     *   Row 2:  RND nn | FPS nn | 1x n  [ 2x n ]
     * (Level Plane A rows 0-4 are transparent, so this band shows over the art.) */
    {
        char buf[12];
        u32 fps = SYS_getFPS();
        u32 hi  = g_game.players[0].score;
        if (g_game.numPlayers > 1 && g_game.players[1].score > hi)
            hi = g_game.players[1].score;
        if (hi < 10000) hi = 10000;        /* arcade default HIGH = 10000        */

        /* P1 (SONSON), left */
        VDP_drawText("SONSON", 0, 0);
        intToStr((s32)g_game.players[0].score, buf, 6);
        VDP_drawText(buf, 0, 1);

        /* HIGH, center */
        VDP_drawText("HIGH", 13, 0);
        intToStr((s32)hi, buf, 6);
        VDP_drawText(buf, 12, 1);

        /* P2 (TONTON), right -- only in a 2-player game */
        if (g_game.numPlayers > 1)
        {
            VDP_drawText("TONTON", 26, 0);
            intToStr((s32)g_game.players[1].score, buf, 6);
            VDP_drawText(buf, 26, 1);
        }

        /* Row 2: dev ROUND + FPS, then per-player lives ("1x n" / "2x n"). */
        intToStr((s32)g_game.round, buf, 2);
        VDP_drawText("RND", 0, 2);
        VDP_drawText(buf, 4, 2);
        if (fps > 99) fps = 99;
        intToStr((s32)fps, buf, 2);
        VDP_drawText("FPS", 7, 2);
        VDP_drawText(buf, 11, 2);
        {
            char lv[5];
            lv[0] = '1'; lv[1] = 'x';
            lv[2] = (char)('0' + (g_game.players[0].lives <= 9
                                  ? g_game.players[0].lives : 9));
            lv[3] = ' '; lv[4] = 0;
            VDP_drawText(lv, 15, 2);
            if (g_game.numPlayers > 1)
            {
                lv[0] = '2';
                lv[2] = (char)('0' + (g_game.players[1].lives <= 9
                                      ? g_game.players[1].lives : 9));
                VDP_drawText(lv, 21, 2);
            }
        }
    }

    /* (The floating "+N" score POPUPS are emitted as hardware sprites inside
     * entity_render_all -- font-tile glyphs chained into the same SAT DMA, so they
     * cost no active-display VRAM writes. See entity_render_all.) */

    /* --- GAME OVER overlay (Plane B, centered) ---
     * Shown for the whole FLOW_GAME_OVER beat over the still-scrolling level.
     * Drawn on the unscrolled text plane so it stays pinned + centered. No need
     * to clear it afterwards: a game-over returns to the selector (main.c), and
     * a fresh session's game_init re-inits Plane B (VDP_clearTextArea) -- so the
     * overlay can never linger into the next life or the next session. */
    if (g_gameOverBeat)
        VDP_drawText("GAME OVER", 11, 13);   /* H32: (32-9)/2 ~= 11 -> centered  */

    /* (No area-transition banner: the seamless ribbon never pauses -- the scenery
     * flows continuously from one segment into the next, so there is no round-clear
     * interlude to flash. The HUD "RND" field (above) tracks the segment under the
     * camera, updated live as each seam scrolls in.) */
}

/* ---------------------------------------------------------------------------
 *  game_run -- a whole game SESSION: init, then the update/render/V-blank loop,
 *  returning to the caller (main.c) when the game is over.
 *
 *  This is the native mirror of the arcade's "game over unwinds back to the
 *  attract/selector" (project memory: the front-end re-entry for(;;) loop in
 *  main()). main.c calls game_run() inside its own boot for(;;): SEGA/CAPCOM run
 *  ONCE before the loop; the loop body is selector -> config -> game_run, so a
 *  game-over returns here and main.c falls back to the selector for the next
 *  session. game_update returns TRUE once the GAME OVER beat is done, which is
 *  our cue to stop and return.
 *
 *  The per-frame discipline is unchanged from the old main loop: simulate
 *  (game_update), build SAT + scroll + HUD (game_render), then hand the frame to
 *  SGDK (SYS_doVBlankProcess -- flush the DMA queue + wait for V-blank, the
 *  single 60Hz timebase). Holding 60fps: the flow logic is a handful of byte
 *  compares + one branch per frame, and death/respawn/HUD add no per-pixel work,
 *  so the busy-scene frame budget is untouched. */
void game_run(void)
{
    u16 demoT = 0;
    game_init();
    while (TRUE)
    {
        bool done = game_update();
        game_render();
        SYS_doVBlankProcess();
        if (done) break;           /* GAME OVER finished -> back to the selector */
        if (g_attract)             /* attract demo: abort on any button, else time-cap */
        {
            if (JOY_readJoypad(JOY_1) & (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C))
                { g_attract_started = 1; break; }
            if (++demoT >= ATTRACT_MAX_FRAMES) break;
        }
    }

    /* AUDIO teardown: stop the music/SFX, silence the chips, and DETACH the
     * V-Blank replay tick so the front-end (selector / SEGA / CAPCOM splashes)
     * owns the sound hardware again on the way back.  Without this, snd_vblank
     * would keep firing during those stages and fight their direct YM use. */
    snd_shutdown();
}

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
 *  Phase 4 replaces the single static screen with a WIDE baked level; Phase 5
 *  ships SS_LEVEL_COUNT DISTINCT levels (ss_level.c: ss_levels[], ~5 screens each,
 *  baked from REAL arcade gameplay -- md=02, the CLEAN playfields, NOT the attract
 *  demo). Each round plays a different one (round 1->level1, 2->level2, ..., then
 *  it cycles; see level_select / round_advance). The 64-col Genesis Plane A is
 *  driven as a COLUMN-STREAMED sliding window over the active level (ring buffer;
 *  see stream_column / bg_stream):
 *  as the player's perpetual forward creep advances the camera, newly-revealed
 *  columns are decoded + DMA'd in at the seam (1 column/frame, tear-free) and
 *  the BG hardware-scrolls -- so the forward run travels through the whole level
 *  at full 60fps. The player keeps its screen-band pin and a baked ss_gfx sprite
 *  tile; the FPS readout stays on the unscrolled Plane B.
 * =========================================================================== */

GameState g_game;

/* Current level descriptor + its pixel width (Phase 5 multi-level). g_curLevel
 * points into ss_levels[] for the round being played; g_levelW = cols*8. Both are
 * set by level_select() below (called from level_restart / round_advance). They
 * default to level 1 so any code reading them before the first select is sane. */
const SsLevel *g_curLevel = &ss_levels[0];
u16            g_levelW   = 0;

/* Index of the level resident this round (round R -> ss_levels[(R-1)%8]); set by
 * level_select(), read by tier_solid() to pick ss_collision[g_levelIdx]. */
static u8      g_levelIdx = 0;

/* Free-running game-loop tick: incremented once per game_update(). Used by the
 * headless FPS probe (tools/entity_fps.lua) to measure the actual game-frame
 * rate -- if the loop holds 60fps this advances at 60Hz; any slowdown shows as
 * a lower rate (the loop runs less often than the 60Hz V-blank). Harmless in
 * the shipped game (a single counter). */
volatile u32 g_frameCounter = 0;

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
static void check_extra_life(void)
{
    u8 bl = g_config.bonusLife;
    const u32 *list;
    if (bl > 3) bl = 3;
    list = k_bonus_thresholds[bl];

    while (g_game.bonusNextIdx < BONUS_LIST_MAX)
    {
        u32 thr = list[g_game.bonusNextIdx];
        if (thr == 0) break;                       /* list terminator (idx2/idx3)  */
        if (g_game.score < thr) break;             /* not reached the next one yet */

        /* crossed this threshold: award a life (capped) + advance */
        if (g_game.lives < LIFE_MAX) g_game.lives++;
        g_game.bonusNextIdx++;
#ifdef SND_CMD_1UP
        snd_command(SND_CMD_1UP);                  /* 1-up cue (see sound.h note)  */
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
    u16 cols     = g_curLevel->cols;           /* current level width (Phase 5) */
    u16 row;

    if (levelCol < 0 || levelCol >= (s16)cols)
    {
        for (row = 0; row < SS_LEVEL_ROWS; row++) colbuf[row] = 0;   /* blank   */
    }
    else
    {
        const u16 *src = &g_curLevel->map[(u16)levelCol];  /* row 0 of this col */
        for (row = 0; row < SS_LEVEL_ROWS; row++)
            colbuf[row] = src[row * cols];                 /* stride a whole row */
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
 *  level_select -- choose the baked level for a given round and make it active.
 *
 *  Phase 5 multi-level: the engine ships SS_LEVEL_COUNT distinct levels captured
 *  from REAL arcade gameplay (ss_levels[], see ss_level.c). Round R (1-based)
 *  plays ss_levels[(R-1) % SS_LEVEL_COUNT] -- round 1->level1, 2->level2, ...,
 *  then it cycles. Selecting a level: point g_curLevel at it, set the runtime
 *  level width g_levelW (= cols*8, used by the camera clamp), and DMA its deduped
 *  tiles into the shared level-tile VRAM region at SS_LEVEL_TILE_BASE (only ONE
 *  level is resident at a time -- the per-round tile budget is max(per-level),
 *  not the sum). The level's map words are absolute (base+local), so they're
 *  consistent for whichever level is loaded at SS_LEVEL_TILE_BASE.
 *
 *  Tiles are loaded with immediate DMA so they're resident before level_restart
 *  cold-fills the plane from this level's map. Caller then streams the map. */
static void level_select(u8 round)
{
    u8 idx = (u8)((round - 1) % SS_LEVEL_COUNT);   /* round 1 -> level 0, cycle */
    g_curLevel = &ss_levels[idx];
    g_levelIdx = idx;                              /* -> ss_collision[idx]       */
    g_levelW   = (u16)(g_curLevel->cols * 8);

    /* Load THIS level's tiles into the shared level region (base 135). The level
     * tile count varies per level but all fit in SS_LEVEL_MAX_TILES slots, which
     * sit below the resident anim tiles (SS_ANIM_TILE_BASE) -- no overlap. */
    VDP_loadTileData(g_curLevel->tiles, SS_LEVEL_TILE_BASE,
                     g_curLevel->tileCount, DMA);
}

/* ---------------------------------------------------------------------------
 *  level_restart -- (re)build the level window + place the player at the start.
 *
 *  Factored out of game_init so the ROUND-CLEAR advance can reuse the exact same
 *  level setup: cold-fill the 64-col Plane A window with the first 64 level
 *  columns (immediate DMA so it's resident before the loop streams the rest in),
 *  reset the column-streaming range, park the camera at the level start, and
 *  spawn the player low-left on lane 0 pinned at the band -- the SonSon hero's
 *  start pose. Used by game_init (first round) and the round advance (each new
 *  round runs THIS round's level -- level_select picked it). Does NOT touch
 *  score / lives / round / flow -- those are the caller's policy. */
static void level_restart(bool keepPose)
{
    /* Select the level for the CURRENT round (loads its tiles at base 135, sets
     * g_curLevel + g_levelW). game_init sets round=1 first; round_advance bumps it
     * before calling here -- so each round shows its own captured level, cycling
     * through ss_levels[] (round 1->level1 ... round 8->level8, then round 9->level1). */
    level_select(g_game.round);

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

    {
        Player *p = &g_game.player;
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

    g_game.cameraX  = 0;
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
     *   level tiles  [SS_LEVEL_TILE_BASE .. +SS_LEVEL_MAX_TILES)  (base = 135)
     *                  -- ONE round's level resident at a time (level_select DMAs
     *                     the active round's tiles here); max 91 tiles.
     *   anim tiles   [SS_ANIM_TILE_BASE .. )               (base = 346)
     * The scene's static BG is dropped; the WIDE per-round level replaces it. We
     * still load the scene tiles because the player sprite borrows
     * ss_scene_sprites[0].tile. The level tiles are loaded per-round by
     * level_select() (called from level_restart via the round below). */
    VDP_loadTileData(ss_scene_tiles, SS_SCENE_TILE_BASE, SS_SCENE_TILE_COUNT, DMA);
    /* Faithful player + enemy animation frames, loaded at a fresh VRAM base
     * (346) past the scene (0..134) and level (135..225, max) tiles -- well below
     * the SGDK font/sprite-engine tiles near ~1376. The baked anim table's
     * per-frame tile indices are ABSOLUTE (already include SS_ANIM_TILE_BASE). */
    VDP_loadTileData(ss_anim_tiles, SS_ANIM_TILE_BASE, SS_ANIM_TILE_COUNT, DMA);

    /* Round progression: a NEW game (from the selector) always starts at round 1.
     * The round number drives the HUD readout, the per-round LEVEL select
     * (level_restart -> level_select), and entity.c's per-round difficulty ramp.
     * Set it BEFORE level_restart so the right level (level 1) is loaded. */
    g_game.round = 1;
    g_game.stage = 0;                 /* difficulty counter ($005A) starts at 0     */

    /* Build the level window + place the player at the start (shared with the
     * round advance -- see level_restart). level_restart selects round 1's level
     * (loads its tiles at base 135 + sets g_levelW), cold-fills Plane A from its
     * map, resets the stream range + camera, and spawns the hero low-left. FALSE =
     * full cold-start pose (round_advance passes TRUE to keep the pose across a swap). */
    level_restart(FALSE);

    g_game.score   = 0;

    /* Life economy: the DIP/options LIVES setting now takes effect. g_config.lives
     * is the arcade DSW2 0x03 field decoded by sync_from_dsw() (3 / 4 / 5 / 7);
     * the front-end wrote it before game_init. We treat it as the STARTING life
     * count (lives in hand, the current one included). Start ALIVE, no flow. */
    g_game.lives     = g_config.lives;
    g_game.bonusNextIdx = 0;          /* no bonus thresholds reached yet            */
    g_game.flow      = FLOW_ALIVE;
    g_game.flowTimer = 0;

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
     * the SFX channel.) Subsequent rounds re-arm this via round_advance(). */
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
static void player_respawn(void)
{
    Player *p = &g_game.player;
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
    anim_restart(&p->av, SS_ANIM_PLAYER_RUN);       /* respawn in the run cycle */
}

/* ---------------------------------------------------------------------------
 *  round_advance -- swap in the NEXT scenery after the auto-scroll reaches the end.
 *
 *  The native engine ships SS_LEVEL_COUNT DISTINCT captured levels (ss_levels[],
 *  baked from real arcade gameplay). Advancing: bump the round counter (selects the
 *  next scenery) AND tick the saturating stage counter (difficulty), then rebuild
 *  via level_restart(TRUE) -- which calls level_select(round) to load the NEW
 *  round's level (round 1->level1 ... then it cycles), cold-fill Plane A from that
 *  level's map, and re-pin the hero at the band start WITHOUT re-posing him (his
 *  lane / facing / anim carry over -- the arcade scroll never re-poses at a segment
 *  boundary). We also clear + re-seed the entity pool (entity_init re-runs the spawn
 *  wave; the fresh enemies pick up the new STAGE's difficulty at spawn) and replay
 *  the boundary music cue. The caller (game_update) flips the flow back to ALIVE.
 *
 *  All 8 arcade levels are captured + baked into ss_levels[] (SS_LEVEL_COUNT = 8;
 *  gen_level.py --multi level1..level8 from real md=02 gameplay); rounds cycle
 *  round1->level1 ... round8->level8 then wrap. The only per-round VRAM resident set
 *  is the active level's tiles (max 91, base 135) -- see level_select.
 *
 *  FAITHFULNESS NOTE: the RE proved SonSon has NO discrete level-end / "climb to
 *  advance" tower finish -- it is one endless LOOPING auto-scroll ribbon whose
 *  saturating $005A stage counter ramps difficulty as you travel (tools/
 *  LEVEL_END_SPEC.md). This re-expresses that over the 8 baked sceneries: the creep
 *  carries the hero forward (no wall to push) and a brief transition swaps scenery.
 *  TODO(seamless ribbon): a future step can concatenate the 8 maps into one
 *  continuously-streamed looping ribbon (no per-level cold-fill) for a truly seam-
 *  free scroll -- needs an all-levels-resident tileset (tile rebasing) or seam
 *  reloads; deferred as its own milestone (the s16 coordinate model stays as-is). */
static void round_advance(void)
{
    if (g_game.round < 255)       g_game.round++; /* next scenery + HUD pass       */
    if (g_game.stage < STAGE_MAX) g_game.stage++; /* saturating difficulty ($005A) */

    level_restart(TRUE);                          /* swap scenery, KEEP the pose   */
    entity_init();                                /* clear + re-seed the wave      */

    /* Boundary music cue (arcade fires a sound command at segment boundaries;
     * 0x16 then 0x10 -- the round flourish). Re-armed for the new scenery. */
    snd_play_round();
}

/* ===========================================================================
 *  TERRAIN COLLISION -- the native sub_A847 ($A847).
 * ===========================================================================
 *  The arcade A847 maps the hero's worldX (-$B) + scroll ($90) to a 32-col ring
 *  column, then tests the per-column 4-byte solidity mask at RAM $0100 with a row
 *  bit (Y>>3). We re-express that on the native per-column collision table
 *  (ss_collision[level], one 6-bit mask/column; bit t = tier t solid). The native
 *  level's columns ARE world columns (worldX/8), so the lookup is direct -- no
 *  ring wrap (the native streams the whole wide level, not a 32-col ring). */

/* Is tier `tier` (0=bottom..5=top) SOLID at world X `worldX`? Out-of-level columns
 * read as solid (so the hero never falls off the level's left/right end -- the
 * camera clamps the left and the auto-advance handles the right). */
static u8 tier_solid(s16 worldX, u8 tier)
{
    u16 cols = ss_collision_cols[g_levelIdx];
    s16 col  = (s16)(worldX >> 3);          /* world column = px/8 (A847 cellX>>3) */
    if (col < 0 || col >= (s16)cols) return 1;
    if (tier >= SS_COLLISION_TIERS)  return 0;
    return (u8)((ss_collision[g_levelIdx][col] >> tier) & 1);
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
static void player_vertical(u16 pad)
{
    Player *p = &g_game.player;

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
static void player_control(u16 pad)
{
    Player *p = &g_game.player;

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
    player_vertical(pad);

    /* ----- CAMERA + horizontal screen-band clamp (arcade sub_AB52).
     * The hero is pinned to the band [BAND_LEFT..BAND_RIGHT] in SCREEN space;
     * the world scrolls to keep him there. We move the camera so screenX stays
     * inside the band, clamp ONLY the LEFT edge (the level start), and re-pin the
     * player's *world* X to the band there.
     *
     * FAITHFUL AUTO-SCROLL -- no right wall: the camera follows the hero (carried by
     * the perpetual creep) WITHOUT clamping at the level's right edge. game_update
     * advances to the next scenery the frame the creep reaches the end (cameraX >=
     * maxCam), which resets cameraX to 0, so it never needs a right clamp and never
     * overshoots by more than one frame's creep before the swap. This removes the
     * old "run into the finish wall and push" interaction the arcade never had. ----- */
    {
        s16 screenX;

        /* scroll the camera to keep the player inside the band */
        screenX = (s16)(p->x - g_game.cameraX);
        if (screenX < BAND_LEFT)        g_game.cameraX = (s16)(p->x - BAND_LEFT);
        else if (screenX > BAND_RIGHT)  g_game.cameraX = (s16)(p->x - BAND_RIGHT);

        if (g_game.cameraX < 0) g_game.cameraX = 0;   /* clamp ONLY the left edge   */

        /* re-pin the player's world X to the band so he never escapes the visible
         * window at the level START (cameraX==0); the right edge no longer walls. */
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
            if (entity_player_fire((s16)(p->x + 4), (s16)(p->y + 4), p->facing))
            {
                /* arcade player-shot SFX (verified cmd 0x0B), YM2612 SFX voice. */
                snd_sfx_fire();
                /* show the fire/recoil pose for the anim's full length */
                p->fireAnim = (u8)(ss_anims[SS_ANIM_PLAYER_FIRE].frames[0].dur +
                                   ss_anims[SS_ANIM_PLAYER_FIRE].frames[1].dur);
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
static void player_animate(u8 dying)
{
    Player *p = &g_game.player;
    u8 id;

    if (p->fireAnim) p->fireAnim--;

    if (dying)                 id = SS_ANIM_PLAYER_DEATH;
    else if (p->fireAnim)      id = SS_ANIM_PLAYER_FIRE;
    else if (p->climbing)      id = SS_ANIM_PLAYER_CLIMB;
    else                       id = SS_ANIM_PLAYER_RUN;

    /* anim_set restarts only on a CHANGE of id; FIRE/DEATH are one-shots we want
     * to (re)start cleanly when newly entered. */
    if (id == SS_ANIM_PLAYER_DEATH && p->av.id != SS_ANIM_PLAYER_DEATH)
        anim_restart(&p->av, id);
    else
        anim_set(&p->av, id);

    p->tile = anim_tick(&p->av);
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

    /* Publish invulnerability to the entity collision pass: the player can't be
     * hit while dying, in his respawn i-frames, during the game-over beat, or
     * during the ROUND CLEAR interlude (a non-combat beat -- the still-marching
     * enemies must not cost a life on the finish line). */
    g_player_invuln = (u8)(g_game.flow == FLOW_DYING ||
                           g_game.flow == FLOW_RESPAWNING ||
                           g_game.flow == FLOW_GAME_OVER ||
                           g_game.flow == FLOW_ROUND_CLEAR);

    /* Player control: live only while ALIVE or RESPAWNING (he moves during the
     * i-frames). DYING / GAME_OVER coast on a zero pad (no input). */
    if (g_game.flow == FLOW_ALIVE || g_game.flow == FLOW_RESPAWNING)
        player_control(JOY_readJoypad(JOY_1));
    else
        player_control(0);

    /* Advance the player's faithful animation (run / climb / fire / death). The
     * death tumble plays while the flow is DYING (the hero is disabled + blinks);
     * every other phase animates off his live motion/fire state. This is a table
     * lookup + a counter -- no per-pixel work, so the frame budget is untouched. */
    player_animate((u8)(g_game.flow == FLOW_DYING));

    /* Entities always run (scene stays alive through every flow phase). */
    entity_update_all();

    /* --- flow state machine --- */
    switch (g_game.flow)
    {
        case FLOW_ALIVE:
            /* A fresh lethal contact this frame? Consume it: lose a life. The
             * request is only ever raised when not invulnerable (entity.c), so
             * one touch == one life. lives is "lives in hand including current":
             * losing the LAST one (lives reaches 0) is GAME OVER. DEATH takes
             * priority over a round-clear: a fatal hit on the finish line still
             * costs the life (so dying mid-level never short-circuits to a clear). */
            if (g_player_dead_request)
            {
                g_player_dead_request = 0;
                if (g_game.lives > 0) g_game.lives--;

                /* play the arcade player-death SFX (verified cmd 0x14) on the
                 * lethal contact -- fires once as the hero enters the death beat,
                 * whether this is a respawn-death or the final (game-over) life. */
                snd_sfx_death();

                if (g_game.lives == 0)
                {
                    g_game.flow      = FLOW_GAME_OVER;
                    g_game.flowTimer = GAMEOVER_FRAMES;
                }
                else
                {
                    g_game.flow      = FLOW_DYING;
                    g_game.flowTimer = DEATH_FRAMES;
                }
                break;
            }

            /* FAITHFUL ADVANCE: the perpetual creep has carried the camera to the
             * level end -- NO wall to push, NO dwell. The scroll itself is the
             * advance (LEVEL_END_SPEC.md: SonSon has no finish line). maxCam is the
             * scroll position showing the level's last screen; the moment cameraX
             * reaches it, trip the BRIEF transition that swaps in the next baked
             * scenery + ticks the saturating stage. Only checked while ALIVE, so a
             * death / respawn beat can never be mistaken for an advance. */
            {
                s16 maxCam = (s16)((s16)g_levelW - SCREEN_W);  /* current level width */
                if (maxCam < 0) maxCam = 0;
                if (g_game.cameraX >= maxCam)
                {
                    g_game.flow      = FLOW_ROUND_CLEAR;
                    g_game.flowTimer = ROUND_CLEAR_FRAMES;
                    snd_play_round();             /* boundary cue (0x16+0x10)      */
                }
            }
            break;

        case FLOW_DYING:
            if (--g_game.flowTimer == 0)
            {
                player_respawn();                 /* arcade B1C2 re-place        */
                g_game.flow      = FLOW_RESPAWNING;
                g_game.flowTimer = RESPAWN_IFRAMES;
            }
            break;

        case FLOW_RESPAWNING:
            if (--g_game.flowTimer == 0)
                g_game.flow = FLOW_ALIVE;         /* i-frames over, back to play */
            break;

        case FLOW_GAME_OVER:
            if (--g_game.flowTimer == 0)
                return TRUE;                      /* leave the loop -> front-end */
            break;

        case FLOW_ROUND_CLEAR:
            /* BRIEF scenery transition (~0.6s): input is frozen (player_control got
             * a 0 pad above) just long enough to mask the next level's one-frame
             * cold-fill; a short STAGE flash shows (game_render). On timeout,
             * round_advance() swaps the scenery (keeping the hero's pose), ticks the
             * saturating stage, re-seeds entities at the new stage, and resumes play
             * -- the auto-scroll carries straight on into the next ribbon section. */
            if (--g_game.flowTimer == 0)
            {
                round_advance();
                g_game.flow = FLOW_ALIVE;         /* next scenery begins         */
            }
            break;

        default:
            break;
    }

    /* Stray request raised while not ALIVE (e.g. an overlap that began the same
     * frame the flow changed): drop it -- the invuln gate already covers the
     * common case, this is belt-and-braces so it can't leak into the next life. */
    g_player_dead_request = 0;

    /* EXTRA LIFE (arcade DSW2 0x18): award bonus lives for any threshold the score
     * has crossed. The threshold list is selected by g_config.bonusLife (decoded
     * from DSW2 0x18 by sync_from_dsw; dipswitch.c labels 20K/80K/100K etc.); see
     * k_bonus_thresholds[] + the RE notes in game.h. This is what makes the
     * Bonus-Life DIP actually grant lives (previously a no-op TODO). Cheap; checked
     * every frame (score only grows during ALIVE play, so this is safe anywhere). */
    check_extra_life();
    /*
     * AUTO-SCROLL PROGRESSION is implemented above: the FLOW_ALIVE case advances
     * the moment the perpetual creep carries the camera to the level end (cameraX
     * >= maxCam -- NO wall to push, NO dwell) -> FLOW_ROUND_CLEAR; that BRIEF
     * transition masks the scenery cold-fill, then round_advance() bumps the round
     * (next scenery) + the saturating stage (difficulty), swaps the level keeping
     * the hero's pose, re-seeds entities at the new stage, and resumes ALIVE -- so
     * the game is an endless forced auto-scroll, faithful to the arcade ribbon.
     *
     * All 8 arcade levels are captured + baked into ss_levels[] (SS_LEVEL_COUNT =
     * 8) and round_advance() cycles them (round1->level1 ... round8->level8, then
     * wrap) with the saturating per-stage difficulty ramp on top. The arcade's
     * single seamless LOOPING ribbon is re-expressed here as these 8 sceneries
     * chained by a brief transition; a future step can concatenate them into one
     * continuously-streamed ribbon (round_advance() TODO). See round_advance() /
     * level_select() / tools/LEVEL_END_SPEC.md. */
    return FALSE;
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
    /* SAT for the player (slot 0) + the entity pool (slots 1..) + the score-popup
     * glyph sprites. Sized for the player plus ENTITY_SAT_MAX entity bodies plus
     * ENTITY_POPUP_GLYPH_MAX popup glyphs; well under the 80-sprite hardware
     * ceiling. */
    static u16 sat[(1 + ENTITY_SAT_MAX + ENTITY_POPUP_GLYPH_MAX) * 4];
    const Player *p = &g_game.player;
    u16 nEnt;
    u16 totalSprites;

    /* The player is drawn in SCREEN space: world position minus the camera.
     * (The BG scrolls via VDP_setHorizontalScroll below; sprites are not
     * auto-scrolled by the VDP, so we offset them by the camera here.) */
    s16 px = (s16)(p->x - g_game.cameraX);
    s16 py = p->y;

    /* Stream any newly-revealed level columns into the plane window FIRST, so
     * the queued column DMAs flush in the same V-blank as the scroll update. */
    bg_stream();

    /* --- entity pool first: append visible entities at slots 1.. and get the
     * count back so we can chain the player's link to them (or terminate). --- */
    nEnt = entity_render_all(sat, 1);

    /* --- player sprite at slot 0 ---
     * Blink the player while he is in a transient life-cycle phase:
     *   - the brief on-contact hurt-flash (g_player_hurt), and
     *   - the death beat + respawn i-frame window (flow DYING / RESPAWNING),
     * by skipping the sprite on alternate frames. We blink off the player's own
     * flowTimer during DYING/RESPAWNING (so the whole death->respawn grace reads
     * as one continuous blink) and off g_player_hurt during the contact flash.
     * When blinked-off we still emit a (link-only) entry parked off-screen so the
     * sprite chain to the entities stays intact. */
    {
        u8 blink = 0;
        if (g_game.flow == FLOW_DYING || g_game.flow == FLOW_RESPAWNING)
            blink = (u8)((g_game.flowTimer & BLINK_MASK) != 0);
        else if (g_player_hurt)
            blink = (u8)((g_player_hurt & BLINK_MASK) != 0);

        if (blink)
        {
            sat[0] = (u16)((0x80 + 240) & 0x3FF);    /* parked off-screen (blink) */
            sat[3] = (u16)0x80;
        }
        else
        {
            sat[0] = (u16)((0x80 + py) & 0x3FF);
            sat[3] = (u16)((0x80 + px) & 0x1FF);
        }
    }
    /* link the player to the first entity sprite (slot 1) if any were emitted,
     * else terminate the chain at the player. */
    sat[1] = (u16)((SPRITE_SIZE(2, 2) << 8) | (nEnt ? 1 : 0));
    sat[2] = (u16)TILE_ATTR_FULL(PAL1, 0, 0, p->facing, p->tile);

    /* One DMA of the whole SAT (player + entities) to the hardware sprite list. */
    totalSprites = (u16)(1 + nEnt);
    DMA_queueDma(DMA_VRAM, sat, VDP_getSpriteListAddress(),
                 (u16)(totalSprites * 4), 2);

    /* BG horizontal scroll: shift Plane A left by the camera. The 64-col plane
     * is 512px wide and the HScroll register wraps mod 512, which matches the
     * ring layout (level col L at plane col L&63) -- so -cameraX aligns the
     * resident columns under the visible window for any cameraX. */
    VDP_setHorizontalScroll(BG_A, (s16)(-g_game.cameraX));

    /* --- HUD readout (top row, Plane B) ---
     * One compact row across the H32 (32-col) display, pinned on the unscrolled
     * Plane B with the PAL3 font palette; it shows through the blanked HUD band
     * (level rows 0-4 are transparent on Plane A). Fields, left to right, fit in
     * 32 columns:  RND nn (0..5) | FPS nn (7..12) | SC nnnnn (14..21) | Lxn (23..25).
     * SYS_getFPS() returns serviced frames/sec; ~60 = full speed. Labels are
     * shortened from the older layout to make room for the ROUND field without
     * overrunning the right edge. */
    {
        char buf[12];
        u32 fps = SYS_getFPS();

        /* ROUND number (HUD headline, left). 2-digit field so a two-digit round
         * never leaves a stale digit; round is 1-based and bumped per clear. */
        intToStr((s32)g_game.round, buf, 2);
        VDP_drawText("RND", 0, 1);
        VDP_drawText(buf, 4, 1);           /* cols 4..5                         */

        if (fps > 99) fps = 99;            /* keep it a tidy 2-digit field     */
        intToStr((s32)fps, buf, 2);
        VDP_drawText("FPS", 7, 1);
        VDP_drawText(buf, 11, 1);          /* cols 11..12                       */

        /* Score readout. Incremented by entity defeats / item pickups in
         * entity_update_all. */
        intToStr((s32)g_game.score, buf, 5);
        VDP_drawText("SC", 14, 1);
        VDP_drawText(buf, 17, 1);          /* 5 digits: cols 17..21             */

        /* --- LIVES readout (HUD, right) ---
         * Compact "Lx N" field (L = the hero/lives, "xN" = lives remaining),
         * chosen to fit the 32-col display alongside ROUND + FPS + SCORE without
         * overrunning the right edge. We redraw it every frame and pad the count
         * to a fixed width so a shrinking value never leaves a stale digit.
         * (lives is the count IN HAND incl. the current one, so it reads N while
         * N-1 are in reserve -- matches the arcade marker count incl. the hero.) */
        {
            char lv[4];
            lv[0] = 'x';
            lv[1] = (char)('0' + (g_game.lives <= 9 ? g_game.lives : 9));
            lv[2] = ' ';                   /* clear any prior 2nd digit/artifact */
            lv[3] = 0;
            VDP_drawText("L", 23, 1);      /* col 23                            */
            VDP_drawText(lv, 24, 1);       /* "xN" at cols 24..25               */
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
    if (g_game.flow == FLOW_GAME_OVER)
        VDP_drawText("GAME OVER", 11, 13);   /* H32: (32-9)/2 ~= 11 -> centered  */

    /* --- AREA-TRANSITION banner (Plane B, centered) ---
     * A brief (~0.6s) flash during the FLOW_ROUND_CLEAR scenery transition. SonSon
     * has no "round clear" (the scroll is continuous -- tools/LEVEL_END_SPEC.md), so
     * this is a light "entering the next area" cue while the next level's one-frame
     * cold-fill is masked -- faithful to the arcade's brief round/area intros, NOT a
     * finish-line banner. g_game.round is still the just-traversed pass during the
     * beat (round_advance bumps it at the end), so we show round+1 = the area being
     * entered. Blanked ONCE on the frame the beat ends (prevFlow tracks the edge) so
     * the text never lingers into the next area's play. */
    {
        static u8 prevFlow = FLOW_ALIVE;
        if (g_game.flow == FLOW_ROUND_CLEAR)
        {
            char buf[12];
            u16 nextRound = (u16)(g_game.round < 255 ? g_game.round + 1 : 255);
            intToStr((s32)nextRound, buf, 2);
            VDP_drawText("ROUND", 13, 13);        /* single centered "ROUND nn" line */
            VDP_drawText(buf, 19, 13);
        }
        else if (prevFlow == FLOW_ROUND_CLEAR)
        {
            VDP_clearText(13, 13, 8);             /* clear "ROUND nn" once           */
        }
        prevFlow = g_game.flow;
    }
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
    game_init();
    while (TRUE)
    {
        bool done = game_update();
        game_render();
        SYS_doVBlankProcess();
        if (done) break;           /* GAME OVER finished -> back to the selector */
    }

    /* AUDIO teardown: stop the music/SFX, silence the chips, and DETACH the
     * V-Blank replay tick so the front-end (selector / SEGA / CAPCOM splashes)
     * owns the sound hardware again on the way back.  Without this, snd_vblank
     * would keep firing during those stages and fight their direct YM use. */
    snd_shutdown();
}

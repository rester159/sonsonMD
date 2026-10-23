#include "arcade_bonus_life.h"
#include "arcade_pacing.h"
#include "arcade_demo_end.h"
#include "arcade_attract.h"
#include "arcade_bonus_art.h"
#include "arcade_food_art.h"
#include "arcade_food_world.h"
#include "arcade_release_art.h"
#include "arcade_food_bonus.h"
#include <genesis.h>
#include "arcade_ui.h"
#include "arcade_player.h"
#include "arcade_life.h"
#include "arcade_difficulty.h"
#include "arcade_scroll.h"
#include "arcade_section.h"
#include "arcade_finish.h"
#include "arcade_main_art.h"
#include "arcade_c_art.h"
#include "arcade_d_art.h"
#include "arcade_finish_art.h"
#include "arcade_results.h"
#include "arcade_results_display.h"
#include "arcade_cell_sat.h"
#include "game.h"
#include "mode_controls.h"
#include "frontend.h"
#include "survival.h"
#include "save.h"
#include "arcade_postgame.h"
#include "entity.h"
#include "ss_gfx.h"
#include "ss_level.h"
#include "ss_collision.h" /* per-column 6-tier terrain collision (native sub_A847) */
#include "ss_anim.h"     /* baked player/enemy animation frames + table */
#include "prof_hint.h"
#include "frame_budget.h"
#include "vram_words.h"
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
 *  (cameraX wrap + ribbon_rebase). The player keeps its screen-band pin and a baked
 *  ss_gfx sprite tile; the FPS readout stays on the unscrolled Plane B.
 * =========================================================================== */

GameState g_game;
static ArcadePlayer player_motion[MAX_PLAYERS];
static ArcadePlayerAnimation player_animation[MAX_PLAYERS];
static ArcadeLife player_life[MAX_PLAYERS];
static u8 life_owned[MAX_PLAYERS], life_path[MAX_PLAYERS];

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
volatile u8    g_attract_startPlayers = 0;

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
static u8 g_scrollColumnLatch=8,g_scrollColumnAdvance;
static ArcadeSection g_arcadeSection;
static u8 g_sectionGrade;
static ArcadeResults g_results;
static u8 g_resultsActive,g_resultsDisplay,g_resultsPlayers;
/* Both source stop states suppress scroll-dependent player/actor movement. */
u8 game_arcade_stopped(void){return !g_survivalRun && g_arcadeSection.stop!=0;}
/* Source007E: completed campaigns, distinct from combat/base difficulty. */
static u8 g_foodRound;
u8 game_arcade_column_advance(void){return g_scrollColumnAdvance;}

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
volatile u8 g_survivalChoice, g_survivalRun, g_survivalPressure;
volatile u32 g_survivalTicks, g_survivalSeconds;
static u16 survival_fraction, run_key;
static u8 run_started;
u8 game_player_started(u8 which){return (u8)(run_started&(1<<which));}
/* Preserve source active/kind during AD9B/AE99 ownership. The fallback
 * remains the legacy entry/game-over policy until those contracts are rebuilt. */
u16 game_player_target_flags(u8 which)
{
    if(life_owned[which])
        return (u16)(((u16)player_life[which].active<<8)|player_life[which].kind);
    /* Read flow once: both flag bytes use the same immutable call snapshot. */
    {
        const u8 flow=g_game.players[which].flow;
        u16 flags=(which<g_game.numPlayers && flow!=FLOW_GAME_OVER)?256:0;
        if(game_player_started(which) && (flow==FLOW_DYING || flow==FLOW_GAME_OVER))
            flags|=1;
        return flags;
    }
}

static u8 survival_step;
static u32 run_best_score, run_best_ticks, hud_survival_second;
/* Invalidated on every scene entry; text persists in Plane B between changes. */
static u8 ribbon_plane_phase;
static u8 hud_valid,hud_deferred;
#define HUD_LATE_LINE 200
static u32 hud_score[2], hud_high, hud_total;
static u16 hud_fps;
static u8 hud_round, hud_lives[2], hud_gameover, hud_credit_valid;
static u16 hud_credits;
volatile u32 g_presentCounter = 0;

/* Co-op GAME OVER beat: frames left in the final all-players-out overlay before
 * game_update returns to the front-end. 0 = not started. Set when every active
 * player has reached FLOW_GAME_OVER (in 1P that is just P1). */
static u16 g_gameOverBeat = 0;

static ArcadeAttract attract_input;
/* Translate the original input bits at the existing console-input boundary. */
static u16 attract_pad(u8 player)
{
    u8 input = attract_input.input[player];
    u16 pad = 0;
    if (input & 1) pad |= BUTTON_B;
    if (input & 4) pad |= BUTTON_LEFT;
    if (input & 8) pad |= BUTTON_RIGHT;
    if (input & 16) pad |= BUTTON_UP;
    if (input & 32) pad |= BUTTON_DOWN;
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
    0,            /* flipScreen = off                                           */
    3             /* Home continue credits                                      */
};

/* The player now ANIMATES: its sprite tile each frame comes from the baked
 * faithful animation set (ss_anim.c -- run/climb/fire/death), driven by the
 * Anim cursor p->av. PLAYER_TILE is just the cold-start frame (run frame 0). */
#define PLAYER_TILE   (ss_anims[SS_ANIM_PLAYER_RUN].frames[0].tile)

/* Source reserve counts can reach255 (256 lives including the current one).
 * Keep Player's existing byte ABI and distinguish that value from game-over0. */
/* GCC 16 O3/LTO widened bonusLife's byte load to AND.L at odd
 * g_config+1 during initialization, causing a 68000 address error.
 * Keep this packed configuration field's access explicitly byte-sized. */
static u8 bonus_life_dip(void)
{return *(volatile const u8 *)&g_config.bonusLife;}
static ArcadeBonusLife bonus_life[MAX_PLAYERS];
static u8 lives_256[MAX_PLAYERS];
static u8 player_reserves(const Player *p)
{return lives_256[p->which]?255:(p->lives?(u8)(p->lives-1):0);}
static void player_set_reserves(Player *p,u8 reserves)
{p->lives=(u8)(reserves+1);lives_256[p->which]=(reserves==255);}
/* EF78/FC2C: once per eight source ticks, alternating players. One award per
 * eligible check, after contacts/scheduling; no checks on results award ticks. */
static void check_extra_life(Player *p)
{
    u8 owner=p->which;
    if(g_survivalRun)return;
    if(arcade_bonus_life_step(&bonus_life[owner],bonus_life_dip(),
        entity_arcade_clock(),owner,!g_attract,p->flow==FLOW_GAME_OVER,p->score))
    {
        player_set_reserves(p,(u8)(player_reserves(p)+1));
        p->bonusNextIdx=bonus_life[owner].phase;
#if defined(SND_CMD_1UP) && defined(SND_1UP_ENABLE)
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
 *  ((rc + ribbon_plane_phase) & PLANE_COL_MASK). To reveal a column we wrap rc into [0..SS_RIBBON_COLS),
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
    u16 planeCol = (u16)((ribbonCol + ribbon_plane_phase) & PLANE_COL_MASK);
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

/* Refill the background at the current camera without touching player state.
 * Course restart uses this before the source9094 eligibility-based setup. */
static void level_refill(void)
{
    ribbon_plane_phase = 0;
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

}

/* New-game player setup, separate from course background restoration. */
static void level_restart(bool keepPose)
{
    level_refill();

    {
        Player *p = &g_game.players[0];
        /* The hero is always band-pinned, so re-pin his world X to the band's left
         * edge as the scroll resets to the new level's start (cameraX = 0 below). */
        p->x    = 32;
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
            p->y         = g_survivalRun ? SS_TIER_Y[PLAYER_START_TIER] : 135;
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
        q->x        = 32;
        q->xSub     = 0;
        q->vx       = 0;
        q->tier     = PLAYER_START_TIER+1;
        q->vstate   = PV_RUN;
        q->y        = g_survivalRun ? SS_TIER_Y[PLAYER_START_TIER+1] : 103;
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

/* Rebase logical coordinates without touching resident VRAM. For old column C
 * and ribbon length N, (C-N + phase+N)&63 == (C+phase)&63. The same phase is
 * included in horizontal scroll, so the visible plane and all queued columns
 * retain their physical slots across every full-ribbon wrap. */
static void ribbon_rebase(void)
{
    ribbon_plane_phase = (u8)((ribbon_plane_phase + SS_RIBBON_COLS) & PLANE_COL_MASK);
    g_game.loadedMinCol = (s16)(g_game.loadedMinCol - SS_RIBBON_COLS);
    g_game.loadedMaxCol = (s16)(g_game.loadedMaxCol - SS_RIBBON_COLS);
}

/* A689 running-scroll/state1 owner. Players consume the entry stop state;
 * world actors consume the resulting state later in this same update.
 * A730 advances the lookahead section, one screen ahead of visible scenery.
 * Ending state2 and its presentation remain under provisional campaign flow. */
void game_arcade_scroll_tick(void)
{
    u16 camera=(u16)g_game.cameraX;
    if(!g_survivalRun && g_arcadeSection.stop==2)
    {
        ArcadeFinish finish={g_arcadeSection.timer};ArcadeFinishEvent event;
        arcade_finish_step(&finish,entity_arcade_clock(),&event);
        g_arcadeSection.timer=finish.timer;
        if(event.write)arcade_finish_background_write(camera,ribbon_plane_phase,event.pattern);
        if(event.results && !g_resultsActive)
        {
            memset(&g_results,0,sizeof(g_results));g_results.course=g_foodRound;
            g_resultsActive=1;
        }
        return;
    }
    if(game_arcade_stopped())
    {
        ArcadeStop stop={g_arcadeSection.timer,g_sectionGrade};ArcadeStopEvent event;
        arcade_food_world_stop(&stop,camera,entity_arcade_clock(),&event);
        g_arcadeSection.timer=stop.timer;g_sectionGrade=stop.grade;
        if(event.release)
        {
            g_arcadeSection.stop=0;
            arcade_release_background_begin();
        }
        if(event.bonus)arcade_food_bonus_section(event.variant);
        /* A787 leaves008B unchanged, including the release update. */
        return;
    }
    g_scrollColumnAdvance=arcade_scroll_step(&camera,&g_scrollSub,&g_scrollColumnLatch);
    if(!g_survivalRun && g_scrollColumnAdvance)
    {
        ++g_arcadeSection.column;
        g_arcadeSection.difficulty=g_game.stage;
        g_arcadeSection.speed=g_arcadeCombatDifficulty;
        arcade_section_column(&g_arcadeSection,camera);
        g_game.stage=g_arcadeSection.difficulty;
        g_arcadeCombatDifficulty=g_arcadeSection.speed;
    }
    g_game.cameraX=(s16)camera;
    if (g_game.cameraX < 0) g_game.cameraX = 0;       /* ribbon-start left clamp */
}

/* ---------------------------------------------------------------------------
 *  game_init -- one-time scene + player setup.
 * ------------------------------------------------------------------------- */
static ArcadeDemoEnd demo_end;

void prof_start(void);
void game_init(void)
{
    u8 motion_index;
    VDP_setEnable(FALSE);
    DMA_flushQueue();
    DMA_waitCompletion();
    for(motion_index=0;motion_index<MAX_PLAYERS;motion_index++)
    {
        memset(&player_motion[motion_index],0,sizeof(ArcadePlayer));
        /* $9094 starts at state0; AA03 owns the first update. */
        player_motion[motion_index].state=g_survivalRun?1:0;
        memset(&player_animation[motion_index],0,sizeof(ArcadePlayerAnimation));
        player_animation[motion_index].owner=motion_index;
        player_animation[motion_index].attr=(u8)(0xc0|motion_index);
        life_owned[motion_index]=life_path[motion_index]=0;
    }
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
    arcade_gameplay_font_load();
    VDP_setTextPlane(BG_B);
    VDP_clearTextArea(0, 0, 40, 4);
    hud_valid = 0;
    hud_gameover = 0;
    hud_credits = 0;hud_credit_valid = 0;
    hud_survival_second = 0xFFFFFFFF;
    g_survivalRun = (u8)(g_mode == MODE_HOME && !g_attract && g_survivalChoice);
    g_survivalTicks = g_survivalSeconds = 0;
    g_survivalPressure = 0; survival_fraction = 0;
    survival_step = SYS_isPAL() ? 6 : 5;
    g_paused = 0;

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
    /* Exact main-enemy poses occupy retired scene tiles; food/barrier aliases survive. */
    VDP_loadTileData(arcade_main_pixels, ARCADE_MAIN_TILE_BASE, ARCADE_MAIN_TILE_COUNT, DMA);
    {u16 k;for(k=0;k<ARCADE_C_ART_CELLS;k++)VDP_loadTileData(arcade_c_pixels[k],arcade_c_art_slots[k],4,DMA); }
    /* Plane scrolling touches F000..F003; resident D art owns F020..F31F. */
    VDP_setScrollingMode(HSCROLL_PLANE,VSCROLL_PLANE);
    VDP_setHScrollTableAddress(0xF000);
    VDP_setSpriteListAddress(0xF400);
#if ARCADE_D_TILE_BASE * 32 < 0xF004 || (ARCADE_D_TILE_BASE + ARCADE_D_TILE_COUNT) * 32 > 0xF400
#error D graphics overlap plane-scroll words or sprite table
#endif
    {u16 k;for(k=0;k<ARCADE_D_ART_CELLS;k++)VDP_loadTileData(arcade_d_pixels[k],arcade_d_art_slots[k],4,DMA); }

#if ARCADE_BONUS_TILE_BASE < SS_SCENE_TILE_BASE + SS_SCENE_TILE_COUNT || ARCADE_BONUS_TILE_BASE + ARCADE_BONUS_TILE_COUNT > SS_ANIM_TILE_BASE
#error Bonus graphics overlap gameplay tile storage
#endif
    VDP_loadTileData(arcade_bonus_pixels, ARCADE_BONUS_TILE_BASE, ARCADE_BONUS_TILE_COUNT, DMA);
    /* Faithful player + enemy animation frames at SS_ANIM_TILE_BASE (346); the
     * baked anim table's per-frame tile indices are ABSOLUTE (include the base). */
    VDP_loadTileData(ss_anim_tiles, SS_ANIM_TILE_BASE, SS_ANIM_TILE_COUNT, DMA);
    /* ALL 8 ribbon segments' tilesets, co-resident -- each at its OWN VRAM base so
     * its map words (absolute) reference loaded tiles for the whole session. This
     * is what makes the level-to-level seams tear-free with no runtime DMA. */
    {
        u8 i;
        for (i = 0; i < SS_LEVEL_COUNT; i++)
            if(ss_levels[i].tileCount)VDP_loadTileData(ss_levels[i].tiles, ss_levels[i].tileBase,
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
    g_foodRound = 0; /* 8F68: a new game starts on bonus-grade zero. */
    /* 8F80..8F87: DIP difficulty applies only when live-game22 is set.
     * Scripted demos always begin at stage zero. */
    g_game.stage = g_attract ? 0 : (u8)(g_config.difficulty*4);
    g_arcadeCombatDifficulty=(u16)g_game.stage<<8;

    /* Original 9492 selects the two-player session for the scripted demo. */
    g_game.numPlayers = (u8)((g_attract || g_numPlayers >= 2) ? 2 : 1);
    arcade_attract_init(&attract_input);
    g_scrollSub = 0;                  /* reset the global auto-scroll accumulator    */
    g_scrollColumnLatch=8;g_scrollColumnAdvance=0; /* Original 8FE7 startup. */
    memset(&g_arcadeSection,0,sizeof(g_arcadeSection));g_sectionGrade=0;
    g_resultsActive=g_resultsDisplay=g_resultsPlayers=0;
    /* 900B initial descriptor0 has one screen already preloaded. */
    arcade_section_course_begin(&g_arcadeSection,g_game.stage);

    /* Build the ribbon window + place the player at the start (level_restart cold-
     * fills Plane A from the first 64 ribbon columns, resets the stream range +
     * camera, and spawns the hero low-left). FALSE = full cold-start pose. */
    level_restart(FALSE);
    if(!g_survivalRun)
    {
        arcade_food_art_load();
        arcade_release_background_reset();
        arcade_finish_background_reset();
        arcade_food_world_begin();
        arcade_food_bonus_reset();
        arcade_food_world_render(ribbon_plane_phase,1);
    }

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
            pp->lives        = g_survivalRun ? 1 : g_config.lives;
            pp->bonusNextIdx = 0;lives_256[i]=0;
            arcade_bonus_life_begin(&bonus_life[i],bonus_life_dip());
            pp->flow         = FLOW_ALIVE;
            pp->flowTimer    = 0;
            pp->deadReq      = 0;
            pp->invuln       = 0;
            pp->hurt         = 0;
        }
    }
    if (!g_attract && g_mode == MODE_HOME) g_homeCredits = g_survivalRun ? 0 : g_config.homeCredits;
    if (!g_attract && g_mode == MODE_ARCADE)
    {
        u8 i;
        for (i = 0; i < MAX_PLAYERS; i++)
            if (!(g_startPlayers & (1 << i)))
            {
                g_game.players[i].lives = 0;
                g_game.players[i].flow = FLOW_GAME_OVER;
                g_game.players[i].invuln = 1;
            }
    }
    g_gameOverBeat = 0;
    demo_end.phase=demo_end.timer=0;
    run_started = (g_mode == MODE_ARCADE && !g_attract) ? g_startPlayers : (g_game.numPlayers == 2 ? 3 : 1);
    run_key = save_run_key(g_mode,run_started == 3 ? 2 : 1,g_survivalRun);
    run_best_score = save_best_score(run_key);
    run_best_ticks = save_best_ticks(run_key);

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
    VDP_setEnable(TRUE);
    prof_start();   /* diagnostic builds only (-DPROF_HINT); empty otherwise */
}

/* Survival's legacy placement. Arcade/Home Start admission below enters the
 * verified source life state6 instead; ordinary deaths own their own path. */
static void player_respawn(Player *p)
{
    life_owned[p->which]=0;
    memset(&player_animation[p->which],0,sizeof(ArcadePlayerAnimation));
    player_animation[p->which].owner=p->which;
    player_animation[p->which].attr=(u8)(0xc0|p->which);
    memset(&player_motion[p->which],0,sizeof(ArcadePlayer));
    player_motion[p->which].state=1;
    p->tier      = PLAYER_START_TIER;               /* back on the middle tier   */
    p->vstate    = PV_RUN;
    p->y         = SS_TIER_Y[PLAYER_START_TIER];    /* seated on the tier        */
    p->ySub      = 0;
    p->vy        = 0;
    /* Legacy console placement; the source $9094 startup instead uses
     * camera+32 and upward Y104/Y136 for the two owners. */
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
    if(g_survivalRun)g_arcadeCombatDifficulty=arcade_difficulty_seam(&g_game.stage,g_arcadeCombatDifficulty);

    /* Wave controllers continue across scenery seams; only coarse scroll arms them. */

    /* Boundary music cue (arcade segment-boundary flourish: 0x16 then 0x10). */
    if(ss_segment_event[newSegIdx])snd_play_round();
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
    u16 rc;
    if(tier>=SS_COLLISION_TIERS)return 0;
    rc=ribbon_wrap_col((s16)(worldX>>3));
    return (u8)((ss_ribbon_collision[rc]>>tier)&1);

}

/* The hero's collision column uses his sprite mid-X (+8), like A847 (worldX+8). */
static u8 tier_solid_at_hero(const Player *p, u8 tier)
{
    return tier_solid((s16)(p->x + 8), tier);
}

/* Typed implementation of the original $A9A7..$AD0F player physics.
 * Source Y grows upwards; displayed sprite Y is its byte complement minus 16. */
s16 game_arcade_floor(u16 x,u8 y,void *unused)
{
    u8 band=(u8)(y>>5), low=(u8)(y&31);
    (void)unused;
    if(band<1 || band>6 || low<2 || low>7)return ARCADE_NO_FLOOR;
    /* $A847 rounds in camera-relative space before adding the ring column.
     * Rounding world X directly shifts gap boundaries on seven of eight phases. */
    {
        u16 camera=(u16)g_game.cameraX;
        s16 probe=(s16)((camera&0xFFF8)+((u16)(x+8-camera)&0xFFF8));
        if(!tier_solid(probe,(u8)(band-1)))return ARCADE_NO_FLOOR;
    }
    return (s16)(7-(y&15));
}
static void motion_load(u8 k)
{
    Player *p=&g_game.players[k];ArcadePlayer *m=&player_motion[k];
    m->x=(u16)p->x;m->xf=(u8)p->xSub;
    m->y=(u8)(239-p->y);m->yf=(u8)p->ySub;
    m->facing=p->facing?3:1;
    m->active=(u8)(game_player_target_flags(k)>>8);
}
static void motion_store(u8 k)
{
    Player *p=&g_game.players[k];const ArcadePlayer *m=&player_motion[k];
    p->x=(s16)m->x;p->xSub=m->xf;
    p->y=(s16)((u8)(239-m->y));
    if(p->y>=240)p->y-=256;
    p->ySub=m->yf;p->facing=(u8)(m->facing==3);
    p->climbing=(u8)(m->state!=1);
    p->vstate=m->state==1?PV_RUN:m->state==2?PV_CLIMB_UP:m->state==3?PV_CLIMB_DOWN:PV_FALL;
    if(m->y>=32 && m->y<224)p->tier=(u8)((m->y>>5)-1);
}
/* AD9B owns ordinary death until AE99 releases kind1. Input history is
 * sampled even during the locked portion, so held buttons do not become edges. */
static void player_life_begin(Player *p)
{
    u8 k=p->which;ArcadeLife *l=&player_life[k];
    memset(l,0,sizeof(*l));l->x=(u16)p->x;l->xf=(u8)p->xSub;
    l->y=(u8)(239-p->y);l->yf=(u8)p->ySub;l->owner=k;
    l->active=1;l->kind=1;l->facing=p->facing?3:1;
    life_owned[k]=1;life_path[k]=0;p->flow=FLOW_DYING;p->invuln=1;
    p->hurt=p->fireAnim=0;
}
static void player_control(Player *p,u16 pad);
static void player_life_update(Player *p,u16 pad)
{
    u8 k=p->which,input=0,fire,ready=0;ArcadeLife *l=&player_life[k];
    ArcadePlayer *m=&player_motion[k];ArcadeFirePose pose;
    /* A98A still visits out P1, but skips out P2 through flag89. Preserve
     * the source record after final life instead of inventing inactive/kind1. */
    if(life_owned[k]==2)
    {
        if(k==0 && m->active)
        {
            player_control(p,pad);
            l->x=m->x;l->xf=m->xf;l->y=m->y;l->yf=m->yf;
            l->state=m->state;l->phase=m->phase;l->active=m->active;
            l->cursor=(u8)(8+k*4+player_animation[k].cursor);
            l->anim_timer=player_animation[k].timer;
            l->code=player_animation[k].code;l->attr=player_animation[k].attr;
        }
        return;
    }
    if(pad&BUTTON_LEFT)input|=4;
    if(pad&BUTTON_RIGHT)input|=8;
    if(pad&BUTTON_UP)input|=16;
    if(pad&BUTTON_DOWN)input|=32;
    fire=(u8)((pad&(BUTTON_A|BUTTON_B|(g_mode==MODE_HOME?BUTTON_C:0)))!=0);
    if(l->state==7)
        ready=arcade_life_reentry(l,&life_path[k],entity_arcade_clock(),game_arcade_stopped(),input,m->previous,&pose);
    else
    {
        u8 reserves=player_reserves(p),gate=0,sound;
        u16 event;
        arcade_life_step(l,(u16)g_game.cameraX,game_arcade_stopped(),&reserves,&gate,&event,&sound);
        if(sound)snd_command(sound);
        if(event==(u16)(0xc00|k))player_set_reserves(p,reserves);
        if(gate)
        {
            ArcadePlayerAnimation *a=&player_animation[k];
            p->lives=0;lives_256[k]=0;p->flow=FLOW_GAME_OVER;life_owned[k]=2;
            a->owner=k;a->cursor=(u8)(l->cursor&3);a->timer=l->anim_timer;
            a->code=l->code;a->attr=l->attr;
        }
    }
    if(ready && fire && !p->firePrev &&
       entity_player_fire(k,(s16)pose.x,(s16)(239-pose.y),pose.facing))snd_command(0x08);
    p->firePrev=fire;m->previous=input;
    m->x=l->x;m->xf=l->xf;m->y=l->y;m->yf=l->yf;
    m->state=l->state;m->phase=l->phase;m->facing=l->facing;m->active=l->active;
    m->attached=0;motion_store(k);
    p->tile=arcade_life_tile(l->code,l->attr);
    p->facing=(u8)((l->attr&64)==0);
    if(p->flow!=FLOW_GAME_OVER)
    {
        p->flow=l->kind?(l->state==7?FLOW_RESPAWNING:FLOW_DYING):FLOW_ALIVE;
        p->invuln=(u8)(l->kind!=0);
        if(!l->kind)
        {
            ArcadePlayerAnimation *a=&player_animation[k];
            a->owner=k;a->cursor=(u8)(l->cursor&3);a->timer=l->anim_timer;
            a->code=l->code;a->attr=l->attr;life_owned[k]=0;
        }
    }
}
static void player_control(Player *p,u16 pad)
{
    u8 k=p->which, other=(u8)(k^1),input=0;
    ArcadeFirePose fire_pose;
    if(p->flow!=FLOW_ALIVE && p->flow!=FLOW_RESPAWNING && life_owned[k]!=2)return;
    if(pad&BUTTON_LEFT)input|=4;
    if(pad&BUTTON_RIGHT)input|=8;
    if(pad&BUTTON_UP)input|=16;
    if(pad&BUTTON_DOWN)input|=32;
    if(life_owned[k]!=2)motion_load(k);
    if(g_game.numPlayers>1)motion_load(other);
    arcade_player_step_stopped(&player_motion[k],g_game.numPlayers>1?&player_motion[other]:0,
                              input,(u16)g_game.cameraX,game_arcade_stopped(),game_arcade_floor,0,&fire_pose,g_survivalRun?0:&player_animation[k]);
    motion_store(k);
    if(g_game.numPlayers>1 && !life_owned[other])motion_store(other);
    {
        u8 fire = (u8)((pad & (BUTTON_A | BUTTON_B | (g_mode == MODE_HOME ? BUTTON_C : 0))) != 0);
        if (fire && !p->firePrev)
        {
            /* Original $AD10 launch pose is captured before the $A9C5 drift.
             * Each owner has two slots; an occupied end puff still holds a slot. */
            if (entity_player_fire(p->which, (s16)fire_pose.x, (s16)(239-fire_pose.y),
                                   g_survivalRun?p->facing:fire_pose.facing))
            {
                /* Original AD10 calls6424 (command08) and does not change
                 * the parent animation. Survival keeps its custom recoil. */
                if(g_survivalRun)snd_sfx_fire();else snd_command(0x08);
                /* show the fire/recoil pose for the anim's full length (per-player
                 * anim set: TonTon uses SS_ANIM_PLAYER2_FIRE). */
                if(g_survivalRun)
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
    if(!g_survivalRun)
    {
        const ArcadePlayerAnimation *a=&player_animation[p->which];
        p->tile=arcade_life_tile(a->code,a->attr);
        p->facing=(u8)((a->attr&64)==0);return;
    }
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

    if (dy < -7 || dy > 8)  return;             /* vertical gap too big (±8 -> 16px) */
    dx = (s16)(p1->x - p2->x);
    if (dx < -12 || dx > 11) return;            /* horizontal gap too big (±12->24px)*/

    if (dx < 0) { adx = (s16)(-dx); hi = p2; lo = p1; }  /* p1 left -> p2 is right   */
    else        { adx = dx;         hi = p1; lo = p2; }
    half = (s16)((12 - adx) >> 1);
    hi->x = (s16)(hi->x + half);  /* push the right player right       */
    lo->x = (s16)(lo->x - half);  /* push the left player left         */
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
/* Called by the alternating E2B3-style contact dispatcher. The update-entry
 * clock is unchanged throughout player, food, enemy and scheduler work. */
void game_arcade_food_contact(u8 owner)
{
    Player *p=&g_game.players[owner];u16 points;
    if(life_owned[owner] || p->flow!=FLOW_ALIVE || player_motion[owner].active!=1)return;
    points=arcade_food_world_contact((u16)g_game.cameraX,(u16)p->x,
        (u8)(239-p->y),1,owner,g_foodRound);
    if(points){game_score_add(p,points);snd_command(4);}
    /* E569 caches the entry position for food and barrier tests. Food does
     * not move the player, so this retains that snapshot and stable pushes. */
    player_motion[owner].x=(u16)p->x;
    player_motion[owner].y=(u8)(239-p->y);
    if(arcade_food_world_player(&player_motion[owner],(u16)g_game.cameraX))
        p->deadReq=1; /* Existing life owner takes kind1 after this contact pass. */
    p->x=(s16)player_motion[owner].x;
}
void game_arcade_special_contact(u8 owner)
{
    Player *p=&g_game.players[owner];ArcadeBonusAward award;
    if(life_owned[owner] || p->flow!=FLOW_ALIVE || player_motion[owner].active!=1)return;
    if(arcade_food_bonus_contact((u16)p->x,(u8)(239-p->y),owner,g_arcadeCombatDifficulty,&award))
    {game_score_add(p,award.points);g_arcadeCombatDifficulty=award.combat;snd_command(award.sound);}
    entity_arcade_conversion_contact((u16)p->x,(u8)(239-p->y),owner);
}

static void game_course_restart(void)
{
    u8 k,out[2];
    for(k=0;k<2;k++){out[k]=g_game.players[k].flow==FLOW_GAME_OVER;motion_load(k);}
    VDP_setEnable(FALSE);DMA_flushQueue();
    arcade_results_display_restore();
    g_game.cameraX=0;level_refill();
    g_scrollSub=0;g_scrollColumnLatch=8;g_scrollColumnAdvance=0;
    g_arcadeSection.speed=g_arcadeCombatDifficulty;
    arcade_section_course_begin(&g_arcadeSection,g_game.stage);
    arcade_players_course_begin(player_motion,player_animation,0,g_game.numPlayers>1,out);
    for(k=0;k<2;k++)if(!out[k] && (k==0 || g_game.numPlayers>1))
    {
        Player *p=&g_game.players[k];life_owned[k]=life_path[k]=0;
        motion_store(k);p->flow=FLOW_ALIVE;p->flowTimer=0;
        p->deadReq=p->hurt=p->invuln=0;
    }
    entity_course_restart(g_game.numPlayers>1,out);arcade_food_world_begin();arcade_food_bonus_reset();
    arcade_release_background_reset();arcade_food_world_render(ribbon_plane_phase,1);
    arcade_finish_background_reset();
    g_foodRound=g_results.course;g_game.round=1;g_gameOverBeat=0;
    g_resultsActive=g_resultsDisplay=g_resultsPlayers=0;hud_valid=0;
    snd_command(0x12);VDP_setEnable(TRUE);
}
/* Commit a prepared display. SYS_doVBlankProcess waits for the NEXT VBlank
 * start whenever it is entered during VBlank, so a frame finishing a few
 * lines late lost a whole refresh even though most of this VBlank remained.
 * On NTSC, flush now while enough VBlank lines remain for the queued DMA
 * (H32 moves well over 128 bytes per blank line; four lines spare). PAL's
 * wrapped V-counter is ambiguous and its 89 blank lines rarely need this. */
static void game_present(void)
{
    PROF_SET(0,GET_VCOUNTER);   /* diagnostic builds: line where frame work ended */
    if(!IS_PAL_SYSTEM && GET_VDP_STATUS(VDP_VBLANK_FLAG))
    {
        u16 v=(u16)GET_VCOUNTER,line;
        /* 224..234 read E0..EA, then 235..261 read E5..FF: assume the later. */
        if(v>=0xE0)
        {
            line=v>=0xE5?(u16)(v-0xE5+235):v;
            if(line+4+(DMA_getQueueTransferSize()>>7)<262)
            {
                SYS_doVBlankProcessEx(ON_VBLANK);
                vram_words_flush();
                return;
            }
        }
    }
    SYS_doVBlankProcess();
    vram_words_flush();   /* map overlays after this VBlank's queued DMA */
}
/* Display sampling retains button presses until the next source update. */
static ArcadePacing home_pacing;
static ArcadePacing *gameplay_pacing;
static u16 gameplay_held[2],gameplay_pressed[2],gameplay_pad[2];
static u8 gameplay_input_ready;
static void gameplay_input_sample(u8 discard)
{
    u8 k;
    for(k=0;k<2;k++)
    {
        u16 pad=JOY_readJoypad(k?JOY_2:JOY_1);
        if(discard)gameplay_pressed[k]=0;
        else gameplay_pressed[k]|=(u16)(pad&~gameplay_held[k]);
        gameplay_held[k]=pad;
    }
}
static void gameplay_input_consume(void)
{
    u8 k;
    for(k=0;k<2;k++)
    {gameplay_pad[k]=gameplay_held[k]|gameplay_pressed[k];gameplay_pressed[k]=0;}
}
/* Returns true when results own this update instead of normal gameplay. */
static u8 game_results_update(void)
{
    ArcadeResultsInput input;ArcadeResultsEvent event;u8 k;
    input.clock=entity_arcade_clock();input.screen_busy=g_resultsDisplay && arcade_results_display_pending();
    input.p1_out=g_game.players[0].flow==FLOW_GAME_OVER;
    input.p2_out=g_game.players[1].flow==FLOW_GAME_OVER;
    input.two_players=g_game.numPlayers>1;input.marker=0;
    arcade_results_step(&g_results,&input,&event);
    if(event.flags&ARCADE_RESULTS_PLAY)return 0;
    if(event.flags&ARCADE_RESULTS_CLEAR)
    {
        VDP_setEnable(FALSE);DMA_flushQueue();
        arcade_results_display_begin();g_resultsDisplay=1;g_resultsPlayers=0;
        VDP_setHorizontalScroll(BG_A,0);VDP_setEnable(TRUE);
    }
    if(event.flags&ARCADE_RESULTS_PLAYER)g_resultsPlayers|=event.player_mask;
    if(event.flags&ARCADE_RESULTS_ANIMATE)
        for(k=0;k<2;k++)if(g_resultsPlayers&(1<<k))
        {
            if(life_owned[k]==1)arcade_life_animation_step(&player_life[k]);
            else arcade_player_animation_step(&player_animation[k]);
        }
    if(g_resultsDisplay)arcade_results_display_event(&event);
    if((event.flags&ARCADE_RESULTS_AWARD) && !g_attract)
        for(k=0;k<2;k++)if(event.points[k])
        {game_score_add(&g_game.players[k],event.points[k]);}
    if(event.flags&ARCADE_RESULTS_RESTART)game_course_restart();
    return 1;
}
static u8 game_arcade_frozen(void)
{
    return g_mode == MODE_ARCADE &&
        !(*(volatile u8 *)&g_config.dsw2 & 0x80);
}

bool game_update(void)
{
    g_frameCounter++;          /* headless FPS probe tick (see g_frameCounter) */
#ifdef REPLAY_EXACT_INPUT
    /* Diagnostic builds only: inputs and refill indexed by simulation tick,
     * so ROMs with different presentation timing replay identical play. */
    if(!g_attract && gameplay_input_ready){
        static u32 replay_step;
        u32 step=replay_step++;
        u16 pad=BUTTON_RIGHT | (step%16<3?BUTTON_A:0) | (step%200<24?BUTTON_UP:0) | (step%200>=24 && step%200<48?BUTTON_DOWN:0);
        gameplay_pad[0]=pad;gameplay_pad[1]=g_game.numPlayers>1?pad:0;
        g_game.players[0].lives=99;
        if(g_game.numPlayers>1)g_game.players[1].lives=99;
    }
#endif
    /* Original IRQ9112 advances23:24 before dispatch, including results.
     * Home pause bypasses game_update; Survival owns its separate timer. */
    if(!g_survivalRun)entity_arcade_clock_advance();
    /* Original IRQ9130 gates mode dispatch, after incrementing23:24.
     * Unlike Home pause, Freeze does not stop the source clock or audio. */
    if(game_arcade_frozen())return FALSE;
    if(g_attract && demo_end.phase)
    {
        u8 flags=arcade_demo_end_step(&demo_end,entity_arcade_clock());
        /* Original6452/6466 send commands10/14 through6341. */
        if(flags&ARCADE_DEMO_END_SOUND){snd_command(0x10);snd_command(0x14);}
        return (flags&ARCADE_DEMO_END_DONE)!=0;
    }
    /* One shared input dispatch per source update, before either player.
     * Boot/prelude/setup clock phase still needs lifecycle integration. */
    if(g_attract && !arcade_attract_step(&attract_input,entity_arcade_clock()))
        return TRUE;
    if(g_resultsActive && game_results_update())return FALSE;
    if (g_survivalRun)
    {
        u8 i, alive = 0;
        for(i=0;i<g_game.numPlayers;i++)
            if(g_game.players[i].flow == FLOW_ALIVE) alive = 1;
        if(alive)
        {
            g_survivalTicks += survival_step;
            survival_fraction += survival_step;
            if(survival_fraction >= 300)
            {
                survival_fraction -= 300; g_survivalSeconds++;
                if(g_survivalSeconds % 20 == 0 && g_survivalPressure < 6)
                    g_survivalPressure++;
            }
            g_game.stage = (u8)(g_survivalPressure * 4);
        }
    }

    /* GLOBAL auto-scroll: advance the camera by the constant forward creep BEFORE the
     * players move/clamp, so each player's band clamp this frame sees the new camera.
     * This is the arcade's player-independent scroll (sub_A697); the camera no longer
     * follows any one player (that is what lets co-op players share the screen). */

    /* Player control + anim, for EACH active player (co-op: P1=JOY_1, P2=JOY_2).
     * Live only while ALIVE or RESPAWNING (players move during the i-frames);
     * The attract demo supplies the original independent inputs for both players. */
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
            if (pp->flow != FLOW_GAME_OVER || (i==0 && life_owned[i]==2))
                pad = g_attract ? attract_pad(i)
                                : (gameplay_input_ready?gameplay_pad[i]:JOY_readJoypad(i == 0 ? JOY_1 : JOY_2));
            if(life_owned[i])player_life_update(pp,pad);
            else
            {
                player_control(pp, pad);
                player_animate(pp, (u8)(pp->flow == FLOW_DYING));
            }
        }
        /* 2-player overlap resolve (arcade sub_A98A $A945): if the two players are
         * within ~12px H and ~16px V, push them apart horizontally. (Skip when
         * either is out -- an out player isn't on the field.) */
        if (g_game.numPlayers > 1 && !life_owned[0] && !life_owned[1] &&
            g_game.players[0].flow != FLOW_GAME_OVER &&
            g_game.players[1].flow != FLOW_GAME_OVER)
            players_separate();
    }

    /* Original $A5E2 order: players, then world scroll, then projectiles/enemies. */
    game_arcade_scroll_tick();
    /* A5EE runs B36F before enemy updates and E2E0/E334 food contacts. */
    if(!g_survivalRun)
    {
        /* A893 follows A689, including phase0 on the stop-release update. */
        arcade_release_background_step((u16)g_game.cameraX,entity_arcade_clock(),ribbon_plane_phase);
        arcade_food_world_step_clock((u16)g_game.cameraX,g_scrollColumnAdvance,entity_arcade_clock());
        arcade_food_bonus_step((u16)g_game.cameraX,game_arcade_stopped(),entity_arcade_sprite_ready(),g_scrollColumnAdvance);
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
     *     Keep the ring's physical slots and scroll phase while rebasing its
     *     logical resident range; no full-plane transfer is needed. */
    if (g_ribbonW > 0 && g_game.cameraX >= g_ribbonW)
    {
        s16 dx = (s16)(-g_ribbonW);
        u8  i;
        g_game.cameraX     = (s16)(g_game.cameraX + dx);
        for (i = 0; i < g_game.numPlayers; i++)
        {
            g_game.players[i].x = (s16)(g_game.players[i].x + dx);
            if(life_owned[i])player_life[i].x=(u16)(player_life[i].x+dx);
        }
        entity_shift_worldx(dx);                 /* shift all entities + spawn cursor */
        ribbon_rebase();                         /* preserve resident plane, no transfer */
        if(!g_survivalRun)
        {
            ++g_foodRound; /* 9A18 increments007E at campaign completion. */
            arcade_food_world_begin();
            arcade_food_bonus_reset();
        }
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
            if(!g_survivalRun && pp->flow==FLOW_ALIVE && pp->deadReq)
                player_life_begin(pp);
            if(!life_owned[i])switch (pp->flow)
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
                        player_respawn(pp);        /* legacy Survival placement */
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
        if (allOut && !g_resultsActive)
        {
            if(g_attract)
            {
                demo_end.phase=1;
                return FALSE;
            }
            if (g_gameOverBeat == 0)
                g_gameOverBeat = (!g_attract && (g_mode == MODE_ARCADE || g_homeCredits))
                                 ? 600 : GAMEOVER_FRAMES;
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
     * tear-free via ribbon_rebase). The per-segment bookkeeping (HUD round, saturating
     * difficulty stage, wave re-arm, music cue) fires the moment a seam scrolls in
     * (segment tracking above) -- faithful to the arcade's one looping ribbon whose
     * saturating $005A stage ramps difficulty as you travel (tools/LEVEL_END_SPEC.md).
     * See stream_column / ribbon_locate / segment_advance / ribbon_rebase. */
    return FALSE;
}

/* ---------------------------------------------------------------------------
 *  bg_stream -- keep the RIBBON resident in the 64-col Plane A window.
 *
 *  The 64-col plane is a ring buffer over the ribbon: stream column S lives at
 *  plane column (S + ribbon_plane_phase) & PLANE_COL_MASK (S is a MONOTONIC counter in the camera frame --
 *  it is NOT wrapped to the ribbon width here; stream_column wraps the SOURCE column
 *  by SS_RIBBON_COLS internally, so the ribbon's own scenery loop -- last segment
 *  back to the first -- is seamless regardless of the ring phase). We keep every
 *  column the visible 256px window can show resident: the window shows columns
 *  [cameraX/8 .. cameraX/8 + 32]; the 64-wide plane gives 32 cols of slack, so we
 *  extend the loaded range a lead column ahead of the right edge. As the camera
 *  advances past a tile boundary we stream the newly-needed column(s) -- typically
 *  0 or 1 per frame (the forward creep is < 1 tile/frame), so streaming touches just
 *  the new seam column, never a full repaint -> 60fps. (Once per full ribbon loop,
 *  ribbon_rebase preserves the resident window and its physical ring phase.)
 *
 *  SEAM SAFETY: a freshly-streamed column S overwrites plane column (S + ribbon_plane_phase) & 63, which
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
/* Source4EBE writes eight digits then blanks at most seven leading zeroes.
 * Two four-digit chunks keep valid source scores within 68000 DIVU range. */
static void hud_score_string(u32 score,char *text)
{
    u8 i;
    if(score>=100000000UL)score%=100000000UL;
    if(score<=10000)uintToStr((u16)score,text,8);
    else
    {
        u32 parts=divmodu(score,10000);
        uintToStr((u16)parts,text,4);
        uintToStr((u16)(parts>>16),text+4,4);
    }
    for(i=0;i<7 && text[i]=='0';i++)text[i]=' ';
}

/* Append players at their source priority position; caller terminates chain. */
u16 game_render_players(u16 *sat,u16 first,u16 limit)
{
    u8 k,np=g_game.numPlayers;u16 slot=first;
    for (k = 0; k < np; k++)
    {
        if(!g_survivalRun)
        {
            u8 active=(u8)(game_player_target_flags(k)>>8);
            if(!active || (active&128))continue;
        }
        if(slot>=limit)break;
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
        else if (!life_owned[k] && (pp->flow == FLOW_DYING || pp->flow == FLOW_RESPAWNING))
            blink = (u8)((pp->flowTimer & BLINK_MASK) != 0);
        else if (pp->hurt)
            blink = (u8)((pp->hurt & BLINK_MASK) != 0);

        if (blink)
        {
            sat[slot * 4 + 0] = (u16)((0x80 + 240) & 0x3FF);   /* parked off-screen */
            sat[slot * 4 + 3] = (u16)0x80;
        }
        else
        {
            sat[slot * 4 + 0] = (u16)((0x80 + py) & 0x3FF);
            sat[slot * 4 + 3] = (u16)((0x80 + px) & 0x1FF);
        }

        /* Caller appends the next source-priority actor and terminates the tail. */
        nextLink=(u16)(slot+1);
        sat[slot * 4 + 1] = (u16)((SPRITE_SIZE(2, 2) << 8) | nextLink);
        sat[slot * 4 + 2] = (u16)TILE_ATTR_FULL(PAL1, 0, 0, pp->facing, pp->tile);
        slot++;
    }

    return (u16)(slot-first);
}

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

    if(g_resultsDisplay)
    {
        static const ArcadeSpriteView view={0,1,256,224};u16 count=0;
        arcade_results_display_background();
        for(k=0;k<2;k++)if(g_resultsPlayers&(1<<k))
        {
            ArcadeLargeCell cell;u16 tile;
            cell.x=32;cell.y=(u8)(-(k?96:160)-16);
            if(life_owned[k]==1){cell.code=player_life[k].code;cell.attr=player_life[k].attr;}
            else{cell.code=player_animation[k].code;cell.attr=player_animation[k].attr;}
            tile=arcade_life_tile(cell.code,cell.attr);
            count+=arcade_cell_sat(&cell,tile,&view,sat,count,80);
        }
        count+=arcade_results_display_sat(sat,count,80);
        if(count)sat[(count-1)*4+1]&=0xff00;
        else{memset(sat,0,8);count=1;}
        DMA_queueDma(DMA_VRAM,sat,VDP_getSpriteListAddress(),count*4,2);
        VDP_setHorizontalScroll(BG_A,0);
        return;
    }

    /* Stream any newly-revealed level columns into the plane window FIRST, so
     * the queued column DMAs flush in the same V-blank as the scroll update. */
    bg_stream();
    if(!g_survivalRun)
    {
        arcade_release_background_render();
        arcade_finish_background_render();
        arcade_food_world_render(ribbon_plane_phase,0);
        arcade_food_bonus_background(ribbon_plane_phase);
    }

    if(g_survivalRun)
    {
        nEnt=entity_render_all(sat,np);
        game_render_players(sat,0,np);
        totalSprites=(u16)(np+nEnt);
    }
    else totalSprites=entity_render_all(sat,0);
    if(totalSprites)sat[(totalSprites-1)*4+1]&=0xff00;
    else{memset(sat,0,8);totalSprites=1;}

    DMA_queueDma(DMA_VRAM, sat, VDP_getSpriteListAddress(),
                 (u16)(totalSprites * 4), 2);

    /* BG horizontal scroll: shift Plane A left by the camera. The 64-col plane
     * is 512px wide and the HScroll register wraps mod 512, which matches the
     * ring layout (level col L at plane col (L+phase)&63) -- so -(cameraX+phase*8) aligns the
     * resident columns under the visible window for any cameraX. */
    /* Commit with the SAT and streamed columns in VBlank. A direct VRAM write
     * here landed mid-display, tearing Plane A against the sprites. */
    VDP_setHorizontalScrollVSync(BG_A, (s16)(-g_game.cameraX - (u16)ribbon_plane_phase * 8));

    /* --- CO-OP HUD readout (Plane B, the transparent top band; H32 = 32 cols) ---
     * Faithful to the arcade's two-player score header (RE'd from the machine's
     * own display test: "SONSON ... HIGH ... TONTON"): P1's score under SONSON on
     * the left, the high score under HIGH in the middle, P2's (TonTon's) score on
     * the right -- each player keeps its OWN score (the whole point of the co-op
     * split). TONTON's column shows only in a 2-player game.
     * Source4EBE writes eight cells, with leading blanks: P1 at2, HIGH at13,
     * P2 at23. Source row2 maps to display row1. Lives and console credits
     * retain their existing row2 layout pending their separate parity pass.
     * Level Plane A's header band is transparent. */
    /* Score fields are cosmetic and redraw from current values, so a late
     * frame may leave them for the next refresh (at most eight in a row).
     * Two-player kills can reformat three eight-digit fields at once. */
    if (hud_valid && hud_deferred < 8 && frame_past_line(HUD_LATE_LINE))
        hud_deferred++;
    else
    {
        char buf[12];

        u32 hi = g_game.players[0].score;
        hud_deferred = 0;
        if (hi < run_best_score) hi = run_best_score;
        if (np > 1 && g_game.players[1].score > hi) hi = g_game.players[1].score;
        if (hi < 10000) hi = 10000;


        /* Labels never change during a session. Numeric fields are formatted
         * and written only when their value changes, avoiding active-display
         * VDP waits and decimal division on every gameplay tick. */
        if (!hud_valid)
        {
            arcade_gameplay_labels(np>1,g_survivalRun);


        }
        for (k = 0; k < np; k++)
        {
            const Player *p = &g_game.players[k];
            if (!hud_valid || hud_score[k] != p->score)
            {
                hud_score_string(p->score,buf);
                VDP_drawTextEx(BG_B,buf,TILE_ATTR_FULL(PAL2,1,0,0,0),k?23:2,1,CPU);
                hud_score[k] = p->score;
            }
            {
                u8 shown=g_survivalRun?p->lives:player_reserves(p);
                if(!g_survivalRun && shown>4)shown=4;
                if(!hud_valid || hud_lives[k]!=shown)
                {
                    if(!g_survivalRun)arcade_gameplay_lives(k,shown);
                    else
                    {
                u16 x = k ? 28 : 0;
                u16 tile = ss_anims[k ? SS_ANIM_PLAYER2_RUN : SS_ANIM_PLAYER_RUN].frames[0].tile;
                char lives[2];
                VDP_setTileMapXY(BG_B,TILE_ATTR_FULL(PAL1,0,0,0,tile),x,2);
                VDP_setTileMapXY(BG_B,TILE_ATTR_FULL(PAL1,0,0,0,tile+2),x+1,2);
                lives[0]=(char)('0'+(!lives_256[k] && p->lives<=9?p->lives:9));lives[1]=0;
                VDP_drawText(lives,x+3,2);
                    }
                    hud_lives[k]=shown;
                }
            }
        }
        if (!g_survivalRun && (!hud_valid || hud_high != hi))
        {
            hud_score_string(hi,buf);
            VDP_drawTextEx(BG_B,buf,TILE_ATTR_FULL(PAL2,1,0,0,0),13,1,CPU);
            hud_high = hi;
        }
        if(!g_survivalRun && np>1)
        {
            /* Source4D5A adds four packed-BCD bytes, discarding final carry.
             * Native individual scores are bounded to eight decimal digits. */
            u32 total=g_game.players[0].score+g_game.players[1].score;
            if(total>=100000000UL)total-=100000000UL;
            if(!hud_valid || hud_total!=total)
            {
                hud_score_string(total,buf);
                VDP_drawTextEx(BG_B,buf,TILE_ATTR_FULL(PAL2,1,0,0,0),13,3,CPU);
                hud_total=total;
            }
        }
        hud_valid = 1;
    }

    if (!g_attract && !g_survivalRun)
    {
        u16 credits = g_mode == MODE_HOME ? g_homeCredits : g_arcadeCredits;
        if (!hud_credit_valid || hud_credits != credits)
        {
            char value[7];
            /* Source credit row30 lies below the224-line viewport. Keep the
             * console credit counter at the bottom, clear of TOTAL SCORE. */
            VDP_drawText("CREDIT", 2, 27);
            arcade_credit_text(credits, value);
            VDP_drawText(value, 9, 27);
            hud_credits = credits;hud_credit_valid = 1;
        }
    }

    if (g_survivalRun && hud_survival_second != g_survivalSeconds)
    {
        char clock[8], n[12];
        u32 seconds = g_survivalSeconds;
        u32 best = run_best_ticks / 300;
        u8 pass;
        VDP_drawText("TIME",0,3);
        for(pass=0;pass<2;pass++)
        {
            u32 t = pass ? (best > seconds ? best : seconds) : seconds;
            if(t>59999)t=59999;
            intToStr((s32)(t/60),n,3);clock[0]=n[0];clock[1]=n[1];clock[2]=n[2];clock[3]=':';
            intToStr((s32)(t%60),n,2);clock[4]=n[0];clock[5]=n[1];clock[6]=0;
            VDP_drawText(clock,pass?12:5,pass?1:3);
        }

        hud_survival_second = seconds;
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
    if (g_gameOverBeat && !hud_gameover)
    {
        VDP_drawText("GAME OVER", 11, 13);
        if (!g_attract && (g_homeCredits || g_mode == MODE_ARCADE))
            VDP_drawText("CONTINUE?", 11, 14);
        hud_gameover = 1;
    }   /* H32: (32-9)/2 ~= 11 -> centered  */

    /* (No area-transition banner: the seamless ribbon never pauses -- the scenery
     * flows continuously from one segment into the next, so there is no round-clear
     * interlude to flash. The HUD "RND" field (above) tracks the segment under the
     * camera, updated live as each seam scrolls in.) */
}

/* Per-controller Start join/continue is a requested console extension.
 * The arcade97AB gameplay loop has no equivalent Start admission. Enter its
 * AD9B state6/AE99 reentry sequence without state5's reserve consumption. */
static void player_continue(u8 which)
{
    Player *p = &g_game.players[which];
    p->which = which;
    p->lives = g_config.lives;lives_256[which]=0;
    p->flow = FLOW_DYING; /* state6 becomes reentry on the next source update */
    p->flowTimer = 0;
    p->deadReq = 0; p->hurt = 0; p->invuln = 1;
    /* Start is sampled at display rate. Park until state6 publishes its
     * source pose; a refresh with no simulation must not show the old body. */
    p->y=240;
    {
        ArcadeLife *l=&player_life[which];
        memset(l,0,sizeof(*l));l->owner=which;l->active=2;l->kind=1;l->state=6;
        life_owned[which]=1;life_path[which]=0;
        p->firePrev=p->fireAnim=0;
        player_motion[which].previous=0;
    }
    if (g_game.numPlayers <= which) g_game.numPlayers = which + 1;
    run_started |= (u8)(1 << which);
    run_key = save_run_key(g_mode,run_started == 3 ? 2 : 1,g_survivalRun);
    run_best_score = save_best_score(run_key);
    run_best_ticks = save_best_ticks(run_key);
    g_gameOverBeat = 0;
    hud_gameover = 0;
    hud_valid = 0;
    VDP_clearTextArea(8, 13, 20, 2);
}

static void game_start_buttons(u8 starts)
{
    u8 i;
    if (!starts) return;
    if (g_mode == MODE_ARCADE)
    {
        for (i = 0; i < MAX_PLAYERS; i++)
            if ((starts & (1 << i)) && g_game.players[i].flow == FLOW_GAME_OVER &&
                arcade_take_credit()) player_continue(i);
    }
    else
    {
        u8 alive = 0;
        for (i = 0; i < g_game.numPlayers; i++)
            if (g_game.players[i].flow != FLOW_GAME_OVER) alive = 1;
        if (!alive)
        {
            if (g_survivalRun) return;
            if (g_homeCredits)
            {
                g_homeCredits--;
                for (i = 0; i < g_game.numPlayers; i++) player_continue(i);
            }
        }
        else
        {
            g_paused ^= 1;
            if (g_paused) VDP_drawText("PAUSED", 13, 13);
            else VDP_clearTextArea(13, 13, 6, 1);
        }
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
    u16 demoRemaining = 30;
    const u8 pal = SYS_isPAL();
    game_init();
    mode_input_rearm();
    gameplay_input_ready=0;
    gameplay_input_sample(1);
    if(g_mode==MODE_ARCADE)
        gameplay_pacing=arcade_pacing_enter(SYS_isPAL(),(u16)vtimer);
    else
    {
        gameplay_pacing=&home_pacing;
        arcade_pacing_init(gameplay_pacing,SYS_isPAL(),(u16)vtimer);
    }
    while (TRUE)
    {
        u16 credits_before = g_arcadeCredits;
        u8 starts = mode_input_poll();
        if(g_attract && g_mode==MODE_ARCADE && mode_input_coins())snd_play_coin();
        u8 updated=0;
        bool done=FALSE;
        if (g_attract && g_mode == MODE_ARCADE && !arcade_title_demo_owned && !game_arcade_frozen() &&
            (starts || g_arcadeCredits != credits_before))
        {
            /* Poll coins before returning; preserve the controller Start edge
             * across title rearm so it is neither lost nor spent twice. */
            g_attract_startPlayers = starts;
            g_attract_started = 1;
            break;
        }
        if (!g_attract && !game_arcade_frozen()) game_start_buttons(starts);
        gameplay_input_sample(g_paused);
        arcade_pacing_observe(gameplay_pacing,(u16)vtimer,g_paused||g_survivalRun);
        if (g_paused)
        {
            SYS_doVBlankProcess();
            continue;
        }
        /* Bounded catch-up retains excess debt. Publish the same logical
         * admission that rendering would provide before the next step.
         * Results/scene transitions get a display boundary before more work.
         * Two source updates never fit one refresh, so taking both after a
         * single overrun overran again and locked into ~30Hz presentation.
         * Carry one tick instead: 55.4Hz source time leaves ~7.5% of NTSC
         * refreshes without a due tick, which repay it within ~13 refreshes.
         * Catch up two per refresh only when debt exceeds that carry. PAL
         * accrues ~1.1 ticks per refresh and keeps the original rule. */
#ifndef CATCHUP_CARRY
#define CATCHUP_CARRY 2
#endif
        u8 limit=gameplay_pacing->pending>(pal?1u:(u32)CATCHUP_CARRY)?2:1;
        while(updated<limit && (g_survivalRun || arcade_pacing_take(gameplay_pacing)))
        {
            u8 was_results=g_resultsActive;
            gameplay_input_consume();gameplay_input_ready=1;
            done = game_update();
            /* 94B6 checks the phased timer after the source update. Setup ticks
             * before gameplay are still pending in the front-end lifecycle. */
            if(g_attract && !game_arcade_frozen()) {
                if(arcade_title_demo_owned)done=arcade_title_demo_update(done);
                else if(!(entity_arcade_clock()&63) && --demoRemaining==0)done=TRUE;
            }
            gameplay_input_ready=0;++updated;
            if(done || g_survivalRun || was_results || g_resultsActive)break;
            if(updated<limit && gameplay_pacing->pending)
                entity_arcade_prepare_admission();
        }
        game_render();
        g_presentCounter++;   /* prepared display, possibly repeated simulation */
        game_present();
        if (done) break;
        if (g_attract && g_mode == MODE_HOME)
        {
            if (JOY_readJoypad(JOY_1) & (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C) ||
                (JOY_readJoypad(JOY_2) & (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C)))
                { g_attract_started = 1; break; }
        }
    }

    /* AUDIO teardown: stop the music/SFX, silence the chips, and DETACH the
     * V-Blank replay tick so the front-end (selector / SEGA / CAPCOM splashes)
     * owns the sound hardware again on the way back.  Without this, snd_vblank
     * would keep firing during those stages and fight their direct YM use. */
    snd_shutdown();
    if (!g_attract)
    {
        u8 i;u32 score=0;
        for(i=0;i<g_game.numPlayers;i++)if(g_game.players[i].score>score)score=g_game.players[i].score;
        save_record_run(run_key,score,g_survivalRun?g_survivalTicks:0);
        if(g_mode==MODE_ARCADE && !g_survivalRun)
            arcade_postgame_run(g_game.players[0].score,g_game.players[1].score,g_game.numPlayers>1);
        save_commit();
    }
}

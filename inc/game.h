/* game.h -- SonSon NATIVE game engine (Phase 3 skeleton).
 *
 * Clean native Genesis/SGDK game state: real C structs + locals, world coords
 * in pixels, direct VDP writes. This is deliberately NOT a transliteration of
 * the arcade 6809 (no source-CPU RAM model, no per-frame palette/tile scan) --
 * see ROADMAP.md Phase 3. The faithful arcade movement (run-along-lanes +
 * climb-between-lanes) is layered on top later; for this phase the goal is a
 * working 60fps loop: a controllable player sprite + a scrolling camera + an
 * on-screen FPS readout, built on the Phase-2 baked scene (ss_gfx.h).
 */
#ifndef GAME_H
#define GAME_H

#include <genesis.h>
#include "ss_level.h"    /* baked levels: ss_levels[], SsLevel, SS_LEVEL_* (multi) */
#include "anim_rt.h"     /* shared animation runtime (consumes baked ss_anim.h) */

/* ---- World / level geometry (pixels) -------------------------------------
 * Phase 4: the world is the WIDE baked level (ss_level.h), reconstructed from
 * the arcade's scrolling level-1 (SS_LEVEL_COLS columns x 32 rows of BG cells).
 * LEVEL_W is the real level pixel width, so the player's perpetual forward creep
 * scrolls through ~5 screens of authentic level. The Genesis Plane A is 64 cols
 * wide (max), so for a level wider than 64 cols the BG is drawn as a 64-col
 * sliding window via COLUMN STREAMING (see game.c). The visible window is
 * 256x224 (H32). */
#define SCREEN_W            256
#define SCREEN_H            224
/* The level pixel width is now PER-ROUND (Phase 5 multi-level): each round shows
 * a different baked level (ss_levels[round]) and they differ in column count, so
 * the wide-level width is a runtime value (g_levelW = current level cols * 8),
 * set by level_select() in game.c. Use g_levelW wherever the old single-level
 * LEVEL_W constant was used (camera clamp, etc.). */
extern u16 g_levelW;                              /* current level pixel width   */
#define LEVEL_H             SCREEN_H

/* Plane A is a 64-col sliding window over the wide level. */
#define PLANE_COLS          64
#define PLANE_COL_MASK      (PLANE_COLS - 1)      /* col % 64 via & mask        */

/* Player sprite is 16x16 (SPRITE_SIZE(2,2)), same as every arcade game sprite. */
#define PLAYER_W            16
#define PLAYER_H            16

/* ===========================================================================
 *  FAITHFUL LOCOMOTION CONSTANTS -- sourced from the arcade RE (the oracle).
 * ===========================================================================
 *  SonSon is a vertically-looping multi-lane climber: the hero runs left/right
 *  along horizontal lanes while held inside a fixed horizontal screen band (the
 *  world scrolls past him), and steps UP/DOWN between lanes -- the lane field
 *  wraps vertically (climb off the top -> reappear at the bottom). That wrap is
 *  the distinctive feel.
 *
 *  Every number below is taken from the verified player code in the sibling
 *  port (../sonson/port/src). Player entity record base = arcade $0490 (P1) /
 *  $04C0 (P2); fields at signed byte offsets from the base. The audit:
 *
 *  - Constant forward "run" drift: sub_A9C5.c @ $A9D9 (and sub_A9A7.c @ $ADA9)
 *    do  ldd -$A,x ; addd #$008D ; std -$A,x ; bcc ; inc -$B,x  -- i.e. EACH
 *    FRAME add 0x008D to the 16-bit X-fraction (-$A) and carry into the integer
 *    world-X (-$B). 0x008D/0x100 = 141/256 = ~0.551 px/frame. This is SonSon's
 *    perpetual forward motion. We mirror it as a 1/256-px sub-pixel accumulator.
 *      -> RUN_DRIFT_SUBPX below.
 *
 *  - Horizontal SCREEN-BAND clamp: sub_AB52.c does  screenX = worldX(-$B) - $90
 *    (scroll); if screenX < $08 push worldX forward to screenX==$08; if
 *    screenX >= $D0 push worldX back to screenX==$D0. So the hero is pinned to
 *    the horizontal band [0x08 .. 0xD0] = [8 .. 208] px and the level scrolls.
 *      -> BAND_LEFT / BAND_RIGHT below.
 *
 *  - Lane (vertical) field: the player Y lives in the SINGLE byte at -$8
 *    (sub_AE16.c @ $AE2B parks it at #$F8; sub_AD10.c @ $AD4E does subb #$08 on
 *    it). An 8-bit Y => it wraps mod 256 -> the vertical loop. Lane motion is
 *    quantized in 8px sub-steps; the visible lane pitch in the captured scene's
 *    ladder sprites (ss_gfx: bank-9 lane sprites at y=108,132,168,216) is ~24px.
 *      -> LANE_PITCH (24) and LANE_COUNT below; lane index wraps.
 *
 *  - Anim cadence: the shared per-frame tick sub_9617.c counts the timer byte
 *    at -$3 down to 0, then steps the 2-byte sprite-cell script cursor (-$2:-1)
 *    and reloads the timer from the script's control byte (low 7 bits). We model
 *    the cadence with a frame countdown; PLAYER_ANIM_PERIOD is the native step.
 * ========================================================================= */

/* Constant forward drift, in 1/256-px units per frame (arcade $008D @ $A9D9). */
#define RUN_DRIFT_SUBPX     0x008D

/* Commanded run velocity (1/256-px per frame). The arcade gates a velocity word
 * (-$B..-$9 accumulate it); we drive it from the D-pad. RIGHT adds the forward
 * drift on top (player + world move together); LEFT pushes back against it. The
 * net magnitude (~2px/frame on screen) is chosen so the band-clamp scroll reads
 * at the arcade's pace. */
#define RUN_VEL_SUBPX       0x0200      /* 2.0 px/frame commanded run         */

/* Horizontal screen band the player is pinned inside (arcade $08..$D0, sub_AB52).
 * Expressed as the sprite's top-left screen X (the arcade clamps the same edge). */
#define BAND_LEFT           8           /* arcade $08                          */
#define BAND_RIGHT          208         /* arcade $D0                          */

/* Lanes: vertical pitch (px) and how many stack before the field wraps. 24px is
 * the captured ladder-rung spacing; the player Y byte wraps mod 256 in the
 * arcade, so the lane INDEX wraps here (climb past the top => bottom lane). */
#define LANE_PITCH          24
#define LANE_COUNT          7           /* 7*24 = 168px of climbable lanes     */
#define LANE_BASE_Y         (LEVEL_H - PLAYER_H - 16)  /* world Y of lane 0    */

/* Frames the player must hold UP/DOWN before the next lane step (one discrete
 * step per press-hold tick -- the arcade advances the lane on an edge, not
 * continuously). Tuned for a brisk-but-controllable climb. */
#define LANE_STEP_PERIOD    8

/* Walk-anim: frames between anim-step increments while moving (sub_9617 cadence). */
#define PLAYER_ANIM_PERIOD  6

/* ===========================================================================
 *  GAME-FLOW CONSTANTS -- death / respawn / game-over, from the arcade RE.
 * ===========================================================================
 *  Sourced from the verified sibling port (../sonson/port/src) + the front-end
 *  spec (project memory: lives come from DSW2 0x03; "credits/lives gone => game
 *  over until power-cycle"; the game-over path unwinds back to the front-end):
 *
 *   - Player DEATH handler  sub_B286.c : zeroes the player slot's state bytes
 *     (clra/clrb -> stb -$10,x ; std -$F,x) -- i.e. the player is removed and
 *     stops moving. We model that as a DEAD phase: the player is disabled and
 *     blinks for a short beat before the respawn re-inits him.
 *
 *   - Player RESPAWN handler sub_B1C2.c : re-inits the player record -- worldX =
 *     scroll($90) + $00F8 (re-placed relative to the current scroll), Y parked
 *     at #$F8, and the slot's per-entity timer +$D loaded with #$20 (0x20 = 32
 *     frames) as a fresh-spawn grace/anim countdown. We re-express that grace as
 *     an i-frame (invulnerable) window with a blink, and respawn the player on
 *     his start lane pinned at the screen band -- the native equivalent of the
 *     arcade's scroll-relative re-placement.
 *
 *   - GAME OVER : when the last life is lost the arcade unwinds out of the game
 *     loop; the front-end re-entry (memory: "GAME-OVER UNWIND fix = re-entry
 *     for(;;) loop in main()") returns to the attract/selector. We mirror that:
 *     a brief GAME OVER overlay, then game_run() returns and main.c loops back
 *     to selector_run().
 *
 *  Durations are in 60fps frames; tuned to feel like the arcade beats (the
 *  arcade's exact death-anim length is foreground-paced and not load-bearing --
 *  see the native-port doctrine: plays-right over cycle-exactness). */
#define DEATH_FRAMES        60      /* ~1.0s disabled+blink before respawn       */
#define RESPAWN_IFRAMES     150     /* ~2.5s invulnerable + blink after respawn  */
#define GAMEOVER_FRAMES     150     /* ~2.5s GAME OVER overlay before returning  */
#define BLINK_MASK          2       /* sprite shown when (timer & BLINK_MASK)==0 */

/* ===========================================================================
 *  EXTRA-LIFE (BONUS) -- arcade DSW2 0x18 thresholds, RE-sourced.
 * ===========================================================================
 *  SonSon awards bonus lives when the score crosses fixed thresholds selected by
 *  the arcade Bonus-Life switch (DSW2 0x18 -> g_config.bonusLife 0..3). The labels
 *  (dipswitch.c, verbatim from MAME -listxml sonson) and their threshold lists:
 *    idx0 "30K 90K 120K"  -> extra life at  30000,  90000, 120000  (three, then none)
 *    idx1 "20K 80K 100K"  -> extra life at  20000,  80000, 100000  (default)
 *    idx2 "30K ONLY"      -> extra life at  30000                  (one only)
 *    idx3 "20K ONLY"      -> extra life at  20000                  (one only)
 *  The "ONLY" suffix on idx2/idx3 confirms idx0/idx1 are fixed 3-award lists (NOT
 *  "first at X then every Y"). All six threshold VALUES (20K/30K/80K/90K/100K/120K)
 *  were confirmed present in the arcade program ROM as BCD constants near the score
 *  code (rom/ss.01e ~$4E60-$4EBC). The running score is plain decimal here (game.c
 *  g_game.score), so thresholds are stored as decimal points -- no BCD/x10 needed.
 *
 *  LIVES CAP: the arcade lives display is a small field; we cap the in-hand count
 *  so a long high-scoring run can't overflow the HUD digit. 9 keeps the single-digit
 *  "Lx N" readout valid (the HUD already clamps the glyph to 9). */
#define BONUS_LIST_MAX      3       /* longest threshold list (idx0/idx1 = 3 entries) */
#define LIFE_MAX            9       /* cap on lives in hand (HUD is one digit)        */

/* ===========================================================================
 *  ROUND / STAGE PROGRESSION -- faithful auto-scroll advance + difficulty ramp.
 * ===========================================================================
 *  SonSon has NO discrete level-end: it is one endless forced auto-scroller whose
 *  world creeps right forever while a SATURATING stage counter ($005A, cap 0x1C)
 *  ramps the difficulty as you travel (RE proof: tools/LEVEL_END_SPEC.md). There
 *  is no goal, boss, temple, or finish wall -- the perpetual forward creep
 *  (RUN_DRIFT_SUBPX, applied every frame in player_control) IS the advance.
 *
 *  The native engine re-expresses that over its 8 baked sceneries: the creep
 *  carries the hero to the end of the current level (no wall to push, no dwell --
 *  just travel), then a BRIEF transition swaps in the next baked scenery (round R
 *  -> ss_levels[(R-1)%8]) and the stage counter ticks up. The transition is short
 *  (it only masks the one-frame scenery cold-fill), NOT the old 2-second banner
 *  that required pushing into a finish wall -- an interaction the arcade never had.
 *
 *  ADVANCE CONDITION: the camera has been carried to the level end
 *  (cameraX >= maxCam = LEVEL_W - SCREEN_W). No player push, no dwell -- the creep
 *  alone trips it, exactly the arcade's "the scroll carries you forward." */
#define ROUND_CLEAR_FRAMES  36      /* ~0.6s transition mask (scenery cold-fill)   */

/* STAGE difficulty: the arcade's $005A counter increments once per advance and
 * SATURATES at 0x1C (28) -- difficulty ramps then plateaus, it never runs away.
 * (Mirrored to $0096, whose hi-nibble the wave spawner reads; see entity.c.) */
#define STAGE_MAX           28      /* arcade $005A cap (0x1C)                      */

/* Per-STAGE difficulty ramp. Each stage nudges the enemies up a notch on TOP of
 * the g_config.difficulty base (kept subtle/faithful: a single chaser must still
 * stay out-paceable). entity.c reads g_game.stage and adds ROUND_SPEED_STEP
 * (1/256-px/frame) per stage to the march speed, capped so it never exceeds the
 * player's ~2px/frame run; it also tightens the flyer dive telegraph slightly per
 * stage. Because stage SATURATES at STAGE_MAX the ramp plateaus (arcade-faithful);
 * ROUND_SPEED_MAX_BONUS bounds it regardless. */
#define ROUND_SPEED_STEP        0x0020  /* +0.125 px/frame enemy speed per stage   */
#define ROUND_SPEED_MAX_BONUS   0x00C0  /* cap the cumulative stage speed bonus    */

/* Game-flow phase: the player's life-cycle state machine driven each frame in
 * game_update. ALIVE is normal play; DYING runs the death beat; RESPAWNING is
 * the post-respawn i-frame window; GAME_OVER shows the overlay then signals the
 * loop to return to the front-end. */
typedef enum
{
    FLOW_ALIVE = 0,
    FLOW_DYING,
    FLOW_RESPAWNING,
    FLOW_GAME_OVER,
    FLOW_ROUND_CLEAR    /* level traversed: brief transition, then next scenery  */
} GameFlow;

/* Facing flags (maps straight to the SAT flipH bit). */
#define FACING_RIGHT        0
#define FACING_LEFT         1

/* ---- Player ---------------------------------------------------------------
 * Clean native struct mirroring the arcade record's INTEGER behaviour (no 6809
 * register file / cc flags / RAM() accessors -- this is native C). The arcade
 * stores world-X as 24-bit fixed-point (-$B integer : -$A,-$9 fraction) and Y
 * as an 8-bit lane byte (-$8); we keep an integer world X plus a 1/256-px
 * sub-pixel accumulator (xSub) for the forward drift, and a discrete lane index
 * (+ derived world Y). `tile` is the absolute VRAM base tile of the 16x16
 * sprite (4 col-major 8x8 tiles), borrowed from the baked scene set. */
typedef struct
{
    s16 x, y;          /* world position (px), top-left of sprite           */
    u16 xSub;          /* X sub-pixel accumulator (1/256 px) -- drift + vel  */
    s16 vx;            /* commanded horizontal velocity (1/256 px per frame) */
    u8  lane;          /* current lane index [0..LANE_COUNT) -- wraps        */
    u8  laneTimer;     /* counts down to the next allowed lane step          */
    u8  climbing;      /* nonzero while a lane change is in progress (anim)   */
    u8  facing;        /* FACING_LEFT / FACING_RIGHT -> SAT flipH            */
    u8  anim;          /* walk/climb-cycle frame index (legacy state var)    */
    u8  animTimer;     /* counts down to the next anim step (legacy)         */
    u8  firePrev;      /* fire button state last frame (press-edge debounce) */
    u8  fireAnim;      /* frames left to keep showing the FIRE anim (recoil) */
    Anim av;           /* faithful animation cursor (run/climb/fire/death)   */
    u16 tile;          /* absolute VRAM base tile (SPRITE_SIZE(2,2))        */
} Player;

/* ---- Game state -----------------------------------------------------------
 * Small native global state. cameraX is the BG/world scroll origin in px
 * (the world x of the left screen edge); the player is kept roughly centered
 * and the camera is clamped to the level bounds. */
typedef struct
{
    Player player;
    u32    score;      /* running score (enemy defeats + item pickups)       */
    /* --- life economy / flow (Phase 8c game-flow) ---
     * lives    = remaining lives, initialised from g_config.lives in game_init
     *            (so the DIP/options LIVES setting takes effect). A life is lost
     *            on lethal enemy contact; when it would drop below 0 -> GAME OVER.
     * flow     = the death/respawn/game-over state machine (GameFlow).
     * flowTimer= frame countdown for the current non-ALIVE phase.            */
    u8     lives;
    /* --- extra-life (bonus) economy (arcade DSW2 0x18 thresholds) ---
     * bonusNextIdx = index of the NEXT not-yet-awarded bonus threshold in the
     *   active g_config.bonusLife list (see game.c k_bonus_thresholds[]). Each time
     *   the running score crosses k_bonus_thresholds[bonusLife][bonusNextIdx], a
     *   life is granted and this advances; when it passes the list end no more
     *   bonus lives are awarded (the arcade tables are fixed 3-shot / 1-shot lists,
     *   not "every N"). Reset to 0 in game_init. */
    u8     bonusNextIdx;
    u8     flow;       /* GameFlow                                            */
    u16    flowTimer;  /* frames left in the current DYING/RESPAWNING/GAMEOVER */
    /* --- round / stage progression (Phase 5 + faithful auto-scroll) ---
     * round    = current LEVEL pass, 1-based. Starts at 1; bumped each time the
     *            auto-scroll carries the hero to the level end. Selects the baked
     *            scenery (round R -> ss_levels[(R-1)%8]) + the HUD readout.
     * stage    = the arcade's saturating difficulty counter ($005A, cap 0x1C=28).
     *            Increments once per level-advance and SATURATES at STAGE_MAX, so
     *            difficulty ramps then plateaus exactly like the arcade -- it never
     *            climbs forever. entity.c reads this for the speed/wave/cadence ramp.
     *            Faithful model: SonSon advances by traversed scroll, NOT a finish
     *            line -- see tools/LEVEL_END_SPEC.md (the perpetual creep IS it). */
    u8     round;
    u8     stage;
    s16    cameraX;    /* world x at the left edge of the screen (px)        */
    /* --- column-streaming window state ---
     * The 64-col Plane A is a ring buffer over the wide level. loadedMinCol /
     * loadedMaxCol is the inclusive range of LEVEL columns currently resident in
     * the plane (a level column L lives at plane column L & PLANE_COL_MASK). As
     * the camera advances, newly-revealed columns are decoded + DMA'd in and the
     * range slides. -1 = nothing loaded yet (cold start fills the first window). */
    s16    loadedMinCol;
    s16    loadedMaxCol;
} GameState;

extern GameState g_game;

/* Current level descriptor (Phase 5 multi-level): points into ss_levels[] for the
 * round being played. Selected by level_select(round) in game.c, which also loads
 * the level's tiles at SS_LEVEL_TILE_BASE and sets g_levelW. The column streamer
 * reads g_curLevel->map / ->cols. */
extern const SsLevel *g_curLevel;

/* Boot-flow config: the mode chosen at the front-end selector (Phase 8),
 * MODE_ARCADE (0) or MODE_HOME (1) from frontend.h. Stored once by main.c after
 * selector_run() and readable by the game (e.g. for the arcade DIP / home
 * options branch -- Phase 8b). */
extern u8 g_mode;

/* ===========================================================================
 *  GameConfig -- the front-end-selected game parameters (Phase 8b).
 * ===========================================================================
 *  Filled by the two config screens that run after the selector:
 *    MODE_ARCADE -> dip_screen_run()  (the full authentic arcade DIP bank)
 *    MODE_HOME   -> options_run()     (the console-style options subset)
 *  Both write this struct; the native engine reads it as game-flow lands
 *  (lives, difficulty, bonus thresholds, etc.).
 *
 *  The two raw bytes `dsw1`/`dsw2` mirror SonSon's REAL arcade DIP banks bit
 *  for bit (verified against MAME `-listxml sonson` and the sibling port's
 *  shared menu_label/menu_cycle), so the arcade DIP screen edits the authentic
 *  hardware switches directly. The decoded scalar fields below are the
 *  convenient view the engine consumes; sync_from_dsw() (dipswitch.c) derives
 *  them from the two bytes. Authentic bit layout:
 *
 *    DSW1: Coinage        (mask 0x0F)   1C/1C default (0x0F)
 *          Coinage affects (0x10)        Coin A default
 *          Demo Sounds     (0x20)        0=On (default), 0x20=Off
 *          Service Mode    (0x40)        0x40=Off (default)
 *          Flip Screen     (0x80)        0x80=Off (default)
 *    DSW2: Lives          (0x03)        3:0x03(def) 4:0x02 5:0x01 7:0x00
 *          2 Players Game  (0x04)        0=2 Credits(def), 0x04=1 Credit
 *          Bonus Life      (0x18)        idx0=20K/80K/100K(def) ... (see table)
 *          Difficulty      (0x60)        idx 0Easy..3VeryHard; 0x40 Normal def
 *          Freeze          (0x80)        0x80=Off (default)
 * ========================================================================= */

/* Authentic power-on DIP defaults: the OR of every MAME "default=yes" value
 * (verified against -listxml sonson; each value= attribute is the raw masked
 * field value, so the bank byte is their bitwise OR).
 *   DSW1 = Coinage 1C/1C(0x0F) | CoinA(0x10) | DemoOn(0x00) | ServiceOff(0x40)
 *          | FlipOff(0x80)                                             = 0xDF
 *   DSW2 = Lives 3(0x03) | 2Players 2Cred(0x00) | Bonus 20K/80K/100K(0x08)
 *          | Difficulty Normal(0x40) | FreezeOff(0x80)                 = 0xCB  */
#define DSW1_DEFAULT   0xDF
#define DSW2_DEFAULT   0xCB

typedef struct
{
    /* Raw arcade DIP banks (edited bit-exact by the arcade DIP screen). */
    u8 dsw1;
    u8 dsw2;

    /* Decoded view the engine reads (kept in sync with dsw1/dsw2). */
    u8 lives;        /* extra lives count: 3 / 4 / 5 / 7                        */
    u8 difficulty;   /* 0=Easy 1=Normal 2=Hard 3=Very Hard                     */
    u8 bonusLife;    /* Bonus-Life table index 0..3 (see dipswitch.c labels)   */
    u8 demoSound;    /* 1=demo sounds on, 0=off                                */
    u8 flipScreen;   /* 1=screen flipped (cocktail), 0=normal                  */
} GameConfig;

extern GameConfig g_config;

/* Decode g_config.dsw1/.dsw2 (the raw arcade DIP banks) into the engine-facing
 * scalar fields (lives/difficulty/bonusLife/demoSound/flipScreen). Defined in
 * dipswitch.c; called by both config screens after they finish editing. */
void sync_from_dsw(void);

/* Lifecycle.
 *  game_init   -- one-time scene + player + life-economy setup (reads g_config).
 *  game_update -- one frame of simulation + the death/respawn/game-over flow.
 *                 Returns TRUE once the game is over AND the GAME OVER overlay
 *                 has finished, i.e. the caller should leave the game loop and
 *                 return to the front-end. FALSE while the game continues.
 *  game_render -- direct SAT build + scroll + HUD (score / lives / FPS) +
 *                 the GAME OVER overlay.
 *  game_run    -- the whole game session: game_init then the update/render/
 *                 V-blank loop, returning to the caller (main.c) when the game
 *                 is over so the boot flow can re-enter the selector. This is
 *                 the native mirror of the arcade's game-over unwind-to-attract
 *                 (project memory: the "re-entry for(;;) loop in main()").     */
void game_init(void);
bool game_update(void);
void game_render(void);
void game_run(void);

#endif /* GAME_H */

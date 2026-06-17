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
#include "ss_level.h"    /* wide baked level: SS_LEVEL_COLS, ss_level_map, tiles */

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
#define LEVEL_W             (SS_LEVEL_COLS * 8)   /* real wide-level pixel width */
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
    u8  anim;          /* walk/climb-cycle frame index (drives future frames)*/
    u8  animTimer;     /* counts down to the next anim step                  */
    u16 tile;          /* absolute VRAM base tile (SPRITE_SIZE(2,2))        */
} Player;

/* ---- Game state -----------------------------------------------------------
 * Small native global state. cameraX is the BG/world scroll origin in px
 * (the world x of the left screen edge); the player is kept roughly centered
 * and the camera is clamped to the level bounds. */
typedef struct
{
    Player player;
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

/* Lifecycle (called from main.c). */
void game_init(void);      /* palettes, tiles, BG nametable, player spawn    */
void game_update(void);    /* read pad, move player, advance anim, camera    */
void game_render(void);    /* direct SAT build + scroll + FPS readout        */

#endif /* GAME_H */

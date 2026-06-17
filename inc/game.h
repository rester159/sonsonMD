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

/* ---- World / level geometry (pixels) -------------------------------------
 * The captured scene is one 32x32-cell screen of BG = 256x256 px. The visible
 * window is 256x224 (H32). We give the "level" a little horizontal slack so the
 * camera has something to follow; the BG plane is 32 cells (256px) wide and
 * wraps, so for Phase 3 we clamp the world to the single painted screen and let
 * the camera scroll within it. LEVEL_W can grow once multi-screen level data is
 * wired (Phase 5). */
#define SCREEN_W            256
#define SCREEN_H            224
#define LEVEL_W             SCREEN_W      /* one painted screen for now        */
#define LEVEL_H             SCREEN_H

/* Player sprite is 16x16 (SPRITE_SIZE(2,2)), same as every arcade game sprite. */
#define PLAYER_W            16
#define PLAYER_H            16

/* Movement tuning (pixels/frame). Plain integers -- simple + native. The
 * faithful arcade accel/lane model replaces this in Phase 3b. */
#define PLAYER_SPEED_X      2
#define PLAYER_SPEED_Y      2

/* Walk-anim: frames between anim-step increments while moving. */
#define PLAYER_ANIM_PERIOD  6

/* Facing flags (maps straight to the SAT flipH bit). */
#define FACING_RIGHT        0
#define FACING_LEFT         1

/* ---- Player ---------------------------------------------------------------
 * Clean native struct. World-space position in pixels (top-left of the 16x16
 * sprite); velocity in px/frame. `tile` is the absolute VRAM base tile index of
 * the 16x16 sprite (4 col-major 8x8 tiles from there) -- taken from the baked
 * scene set so it's a recognizable SonSon sprite. */
typedef struct
{
    s16 x, y;          /* world position (px), top-left of sprite           */
    s16 vx, vy;        /* velocity (px/frame)                               */
    u8  facing;        /* FACING_LEFT / FACING_RIGHT -> SAT flipH           */
    u8  anim;          /* walk-cycle frame index (cosmetic for now)         */
    u8  animTimer;     /* counts down to the next anim step                 */
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
} GameState;

extern GameState g_game;

/* Lifecycle (called from main.c). */
void game_init(void);      /* palettes, tiles, BG nametable, player spawn    */
void game_update(void);    /* read pad, move player, advance anim, camera    */
void game_render(void);    /* direct SAT build + scroll + FPS readout        */

#endif /* GAME_H */

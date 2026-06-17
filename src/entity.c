#include <genesis.h>
#include "game.h"
#include "entity.h"
#include "ss_gfx.h"
#include "ss_anim.h"     /* baked player/enemy animation frames + table */
#include "sound.h"       /* snd_sfx_enemy / snd_sfx_item (verified $3010 cmds) */

/* ===========================================================================
 *  SON SON  --  NATIVE entity system (Phase 4)
 * ===========================================================================
 *  CLEAN NATIVE C. A fixed Entity pool the 68000 walks directly: spawn / update
 *  (AI + motion) / collision / score / render-to-SAT. NO 6809 RAM overlay, NO
 *  per-frame palette/tile scan -- the whole point of the native rewrite is that
 *  this loop is cheap enough to hold 60fps with a BUSY scene where the sibling
 *  transliteration dropped to ~48 (GF48).
 *
 *  Behavior is re-expressed from the arcade RE (the oracle), not transliterated:
 *    - enemyA ground enemy: spawn at the right edge, march LEFT toward the
 *      player, lane-align, contact-hurt the player.  (census enemyA $0768;
 *      handlers sub_DC26/DC33 spawn, sub_DC6C/DC7D/DC94/DCAC active, sub_DD78
 *      death.)
 *    - player SHOT: a debounced fire spawns a projectile that travels in the
 *      facing direction and despawns off-screen / on hit.  (arcade player fire
 *      sub_B1F3 -> sub_B25A spawning into $0E28.)
 *    - ITEM: a stationary fruit pickup; player touch => bonus score + vanish.
 *
 *  TWO MORE ARCHETYPES (the native-enrichment task), faithful AI re-expressed
 *  from the classF / enemyMain / classI handlers (the oracle):
 *    - FLYER (classF, dispatch $DF18, core mover $DF90, homing-dive $DFD9):
 *      the arcade flyer reads a per-state VELOCITY VECTOR from a 5-byte-stride
 *      table (sub_DF90: dd at [u+1]=dx, [u+3]=dy added to the 24-bit X / 16-bit
 *      Y) -> a scripted GLIDE path; its lifecycle ($DF79) glides ~16 frames then
 *      flips to a DIVE state ($0300) whose mover ($DFD9) HOMES toward the player
 *      (reads the player pos via $E162, biases dx/Y to chase). We re-express that
 *      as: enter from the right, GLIDE left with a vertical sine bob, then after
 *      a telegraph DIVE toward the player's current position. Body contact hurts.
 *    - THROWER (enemyMain idx4, dispatch $BAB3, mover core $BB46): the arcade
 *      thrower marches via the same per-state velocity table (sub_BB46) and its
 *      sub-states clamp the approach to within a distance of the player
 *      (cmpd #$0030 / #$0018 in sub_BAC3/BAE5) then flip to a fire phase. We
 *      re-express: march toward the player; once within firing range, periodically
 *      LOB a projectile (the classI child) on a cadence. Body contact hurts.
 *    - ENEMY_SHOT (lobbed projectile classI, launch $D352, arc $D384/$D3DC):
 *      $D352 launches it up+toward the target (initial Y-vel byte $F8 = -8,
 *      X = scroll+$E8) and sub_D4AB applies a downward accel each frame; the
 *      ascend ($D38E) / descend ($D3E6) halves trace a PARABOLA. We re-express
 *      with a native sub-pixel vy that GRAVITY increments each frame (vx toward
 *      the player at launch) -> the same up-then-down arc. Contact hurts the
 *      player; it despawns on the "ground" (its spawn lane) or off-screen.
 * =========================================================================== */

Entity g_entities[ENTITY_MAX];
u8 g_player_hurt = 0;

/* Game-flow bridge (see entity.h): game.c owns the life economy. */
u8 g_player_invuln       = 0;   /* set by game.c during death/respawn i-frames */
u8 g_player_dead_request = 0;   /* set here on a lethal contact, cleared by game.c */

/* ---- Tunables (re-expressed from the arcade RE, in native units) ----------
 * The arcade enemyA marches by adding a small per-frame delta to its 24-bit X;
 * we drive a velocity in 1/256-px/frame. ENEMY_SPEED gives a brisk ~1.0 px/frame
 * approach. ENEMY_SPAWN_AHEAD mirrors the arcade's worldX = scroll($90)+$00BE
 * (~190px right of the camera's left edge -- i.e. just past the right screen
 * edge, so enemies "walk in" from the right). */
#define ENEMY_SPEED         0x0100      /* 1.0 px/frame toward the player       */
#define ENEMY_SPAWN_AHEAD   0xBE        /* arcade $00BE: px right of camera left */
#define ENEMY_HP            1           /* one shot defeats a basic ground enemy */
#define ENEMY_ANIM_PERIOD   8           /* frames between walk-cell steps        */
#define ENEMY_DEATH_FRAMES  18          /* death-flash frames before despawn     */

/* Player SHOT: travels fast and horizontally (the arcade shot is a fast linear
 * projectile). Despawns when it leaves a generous world window around the view. */
#define SHOT_SPEED          0x0400      /* 4.0 px/frame                          */
#define SHOT_HALF_W         4           /* shot AABB half-width  (8px wide)      */
#define SHOT_HALF_H         3           /* shot AABB half-height                 */

/* Collision: enemy/player/item bodies are ~16x16 sprites; use a 16px AABB.
 * The arcade contact check (DCAC) gates on a ~16px Y window + X overlap. */
#define BODY_W              16
#define BODY_H              16

/* Hurt-flash window after the player is touched (frames). Decays on its own. */
#define HURT_FRAMES         40

/* ---- FLYER (classF $DF18/$DF90/$DFD9) tunables, in native units -----------
 * The arcade flyer glides on a scripted velocity vector then dives homing on the
 * player. We re-express: a leftward glide (FLYER_GLIDE_VX) with a vertical sine
 * bob (the arc), for FLYER_GLIDE_FRAMES, then a DIVE that steers vy/vx toward the
 * player's position (FLYER_DIVE_VX forward + a homing vy step). One shot kills it. */
#define FLYER_HP            1
#define FLYER_GLIDE_VX      0x00C0      /* ~0.75 px/frame leftward glide          */
#define FLYER_GLIDE_FRAMES  90          /* glide ~1.5s before the dive (arcade DF79
                                         * counts a telegraph then flips to dive)  */
#define FLYER_BOB_AMP       0x0140      /* vertical sine-bob velocity amplitude    */
#define FLYER_DIVE_VX       0x0180      /* ~1.5 px/frame forward during the dive   */
#define FLYER_DIVE_VY_STEP  0x0020      /* per-frame homing accel toward player Y   */
#define FLYER_DIVE_VY_MAX   0x0280      /* clamp the dive's vertical speed          */

/* ---- THROWER (enemyMain idx4 $BAB3 / mover $BB46) tunables -----------------
 * Marches toward the player at THROWER_SPEED; once the horizontal gap closes
 * inside THROWER_RANGE px (arcade clamps approach to ~$30/$18 of the target) it
 * stops and LOBS a projectile every THROWER_FIRE_PERIOD frames (with a short
 * THROWER_THROW_ANIM lunge). One shot kills it. */
#define THROWER_HP          1
#define THROWER_SPEED       0x00C0      /* ~0.75 px/frame approach                 */
#define THROWER_RANGE       96          /* stop + start lobbing within this gap px */
#define THROWER_FIRE_PERIOD 70          /* frames between lobs (~1.15s cadence)    */
#define THROWER_THROW_ANIM  14          /* frames the throw-lunge pose shows        */

/* ---- ENEMY_SHOT (lobbed projectile classI $D352) tunables -----------------
 * Launched up+toward the player; GRAVITY adds to vy each frame -> a parabola
 * (the arcade's $F8 up-vel + sub_D4AB down-accel). It hurts the player on contact
 * and despawns when it falls back to (or below) its launch lane, or off-screen. */
#define ESHOT_GRAVITY       0x0018      /* per-frame downward accel (1/256 px)     */
#define ESHOT_VY0           (s16)(-0x0260) /* initial UP velocity (~-2.4 px/frame) */
#define ESHOT_VX            0x0140      /* horizontal speed toward the player       */
#define ESHOT_HALF_W        4           /* projectile AABB half-width (8px)         */
#define ESHOT_HALF_H        4           /* projectile AABB half-height              */

/* Score awards (arcade exact per-enemy point values are not isolated in the RE;
 * reuse the task's +200 ground / +300 tougher tiers -- the flyer + thrower are
 * the "tougher" archetypes, so they score above the basic ground walker). */
#define SCORE_ENEMY         200         /* defeat a ground enemy                 */
#define SCORE_ITEM          500         /* collect a fruit                       */
#define SCORE_FLYER         300         /* defeat a flyer (tougher than ground)  */
#define SCORE_THROWER       300         /* defeat a thrower                      */

/* ---- DIFFICULTY hook (arcade DSW2 0x60 -> g_config.difficulty 0..3) --------
 * SonSon's Difficulty switch makes the enemies more aggressive. We re-express
 * that subtly + faithfully by scaling the ground-enemy march speed by the
 * selected tier: Easy a touch slower than the baseline 1.0px/frame, Normal the
 * baseline, Hard/Very-Hard progressively brisker. The scale is applied to
 * ENEMY_SPEED at spawn time (and on recycle), so the DIP setting changes how
 * fast enemies close on the player without altering the rest of the feel.
 *   idx 0 Easy = x0.75   1 Normal = x1.00   2 Hard = x1.25   3 VeryHard = x1.50
 * (1/256-px units; values chosen so even Very Hard stays under the player's
 * ~2px/frame run so he can always out-pace a single chaser.) */
static const u16 k_enemy_speed_by_diff[4] =
{
    (u16)(ENEMY_SPEED * 3 / 4),     /* Easy      ~0.75 px/frame                 */
    (u16)(ENEMY_SPEED),             /* Normal     1.00 px/frame                 */
    (u16)(ENEMY_SPEED * 5 / 4),     /* Hard      ~1.25 px/frame                 */
    (u16)(ENEMY_SPEED * 3 / 2)      /* Very Hard ~1.50 px/frame                 */
};

/* Resolve the current enemy march speed from g_config.difficulty (clamped). */
static u16 enemy_march_speed(void)
{
    u8 d = g_config.difficulty;
    if (d > 3) d = 3;
    return k_enemy_speed_by_diff[d];
}

/* The ENEMY now ANIMATES: its tile each frame comes from the baked faithful
 * animation set (ss_anim.c -- ENEMY_WALK / ENEMY_DEATH), driven by the Anim
 * cursor e->av. ENEMY_TILE is just the cold-start frame (walk frame 0).
 *
 * SHOT + ITEM keep a single borrowed scene tile (no anim baked for them yet --
 * see TODO). ss_scene_sprites[22].tile = a small bank-1 sprite (shot/item),
 * ss_scene_sprites[17].tile a bank-2 actor (item). */
#define ENEMY_TILE          (ss_anims[SS_ANIM_ENEMY_WALK].frames[0].tile)
#define SHOT_TILE           (ss_scene_sprites[22].tile)
#define ITEM_TILE           (ss_scene_sprites[17].tile)   /* bank-2 actor (119) */
/* New archetypes animate off baked anim sets (cold-start = frame 0). */
#define FLYER_TILE          (ss_anims[SS_ANIM_FLYER_FLY].frames[0].tile)
#define THROWER_TILE        (ss_anims[SS_ANIM_THROWER_IDLE].frames[0].tile)
#define ESHOT_TILE          (ss_anims[SS_ANIM_ESHOT_FLY].frames[0].tile)

/* ------------------------------------------------------------------------- */
/* Free-slot allocator: linear scan of the fixed pool (ENTITY_MAX is small, so
 * this is a handful of byte compares -- cheaper than maintaining a free list).
 * Re-expresses the arcade's "scan the array for an inactive slot" spawn path
 * (e.g. sub_DD31 scanning the $0ED8 pool for a free record). */
Entity *entity_spawn(u8 type, s16 x, s16 y)
{
    u16 i;
    for (i = 0; i < ENTITY_MAX; i++)
    {
        Entity *e = &g_entities[i];
        if (e->active) continue;

        e->active    = 1;
        e->type      = type;
        e->x         = x;
        e->y         = y;
        e->xSub      = 0;
        e->ySub      = 0;
        e->vx        = 0;
        e->vy        = 0;
        e->lane      = 0;
        e->state     = 0;
        e->timer     = 0;
        e->facing    = 0;
        e->anim      = 0;
        e->animTimer = ENEMY_ANIM_PERIOD;
        e->phase     = 0;
        e->fireTimer = 0;
        e->launchY   = 0;
        anim_restart(&e->av, SS_ANIM_ENEMY_WALK);   /* default; per-type below   */

        switch (type)
        {
            case ET_ENEMY_A:
                e->hp     = ENEMY_HP;
                e->facing = 1;             /* enemyA marches LEFT (toward player) */
                e->vx     = (s16)(-(s16)enemy_march_speed());  /* difficulty-scaled */
                anim_restart(&e->av, SS_ANIM_ENEMY_WALK);      /* start walking   */
                e->tile   = ENEMY_TILE;
                break;

            case ET_SHOT:
                e->hp   = 0;
                e->tile = SHOT_TILE;
                /* vx/facing set by entity_player_fire (direction = player facing) */
                break;

            case ET_ITEM:
                e->hp   = 0;
                e->tile = ITEM_TILE;       /* stationary collectible              */
                break;

            case ET_FLYER:
                e->hp     = FLYER_HP;
                e->facing = 1;             /* flies in from the right (faces left) */
                e->state  = 0;             /* 0 = glide, 1 = dive, 2 = dying       */
                e->vx     = (s16)(-FLYER_GLIDE_VX);
                e->timer  = FLYER_GLIDE_FRAMES;     /* glide telegraph countdown   */
                anim_restart(&e->av, SS_ANIM_FLYER_FLY);
                e->tile   = FLYER_TILE;
                break;

            case ET_THROWER:
                e->hp     = THROWER_HP;
                e->facing = 1;             /* marches LEFT toward the player       */
                e->state  = 0;             /* 0 = approach, 1 = fire-stance, 2 dying*/
                e->vx     = (s16)(-THROWER_SPEED);
                e->timer  = THROWER_FIRE_PERIOD;    /* first-lob cadence countdown */
                anim_restart(&e->av, SS_ANIM_THROWER_IDLE);
                e->tile   = THROWER_TILE;
                break;

            case ET_ENEMY_SHOT:
                e->hp   = 0;
                anim_restart(&e->av, SS_ANIM_ESHOT_FLY);
                e->tile = ESHOT_TILE;
                /* vx / vy set by the thrower at launch (toward the player) */
                break;

            default:
                e->tile = ENEMY_TILE;
                break;
        }
        return e;
    }
    return 0;   /* pool full */
}

/* ------------------------------------------------------------------------- */
/* Spawn the initial enemy wave + a couple of items. Called from entity_init.
 * Enemies are seeded across the wide level ahead of the player's start so they
 * "walk in" as the camera scrolls -- and we deliberately place SEVERAL plus a
 * couple of items so the headline busy-scene FPS test has real load. */
static void spawn_initial_wave(void)
{
    /* Lane world-Y for a given lane index (matches game.c's player lanes:
     * lane 0 = bottom band, higher lanes step up by LANE_PITCH). */
    #define LANE_Y(l)   ((s16)(LANE_BASE_Y - (s16)(l) * LANE_PITCH))

    /* A spread of ground enemies down the level, on assorted lanes. They march
     * left toward the player; placing them at increasing world-X means a fresh
     * one enters the right edge roughly every time the camera advances. */
    entity_spawn(ET_ENEMY_A, 320,  LANE_Y(0));
    entity_spawn(ET_ENEMY_A, 460,  LANE_Y(1));
    entity_spawn(ET_ENEMY_A, 600,  LANE_Y(0));
    entity_spawn(ET_ENEMY_A, 760,  LANE_Y(2));
    entity_spawn(ET_ENEMY_A, 900,  LANE_Y(1));
    entity_spawn(ET_ENEMY_A, 1040, LANE_Y(0));
    entity_spawn(ET_ENEMY_A, 1180, LANE_Y(3));

    /* FLYERS: enter high (upper lanes) and glide-then-dive at the player. Two
     * seeded across the level so the busy-scene test always has a flyer on
     * screen alongside the walkers + a thrower. */
    entity_spawn(ET_FLYER, 540,  LANE_Y(5));
    entity_spawn(ET_FLYER, 980,  LANE_Y(4));

    /* THROWERS: march in and lob projectiles from range. Two across the level. */
    entity_spawn(ET_THROWER, 700,  LANE_Y(2));
    entity_spawn(ET_THROWER, 1120, LANE_Y(1));

    /* A couple of fruit pickups on lanes the player will climb to. */
    entity_spawn(ET_ITEM, 520, LANE_Y(2));
    entity_spawn(ET_ITEM, 840, LANE_Y(0));

    #undef LANE_Y
}

void entity_init(void)
{
    u16 i;
    for (i = 0; i < ENTITY_MAX; i++) g_entities[i].active = 0;
    g_player_hurt         = 0;
    g_player_invuln       = 0;
    g_player_dead_request = 0;
    spawn_initial_wave();
}

/* ------------------------------------------------------------------------- */
/* Player FIRE -- arcade sub_B25A re-expressed. Spawn a SHOT at the player's
 * mid-body, traveling in the facing direction. game.c handles the press-edge
 * debounce (the arcade's "+$3 fire-once" flag), so this just emits the shot. */
void entity_player_fire(s16 px, s16 py, u8 facing)
{
    Entity *s = entity_spawn(ET_SHOT, px, py);
    if (!s) return;
    s->facing = facing;
    s->vx     = facing ? (s16)(-SHOT_SPEED) : (s16)SHOT_SPEED;
    s->vy     = 0;
}

/* ------------------------------------------------------------------------- */
/* Integrate a sub-pixel velocity into an integer world coordinate. Mirrors the
 * arcade's "addd to the fraction word, carry into the integer byte" idiom. */
static inline void integrate(s16 *pos, u16 *sub, s16 vel)
{
    s32 acc = (s32)(*sub) + (s32)vel;
    *pos    = (s16)(*pos + (acc >> 8));
    *sub    = (u16)(acc & 0xFF);
}

/* Advance the anim-cell cadence (arcade sub_9617: countdown then step). */
static inline void anim_step(Entity *e, u8 period)
{
    if (--e->animTimer == 0)
    {
        e->animTimer = period;
        e->anim = (u8)((e->anim + 1) & 0x03);   /* 4-cell cycle                  */
    }
}

/* AABB overlap of two world-space boxes (top-left + size). */
static inline u8 aabb(s16 ax, s16 ay, s16 aw, s16 ah,
                      s16 bx, s16 by, s16 bw, s16 bh)
{
    return (u8)(ax < bx + bw && ax + aw > bx &&
                ay < by + bh && ay + ah > by);
}

/* ---- FLYER vertical-bob sine -----------------------------------------------
 * The arcade flyer's glide vy comes from a per-state velocity-table entry; on a
 * looping glide that reads as a gentle vertical oscillation. We reproduce the
 * arc with a tiny 16-step sine LUT (signed, +-64 ~= +-1.0 in 1/64 units), scaled
 * by FLYER_BOB_AMP, indexed by a per-entity phase that ticks each frame. Cheap:
 * one table read + a multiply per flyer per frame. */
static const s8 k_sin16[16] =
{
      0,  24,  45,  59,  64,  59,  45,  24,
      0, -24, -45, -59, -64, -59, -45, -24
};
static inline s16 flyer_bob_vy(u8 phase)
{
    /* phase advances every 4 frames (see update); >>2 gives a ~64-frame period. */
    s32 s = k_sin16[(phase >> 2) & 0x0F];
    return (s16)((s * (s32)FLYER_BOB_AMP) >> 6);
}

/* ---- THROWER lob: spawn a lobbed projectile arcing toward the player --------
 * Re-expresses the classI launch (sub_D352): the projectile starts at the
 * thrower's mid-body with an UP initial velocity (ESHOT_VY0) and a horizontal
 * velocity toward the player; gravity (applied each frame in the ESHOT update)
 * pulls it down into a parabola. launchY records the lob height so the shot
 * despawns when it falls back to that level -- the native land/impact (arcade
 * sub_D446), plus a hard max-life guard. */
static void thrower_lob(s16 sx, s16 sy, const Player *p)
{
    Entity *s = entity_spawn(ET_ENEMY_SHOT, sx, sy);
    if (!s) return;
    s->vy      = ESHOT_VY0;                              /* up -- the arc start    */
    s->facing  = (p->x < sx) ? 1 : 0;                    /* aim at the player      */
    s->vx      = s->facing ? (s16)(-ESHOT_VX) : (s16)ESHOT_VX;
    s->timer   = 180;                                    /* ~3s hard max-life      */
    s->launchY = sy;                                     /* land when it falls back */
}

/* ------------------------------------------------------------------------- */
/* Per-entity update. Returns nothing; sets e->active = 0 to despawn.
 * `p` is the live player (read-only) for AI/contact; the camera/score live in
 * g_game. */
static void entity_update_one(Entity *e, const Player *p)
{
    switch (e->type)
    {
        /* ----- enemyA: march LEFT toward the player, lane-tracked ----------
         * RE: sub_DC6C active dispatch -> sub_DC7D faces the player and picks a
         * leftward velocity; the body marches toward him. We keep it simple and
         * faithful: constant leftward creep, gently home onto the player's lane
         * (Y) so it "seeks" like the arcade's approach phase, animate the walk
         * cadence, and recycle when it scrolls well off the left (arcade DD52
         * off-screen recycle). Death (state 1) is a brief flash then despawn
         * (arcade DD78 death sequence). */
        case ET_ENEMY_A:
        {
            if (e->state == 1)             /* dying: play DEATH anim then despawn */
            {
                anim_set(&e->av, SS_ANIM_ENEMY_DEATH);
                e->tile = anim_tick(&e->av);
                if (--e->timer == 0) e->active = 0;
                break;
            }

            /* face + move toward the player's world X (the seek) */
            if (p->x < e->x) { e->facing = 1; if (e->vx > 0) e->vx = -e->vx; }
            else             { e->facing = 0; if (e->vx < 0) e->vx = -e->vx; }
            integrate(&e->x, &e->xSub, e->vx);

            /* home onto the player's lane Y one step at a time (lane-tracking
             * approach -- the arcade enemy shifts toward the player's tier). */
            if      (e->y < p->y) e->y += 1;
            else if (e->y > p->y) e->y -= 1;

            /* faithful walk cycle (captured arcade cadence) */
            anim_set(&e->av, SS_ANIM_ENEMY_WALK);
            e->tile = anim_tick(&e->av);

            /* recycle when it has marched well past the left edge of the view
             * (arcade DD52: off-screen-left recycle). Re-seed it ahead on the
             * right so the scene stays populated. */
            if (e->x < g_game.cameraX - 32)
            {
                e->x = (s16)(g_game.cameraX + SCREEN_W + 16);
                e->xSub = 0;
            }
            break;
        }

        /* ----- SHOT: travel in the facing direction; despawn off-screen -----
         * RE: the player shot ($0E28) is a fast linear projectile. */
        case ET_SHOT:
        {
            integrate(&e->x, &e->xSub, e->vx);
            integrate(&e->y, &e->ySub, e->vy);
            anim_step(e, 4);
            /* despawn when it leaves a generous world window around the view */
            if (e->x < g_game.cameraX - 16 ||
                e->x > g_game.cameraX + SCREEN_W + 16)
                e->active = 0;
            break;
        }

        /* ----- FLYER (classF $DF18/$DF90/$DFD9): glide an arc, then dive ------
         * RE: sub_DF90 adds a per-state velocity vector each frame (scripted
         * glide); sub_DF79 counts a telegraph then flips to the dive state whose
         * mover sub_DFD9 HOMES toward the player. We re-express two phases:
         *   state 0 GLIDE: drift left + a vertical sine bob (the arc), counting
         *      `timer` down; at 0 -> enter the dive.
         *   state 1 DIVE : steer forward toward the player's X + accelerate vy
         *      toward the player's Y (homing), like sub_DFD9's player-seek.
         *   state 2 DYING: play the death burst then despawn. */
        case ET_FLYER:
        {
            if (e->state == 2)                 /* dying */
            {
                anim_set(&e->av, SS_ANIM_FLYER_DEATH);
                e->tile = anim_tick(&e->av);
                if (--e->timer == 0) e->active = 0;
                break;
            }

            anim_set(&e->av, SS_ANIM_FLYER_FLY);
            e->tile = anim_tick(&e->av);
            e->phase++;                        /* drives the bob sine            */

            if (e->state == 0)                 /* GLIDE: arc in from the right   */
            {
                e->facing = 1;
                integrate(&e->x, &e->xSub, (s16)(-FLYER_GLIDE_VX));
                integrate(&e->y, &e->ySub, flyer_bob_vy(e->phase));
                if (--e->timer == 0)
                {
                    e->state = 1;              /* telegraph done -> DIVE          */
                    e->vy    = 0;
                }
            }
            else                               /* DIVE: home onto the player     */
            {
                /* forward toward the player's X (faithful sub_DFD9 chase) */
                if (p->x < e->x) { e->facing = 1; integrate(&e->x, &e->xSub, (s16)(-FLYER_DIVE_VX)); }
                else             { e->facing = 0; integrate(&e->x, &e->xSub, (s16)( FLYER_DIVE_VX)); }

                /* accelerate vy toward the player's Y, clamped (homing dive) */
                if (e->y < p->y) { e->vy += FLYER_DIVE_VY_STEP; if (e->vy >  FLYER_DIVE_VY_MAX) e->vy =  FLYER_DIVE_VY_MAX; }
                else             { e->vy -= FLYER_DIVE_VY_STEP; if (e->vy < -FLYER_DIVE_VY_MAX) e->vy = -FLYER_DIVE_VY_MAX; }
                integrate(&e->y, &e->ySub, e->vy);
            }

            /* recycle off the left edge: re-seed high on the right, gliding again */
            if (e->x < g_game.cameraX - 32)
            {
                e->x      = (s16)(g_game.cameraX + SCREEN_W + 16);
                e->y      = (s16)(LANE_BASE_Y - 5 * LANE_PITCH);   /* high lane    */
                e->xSub   = 0;
                e->ySub   = 0;
                e->vy     = 0;
                e->state  = 0;
                e->timer  = FLYER_GLIDE_FRAMES;
            }
            break;
        }

        /* ----- THROWER (enemyMain idx4 $BAB3 / mover $BB46): march + lob ------
         * RE: the thrower marches via the per-state velocity table (sub_BB46);
         * its sub-states clamp the approach within ~$30/$18 px of the player
         * (sub_BAC3/BAE5) then fire. We re-express:
         *   state 0 APPROACH: march toward the player until inside THROWER_RANGE.
         *   state 1 FIRE-STANCE: hold position and LOB a projectile every
         *      THROWER_FIRE_PERIOD frames (short throw-lunge anim each lob); if
         *      the player runs back out of range, resume the approach.
         *   state 2 DYING: death burst then despawn. */
        case ET_THROWER:
        {
            s16 gap;
            if (e->state == 2)                 /* dying */
            {
                anim_set(&e->av, SS_ANIM_THROWER_DEATH);
                e->tile = anim_tick(&e->av);
                if (--e->timer == 0) e->active = 0;
                break;
            }

            /* face the player; gap = horizontal distance to him */
            e->facing = (p->x < e->x) ? 1 : 0;
            gap = (s16)((p->x < e->x) ? (e->x - p->x) : (p->x - e->x));

            if (gap > THROWER_RANGE)           /* APPROACH: close the distance    */
            {
                e->state = 0;
                integrate(&e->x, &e->xSub,
                          e->facing ? (s16)(-THROWER_SPEED) : (s16)THROWER_SPEED);
                /* home onto the player's lane Y one step at a time */
                if      (e->y < p->y) e->y += 1;
                else if (e->y > p->y) e->y -= 1;
                anim_set(&e->av, SS_ANIM_THROWER_IDLE);
            }
            else                               /* FIRE-STANCE: in range -> lob    */
            {
                e->state = 1;
                if (e->fireTimer == 0 && --e->timer == 0)
                {
                    /* LOB from the thrower's mid-body toward the player */
                    thrower_lob((s16)(e->x + 4), (s16)(e->y + 4), p);
                    e->timer     = THROWER_FIRE_PERIOD;   /* next-lob cadence      */
                    e->fireTimer = THROWER_THROW_ANIM;    /* show the throw pose   */
                    anim_restart(&e->av, SS_ANIM_THROWER_THROW);
                }
                if (e->fireTimer == 0)
                    anim_set(&e->av, SS_ANIM_THROWER_IDLE);
            }

            if (e->fireTimer) e->fireTimer--;
            e->tile = anim_tick(&e->av);

            /* recycle off the left edge so the scene stays populated */
            if (e->x < g_game.cameraX - 32)
            {
                e->x     = (s16)(g_game.cameraX + SCREEN_W + 16);
                e->xSub  = 0;
                e->state = 0;
                e->timer = THROWER_FIRE_PERIOD;
            }
            break;
        }

        /* ----- ENEMY_SHOT (lobbed projectile classI $D352): parabolic arc -----
         * RE: launched up+toward the player, gravity (sub_D4AB) pulls it down ->
         * a parabola. We integrate vx (constant) + vy (gravity-accelerated) and
         * despawn when it falls back to its launch level (land, arcade sub_D446)
         * or leaves the view / max-life. */
        case ET_ENEMY_SHOT:
        {
            anim_set(&e->av, SS_ANIM_ESHOT_FLY);
            e->tile = anim_tick(&e->av);

            e->vy += ESHOT_GRAVITY;                        /* gravity -> the arc */
            integrate(&e->x, &e->xSub, e->vx);
            integrate(&e->y, &e->ySub, e->vy);

            /* land: once descending (vy>0) and fallen back to/below the launch Y */
            if (e->vy > 0 && e->y >= e->launchY) { e->active = 0; break; }
            /* off-screen / max-life safety despawn */
            if (e->timer && --e->timer == 0) { e->active = 0; break; }
            if (e->x < g_game.cameraX - 16 ||
                e->x > g_game.cameraX + SCREEN_W + 16 ||
                e->y > SCREEN_H + 16)
                e->active = 0;
            break;
        }

        /* ----- ITEM: stationary; collected on player touch (handled below) -- */
        case ET_ITEM:
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------------- */
/* Lethal player<->hostile contact (arcade DCAC). Raises a one-shot death REQUEST
 * that game.c consumes to run the life-loss flow, gated so one touch == one life:
 * only when NOT invulnerable (death/respawn i-frames, set by game.c) and not
 * already pending. Returns 1 if a fresh lethal contact was registered this call
 * (used by the projectile to consume itself on a hit), 0 otherwise. The brief
 * hurt-flash flag is raised for the on-contact blink; the life economy is game.c's.
 *
 * `en` must use the 16x16 BODY box; the caller does the projectile's tighter AABB
 * itself and calls this only on a confirmed overlap (we re-test the body box for
 * the full-size enemies). */
static u8 lethal_contact(const Player *p, const Entity *en)
{
    if (g_player_invuln || g_player_dead_request) return 0;

    /* For full-size enemies, re-test the body AABB here; the projectile path
     * already overlap-tested with its own box and passes an overlapping en. */
    if (en->type != ET_ENEMY_SHOT &&
        !aabb(p->x, p->y, PLAYER_W, PLAYER_H, en->x, en->y, BODY_W, BODY_H))
        return 0;

    g_player_dead_request = 1;                  /* game.c owns the consequence */
    if (g_player_hurt == 0) g_player_hurt = HURT_FRAMES;
    return 1;
}

/* Collision pass: SHOT<->ENEMY (defeat + score) and PLAYER<->ENEMY/ITEM. Runs
 * after motion so positions are current this frame. O(N*M) over a 24-slot pool
 * = trivial on the 68000. */
static void collision_pass(const Player *p)
{
    u16 i, j;

    /* SHOT vs ENEMY: a hit defeats the enemy and consumes the shot, awarding
     * score (arcade: a shot poking an enemy triggers its death sequence
     * sub_DD78). One loop handles every killable archetype -- the ground walker,
     * the flyer, and the thrower -- each with its own death-state index + score.
     * (The flyer/thrower use state 2 = dying; the walker uses state 1.) */
    for (i = 0; i < ENTITY_MAX; i++)
    {
        Entity *s = &g_entities[i];
        if (!s->active || s->type != ET_SHOT) continue;

        for (j = 0; j < ENTITY_MAX; j++)
        {
            Entity *en = &g_entities[j];
            u8  deathState;
            u32 award;

            /* select the killable archetypes + their death-state / score */
            if      (en->type == ET_ENEMY_A) { deathState = 1; award = SCORE_ENEMY;   }
            else if (en->type == ET_FLYER)   { deathState = 2; award = SCORE_FLYER;   }
            else if (en->type == ET_THROWER) { deathState = 2; award = SCORE_THROWER; }
            else continue;

            if (!en->active || en->state == deathState) continue;   /* dead/dying */

            if (aabb(s->x, s->y, SHOT_HALF_W * 2, SHOT_HALF_H * 2,
                     en->x, en->y, BODY_W, BODY_H))
            {
                if (en->hp > 1)
                {
                    en->hp--;
                }
                else
                {
                    en->state = deathState;        /* enter death sequence      */
                    en->timer = ENEMY_DEATH_FRAMES;
                    en->vx    = 0;
                    g_game.score += award;
                    snd_sfx_enemy();               /* enemy-defeat SFX (verified) */
                }
                s->active = 0;                      /* shot is consumed          */
                break;
            }
        }
    }

    /* PLAYER vs ENEMY/PROJECTILE (contact-hurt, arcade DCAC) + PLAYER vs ITEM
     * (collect). Every hostile body -- the ground walker, the flyer, the thrower,
     * AND the thrower's lobbed projectile -- costs a life on overlap. The lethal
     * request is gated to one life per touch (see lethal_contact). */
    for (j = 0; j < ENTITY_MAX; j++)
    {
        Entity *en = &g_entities[j];
        if (!en->active) continue;

        switch (en->type)
        {
            /* hostile bodies: lethal on contact unless already in their death
             * sequence (a dying/corpse enemy is harmless). */
            case ET_ENEMY_A:
                if (en->state != 1) lethal_contact(p, en);
                break;
            case ET_FLYER:
            case ET_THROWER:
                if (en->state != 2) lethal_contact(p, en);
                break;

            /* the lobbed projectile hurts the player, then is consumed */
            case ET_ENEMY_SHOT:
                if (aabb(p->x, p->y, PLAYER_W, PLAYER_H,
                         en->x, en->y, ESHOT_HALF_W * 2, ESHOT_HALF_H * 2))
                {
                    if (lethal_contact(p, en)) en->active = 0;  /* spent on a hit */
                }
                break;

            case ET_ITEM:
                if (aabb(p->x, p->y, PLAYER_W, PLAYER_H,
                         en->x, en->y, BODY_W, BODY_H))
                {
                    g_game.score += SCORE_ITEM;
                    snd_sfx_item();                  /* item-collect SFX (verified) */
                    en->active = 0;                  /* fruit collected            */
                }
                break;

            default:
                break;
        }
    }
}

/* ------------------------------------------------------------------------- */
void entity_update_all(void)
{
    const Player *p = &g_game.player;
    u16 i;

    if (g_player_hurt) g_player_hurt--;             /* decay the hurt window     */

    for (i = 0; i < ENTITY_MAX; i++)
    {
        Entity *e = &g_entities[i];
        if (e->active) entity_update_one(e, p);
    }

    collision_pass(p);
}

/* ------------------------------------------------------------------------- */
/* Render the visible pool into the SAT, appending after the player's sprite(s).
 *   sat       : the SAT word buffer (4 words per sprite)
 *   firstSlot : index of the first FREE sprite slot (e.g. 1, after the player)
 * Builds a properly linked chain among the appended sprites: sprite N links to
 * N+1, and the LAST one links to 0 (chain end). Returns the number appended.
 * On a nonzero return the caller links the player to `firstSlot`; on 0 it just
 * terminates the player's own link.
 *
 * Only on-screen, active entities are emitted, capped at ENTITY_SAT_MAX so we
 * stay clear of the 80-hardware-sprite ceiling and the per-scanline limit.
 * Entities draw in SCREEN space (world - camera), exactly like the player.
 * SAT word layout matches game.c's player sprite:
 *   word0 = (0x80 + screenY) & 0x3FF
 *   word1 = (SPRITE_SIZE(2,2) << 8) | nextLink
 *   word2 = TILE_ATTR_FULL(PAL1, prio, flipV, flipH, tile)
 *   word3 = (0x80 + screenX) & 0x1FF
 */
u16 entity_render_all(u16 *sat, u16 firstSlot)
{
    u16 count = 0;
    u16 slot  = firstSlot;
    u16 i;

    for (i = 0; i < ENTITY_MAX && count < ENTITY_SAT_MAX; i++)
    {
        Entity *e = &g_entities[i];
        s16 sx, sy;
        if (!e->active) continue;

        sx = (s16)(e->x - g_game.cameraX);
        sy = e->y;

        /* cull off-screen sprites (with a small margin) so the SAT stays lean */
        if (sx <= -BODY_W || sx >= SCREEN_W || sy <= -BODY_H || sy >= SCREEN_H)
            continue;

        /* (dying enemies no longer blink: the baked ENEMY_DEATH animation -- the
         * 307->308->309->310 collapse, driven in entity_update_one -- IS the
         * death feedback now, so we draw every frame of it.) */

        sat[slot * 4 + 0] = (u16)((0x80 + sy) & 0x3FF);
        /* link to the NEXT slot; the loop tail patches the final link to 0 */
        sat[slot * 4 + 1] = (u16)((SPRITE_SIZE(2, 2) << 8) | (slot + 1));
        sat[slot * 4 + 2] = (u16)TILE_ATTR_FULL(PAL1, 0, 0, e->facing, e->tile);
        sat[slot * 4 + 3] = (u16)((0x80 + sx) & 0x1FF);

        slot++;
        count++;
    }

    /* terminate the chain: the last appended sprite links to 0 */
    if (count) sat[(slot - 1) * 4 + 1] = (u16)((SPRITE_SIZE(2, 2) << 8) | 0);

    return count;
}

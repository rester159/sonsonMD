#include <genesis.h>
#include "game.h"
#include "entity.h"
#include "ss_gfx.h"

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

/* Score awards. */
#define SCORE_ENEMY         200         /* defeat a ground enemy                 */
#define SCORE_ITEM          500         /* collect a fruit                       */

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

/* Sprite tiles borrowed from the baked scene set (ss_gfx). These are real
 * arcade actor tiles captured in the scene; a single static cell per type is
 * fine for the slice (the anim STATE is driven for when frames get baked).
 *   ss_scene_sprites[13].tile (103) = a bank-9 lane actor   -> enemy body
 *   ss_scene_sprites[22].tile (127) = a small bank-1 sprite -> shot/item
 * TODO(faithful-anim): bake the enemy walk cycle + the shot frames and swap on
 *   anim/state instead of a single cell. */
#define ENEMY_TILE          (ss_scene_sprites[13].tile)
#define SHOT_TILE           (ss_scene_sprites[22].tile)
#define ITEM_TILE           (ss_scene_sprites[17].tile)   /* bank-2 actor (119) */

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

        switch (type)
        {
            case ET_ENEMY_A:
                e->hp     = ENEMY_HP;
                e->facing = 1;             /* enemyA marches LEFT (toward player) */
                e->vx     = (s16)(-(s16)enemy_march_speed());  /* difficulty-scaled */
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
            if (e->state == 1)             /* dying: flash then despawn          */
            {
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

            anim_step(e, ENEMY_ANIM_PERIOD);

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

        /* ----- ITEM: stationary; collected on player touch (handled below) -- */
        case ET_ITEM:
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------------- */
/* Collision pass: SHOT<->ENEMY (defeat + score) and PLAYER<->ENEMY/ITEM. Runs
 * after motion so positions are current this frame. O(N*M) over a 24-slot pool
 * = trivial on the 68000. */
static void collision_pass(const Player *p)
{
    u16 i, j;

    /* SHOT vs ENEMY: a hit defeats the enemy (-> death state) and consumes the
     * shot, awarding score (arcade: a shot poking an enemy triggers its death
     * sequence sub_DD78). */
    for (i = 0; i < ENTITY_MAX; i++)
    {
        Entity *s = &g_entities[i];
        if (!s->active || s->type != ET_SHOT) continue;

        for (j = 0; j < ENTITY_MAX; j++)
        {
            Entity *en = &g_entities[j];
            if (!en->active || en->type != ET_ENEMY_A) continue;
            if (en->state == 1) continue;          /* already dying             */

            if (aabb(s->x, s->y, SHOT_HALF_W * 2, SHOT_HALF_H * 2,
                     en->x, en->y, BODY_W, BODY_H))
            {
                if (en->hp > 1)
                {
                    en->hp--;
                }
                else
                {
                    en->state = 1;                 /* enter death sequence      */
                    en->timer = ENEMY_DEATH_FRAMES;
                    en->vx = 0;
                    g_game.score += SCORE_ENEMY;
                }
                s->active = 0;                      /* shot is consumed          */
                break;
            }
        }
    }

    /* PLAYER vs ENEMY (contact-hurt, arcade DCAC) + PLAYER vs ITEM (collect). */
    for (j = 0; j < ENTITY_MAX; j++)
    {
        Entity *en = &g_entities[j];
        if (!en->active) continue;

        if (en->type == ET_ENEMY_A && en->state != 1)
        {
            /* Lethal contact (arcade DCAC): an enemy body overlapping the player
             * costs a life -- but ONLY when the player is not in his death/respawn
             * i-frame window (g_player_invuln, set by game.c). We raise a one-shot
             * death REQUEST that game.c consumes to run the life-loss flow; the
             * invuln gate + the "already pending" guard make one touch == one
             * life. The brief hurt-flash flag is still raised for the on-contact
             * blink, but the life economy is entirely game.c's. */
            if (!g_player_invuln && !g_player_dead_request &&
                aabb(p->x, p->y, PLAYER_W, PLAYER_H, en->x, en->y, BODY_W, BODY_H))
            {
                g_player_dead_request = 1;          /* game.c owns the consequence */
                if (g_player_hurt == 0) g_player_hurt = HURT_FRAMES;
            }
        }
        else if (en->type == ET_ITEM)
        {
            if (aabb(p->x, p->y, PLAYER_W, PLAYER_H, en->x, en->y, BODY_W, BODY_H))
            {
                g_game.score += SCORE_ITEM;
                en->active = 0;                     /* fruit collected           */
            }
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

        /* dying enemies flash: blink (skip) every other frame of the death win */
        if (e->type == ET_ENEMY_A && e->state == 1 && (e->timer & 1))
            continue;

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

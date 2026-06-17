/* entity.h -- SonSon NATIVE entity system (Phase 4: entities/collision/scoring).
 *
 * A CLEAN native entity pool -- real C structs, world coords in pixels, direct
 * SAT output. This is deliberately NOT the arcade's 0x20-byte ram_lo record
 * overlaid on a 6809 RAM array (that is what the sibling transliteration port
 * does, and it is the ~5x "translation tax" the native rewrite exists to remove
 * -- see ROADMAP.md + the native-port doctrine). Instead we re-express the
 * arcade's entity SEMANTICS in a packed native struct the 68000 walks directly.
 *
 * RE ORACLE (behavioral reference, re-expressed here -- not transliterated):
 *   - Entity archetypes + spawn graph: ../sonson/census_semantic.toml
 *   - Faithful 0x20-byte record field layout: ../sonson/port/src/sonson_native.h
 *   - enemyA ground enemy ($0768): spawn sub_DC26->DC33, active AI sub_DC6C
 *     (-> DC7D march-toward-player / DC94 range-gate / DCAC player-contact),
 *     death sub_DD78. enemyA spawns at the RIGHT screen edge (worldX =
 *     scroll($90)+$00BE), marches LEFT toward the player, and on a ~16px body
 *     overlap pokes the player's hit-react state (DCAC). That is the archetype
 *     implemented first here.
 *   - Player FIRE ($0490 active sub_B1F3 -> sub_B25A): a debounced press spawns
 *     a shot entity ($0E28) LINKED to the player; the shot travels and despawns.
 *     Re-expressed as the SHOT entity below (forward travel in the facing dir).
 *
 * The pool, spawn/update/render, and collision are generic; per-type behavior
 * lives in entity.c. game.c owns the Player + camera and calls into here.
 */
#ifndef ENTITY_H
#define ENTITY_H

#include <genesis.h>
#include "anim_rt.h"     /* shared animation runtime (consumes baked ss_anim.h) */

/* ---- Entity types --------------------------------------------------------
 * The arcade has ~55 archetypes (census); these are the implemented native set.
 * Each ET_* re-expresses the SEMANTICS of an arcade handler (the oracle), not a
 * transliteration -- see the per-type AI in entity.c + the $-handler references.
 *
 * BATCH 2 added four more SPAWNED archetypes (census `spawn=true` families):
 *   ET_CREATURE_G (classG ground creature C6DA/C744/C7F2/C920/C9A0 -- walk + throw),
 *   ET_CREATURE_C (classC creature family BC20/BC6C/BC74 -- walker),
 *   ET_COMPOSITE_E (classE large entity C306/C350/C35E/F2B3 -- tough 2-HP walker),
 *   ET_HOPPER     (classH scrolling actor CF39/D0A2/D0B5/D02D -- leaping hopper).
 *
 * BATCH 3 (this pass): the iconic SonSon FRUIT VARIETY (ET_ITEM now carries a
 *   fruit TYPE: cherry/orange/apple/gold, each its own sprite colour + escalating
 *   point value + a floating "+N" score popup) PLUS three more SPAWNED archetypes:
 *   ET_LARGE   (obj16x16 $D57F/$D58B/$D5F3, seeded by $D54B: a TOUGH multi-HP
 *               patroller -- arcade HP 7, native 3; composite render is a TODO),
 *   ET_EMITTER (classD $BF6B/$BFE5/$BF75: a STATIONARY emplacement that
 *               periodically FIRES a projectile -- reuses the classI ENEMY_SHOT),
 *   ET_ORB     (objSingle $E1C4/$E1F5: a slow HOVERING lethal unique object).
 * TODO(more-archetypes): the literal multi-sprite COMPOSITE render for ET_LARGE /
 *   ET_COMPOSITE_E (arcade $F2B3 builds the body from a $F2F8 program-ROM sprite-
 *   assembly table -- NOT in the graphics-ROM bake, so it can't be reconstructed
 *   here yet; both render as a single faithful 16x16 actor for now). */
typedef enum
{
    ET_NONE = 0,    /* empty slot                                            */
    ET_ENEMY_A,     /* ground enemy -- marches left toward the player        */
    ET_SHOT,        /* player projectile -- travels in the facing direction  */
    ET_ITEM,        /* collectible FRUIT (bonus score on touch; e->fruit=type)*/
    ET_FLYER,       /* flyer (classF $DF18/$DF90): glides an arc, then dives  */
    ET_THROWER,     /* thrower (enemyMain $BAB3): marches, lobs projectiles   */
    ET_ENEMY_SHOT,  /* lobbed projectile (classI $D352): parabolic arc, hurts */
    ET_CREATURE_G,  /* classG ground creature ($C744 walk/$C7F2 throw): walks,
                     * periodically throws an arcing projectile (an ENEMY_SHOT) */
    ET_CREATURE_C,  /* classC creature ($BC6C family): a lane-seeking walker    */
    ET_COMPOSITE_E, /* classE large entity ($C35E walker): tough 2-HP lane walk */
    ET_HOPPER,      /* classH leaping hopper ($D0B5): ballistic leaps at player */
    ET_LARGE,       /* obj16x16 ($D57F/$D58B): tough 3-HP patroller            */
    ET_EMITTER,     /* classD ($BF6B/$BF75): stationary periodic shot emitter  */
    ET_ORB,         /* objSingle ($E1C4): slow hovering lethal unique object   */
    ET_POPUP        /* floating "+N" score popup (classK $0ED8 child, $CBC8)   */
} EntityType;

/* Fruit TYPES (ET_ITEM e->fruit). SonSon's signature scoring is EATING FRUIT:
 * one round fruit shape recoloured per type, with ESCALATING point values. The
 * value tiers are the arcade's bonus-number sprites (codes 404-415 = 100/300/
 * 500/1000/2000/10000); these four are the common collectibles. See entity.c
 * k_fruit_score[] + the FRUIT_* baked sprites. */
typedef enum
{
    FRUIT_CHERRY = 0,   /* red    -- 300                                       */
    FRUIT_ORANGE,       /* orange -- 500                                       */
    FRUIT_APPLE,        /* green  -- 1000                                      */
    FRUIT_GOLD,         /* gold   -- 2000                                      */
    FRUIT_TYPE_COUNT
} FruitType;

/* ---- Entity record (CLEAN native, packed) --------------------------------
 * Mirrors the arcade record's INTEGER behaviour (state / fixed-point position /
 * lane / anim cadence / hp), without the 6809 register file or ram_lo overlay.
 * World position is integer px + a 1/256-px sub-pixel accumulator on each axis
 * (the arcade stored 24-bit fixed-point X / 16-bit fixed-point Y; the sub-pixel
 * accumulators reproduce the same smooth fractional drift in native units). */
typedef struct
{
    u8  active;     /* 0 = free slot; nonzero = live (arcade record +0x00)   */
    u8  type;       /* EntityType                                            */
    s16 x, y;       /* world position (px), top-left of the 16x16 sprite     */
    u16 xSub, ySub; /* X/Y sub-pixel accumulators (1/256 px)                 */
    s16 vx, vy;     /* velocity (1/256 px per frame)                         */
    u8  lane;       /* lane index (enemies lane-align to the player field)   */
    u8  state;      /* per-type state-machine phase (arcade sub_state)       */
    u8  timer;      /* per-type countdown (death anim / fire cadence)        */
    u8  hp;         /* hit points (enemies); 0 => defeated                   */
    u8  facing;     /* 0 = faces/moves right, 1 = faces/moves left (flipH)   */
    u8  anim;       /* legacy anim-cell index (kept; AI state still uses it)  */
    u8  animTimer;  /* legacy counts down to the next anim step               */
    u8  phase;      /* free phase accumulator (FLYER vertical-bob sine index) */
    u8  fireTimer;  /* THROWER: frames left in the current throw-lunge pose   */
    s16 launchY;    /* ENEMY_SHOT: world-Y it was lobbed from (arc land test) */
    u8  fruit;      /* ET_ITEM: FruitType                                    */
    u8  popupLen;   /* ET_POPUP: glyph count in popupStr                     */
    u8  fromSched;  /* 1 = spawned by the faithful per-level wave scheduler  *
                     * (so it counts against the on-screen cap + despawns,   *
                     * never recycles, when it marches off the left edge --  *
                     * arcade $0828 wave enemy). 0 = ambient/projectile/item. */
    char popupStr[6];/* ET_POPUP: the "+N" text, formatted ONCE at spawn (so
                     * the per-frame render does zero intToStr/strlen work)   */
    Anim av;        /* faithful animation cursor (walk / death) -- enemyA     */
    u16 tile;       /* absolute VRAM base tile (SPRITE_SIZE(2,2))            */
} Entity;

/* Pool size. The arcade main array ($0768) is 55 slots; we cap the NATIVE pool
 * near that because the headline test is a BUSY scene held at 60fps, and the VDP
 * tops out at 80 sprites / 20 per scanline anyway (entity_render_all also caps
 * on-screen sprites). 48 holds the full seeded roster (now 11 enemy archetypes +
 * the FRUIT VARIETY) plus player shots, the enemy-lobbed/emitter projectiles, AND
 * the floating score POPUPs with headroom; the collision pass is still a trivial
 * O(N^2) byte scan on the 68000 (its inner loop early-outs on non-shots), and
 * only ENTITY_SAT_MAX of them ever reach the hardware sprite list. */
#define ENTITY_MAX          48

/* Hard cap on entities appended to the SAT in one frame (after the player).
 * Keeps us clear of the 80-hardware-sprite ceiling and the per-scanline limit
 * even if the pool is full and all on-screen. The native VDP slows when many
 * 16x16 sprites overlap the same scanlines (the per-scanline sprite limit), so
 * this cap is the headline-busy-scene's 60fps guard -- not the pool size. */
#define ENTITY_SAT_MAX      18

extern Entity g_entities[ENTITY_MAX];

/* ---- Faithful per-level WAVE SCHEDULER (arcade $F0C9 spawn tables) ---------
 * Replaces the old ad-hoc "seed a fixed wave + recycle off-left" spawner (which
 * let enemies ACCUMULATE on the right edge beyond what the arcade ever shows --
 * the measured cause of the sub-60 dips under load). The arcade spawns enemies
 * one at a time from a per-stage placement list (sub_F065/F019 -> sub_F080),
 * keyed to the scroll advancing, at worldX = scroll($90)+$F8 (right edge), into
 * the 16-slot $0828 pool, and STOPS when the live count hits the cap ($00A6 ==
 * 0x10). We re-express that: a scroll-position cursor walks the captured list and
 * spawns the next (row,type) at the right edge whenever the camera advances a
 * faithful spacing AND the live scheduled count is under SPAWN_ONSCREEN_CAP. See
 * tools/spawn_sched.json (the static RE of $F0C9, validated by live md=02
 * capture: arcade on-screen enemies avg ~2.6, peak 10).
 *
 * On-screen simultaneous cap for SCHEDULED wave enemy BODIES. The arcade hard cap
 * is the 16-slot pool ($00A6 != 0x10 gate); live md=02 play PEAKS at exactly 10
 * (tools/arch_census.txt + spawn_cap_L0.txt). We cap at 10 -- the arcade's true
 * observed maximum, so the busy scene matches the arcade AND the co-resident body
 * count (bodies + their lobbed projectiles + the player's shots) stays inside the
 * VDP per-scanline sprite budget at the dense flyer/hopper/projectile worst case
 * (a clean 60fps -- a cap of 12 grazed 59 in a transient burst; see arch_deepfps).
 * The lobbed projectiles the creatures/throwers fire are NOT scheduled bodies, so
 * they don't count against this cap (they are children, like the arcade $0DA8). */
#define SPAWN_ONSCREEN_CAP  10

void entity_schedule_init(void);              /* (re)arm the scheduler for the   *
                                               * current round (game.c round set)*/
void entity_schedule_tick(void);              /* per-frame: spawn scheduled waves*/

/* ---- Lifecycle (called from game.c) -------------------------------------- */
void entity_init(void);                       /* clear the pool + spawn waves  */
Entity *entity_spawn(u8 type, s16 x, s16 y);  /* allocate + init a slot (or 0) */
void entity_update_all(void);                 /* AI + motion + collision + score*/

/* Append the visible pool to the SAT, starting at sprite slot `firstSlot`
 * (e.g. 1, right after the player at slot 0). Builds a properly linked chain
 * among the appended sprites (each links to the next; the LAST links to 0 to
 * terminate). Returns the number of sprites appended; on a nonzero return the
 * caller should link the player's sprite to `firstSlot`, otherwise terminate
 * the player's link with 0.
 *
 * Appends BOTH the entity bodies (16x16) AND the floating "+N" score-popup glyphs
 * (1x1 font-tile sprites) in one chain. Popups carry no body tile -- they're
 * emitted as SGDK font glyphs (PAL3), so they cost no extra active-display VRAM
 * writes (they ride the same SAT DMA). Bodies are capped at ENTITY_SAT_MAX and the
 * popup glyphs at ENTITY_POPUP_GLYPH_MAX, so the SAT buffer must hold
 * 1 + ENTITY_SAT_MAX + ENTITY_POPUP_GLYPH_MAX sprites. */
#define ENTITY_POPUP_GLYPH_MAX  10
u16  entity_render_all(u16 *sat, u16 firstSlot);

/* ---- Player-driven hooks (game.c calls these) ---------------------------- */
/* Spawn a player shot from the player's position/facing (the arcade B25A fire).
 * Debouncing (fire-once-per-press) is handled in game.c at the input edge. The
 * arcade allows only ONE player shot at a time ($0E28 single slot): returns TRUE
 * iff a shot was actually fired (FALSE while a prior shot is still live). */
bool entity_player_fire(s16 px, s16 py, u8 facing);

/* Player hit/flash state, raised by player<->enemy contact (arcade DCAC poke).
 * game.c reads g_player_hurt to flash the sprite during the hurt window. It
 * decays on its own timer inside entity_update_all. (The hurt-flash is the
 * brief on-contact blink; the LIFE-LOSS / death state is owned by game.c and
 * driven off g_player_dead_request below.) */
extern u8 g_player_hurt;     /* >0 => player is in the hurt-flash window      */

/* ---- Game-flow bridge between the entity collision pass and game.c --------
 * The collision pass (entity.c) detects PLAYER<->ENEMY contact but does NOT own
 * the life economy -- that is game.c's job (lives / death anim / respawn /
 * game-over). The bridge is two one-byte globals:
 *
 *   g_player_invuln  -- WRITTEN by game.c, READ by the collision pass. Nonzero
 *       while the player is in a death/respawn i-frame window; collision then
 *       ignores enemy contact entirely (no hurt, no death request). Re-expresses
 *       the arcade respawn-grace timer (sub_B1C2 parks +$D = #$20 on (re)spawn).
 *
 *   g_player_dead_request -- WRITTEN (set to 1) by the collision pass on a fresh
 *       lethal contact, READ + CLEARED by game.c, which then runs the death ->
 *       respawn / game-over flow. A request is only raised when NOT invulnerable
 *       and not already pending, so one contact == one life lost.
 *
 * This keeps the entity pool free of game-state knowledge (clean separation)
 * while letting a single enemy touch cost exactly one life, faithfully. */
extern u8 g_player_invuln;        /* set by game.c: 1 => contact is ignored    */
extern u8 g_player_dead_request;  /* set by entity.c on lethal contact         */

#endif /* ENTITY_H */

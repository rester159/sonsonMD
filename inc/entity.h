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

/* ---- Entity types --------------------------------------------------------
 * Only the Phase-4 vertical-slice set. The arcade has ~55 archetypes (census);
 * more drop in as new ET_* cases in entity_update_one / the spawn table.
 * TODO(more-archetypes): flyer (classF DF18), thrower (enemyMain BAB3),
 *   lobbed projectile (classI D352), fruit/item pickups. */
typedef enum
{
    ET_NONE = 0,    /* empty slot                                            */
    ET_ENEMY_A,     /* ground enemy -- marches left toward the player        */
    ET_SHOT,        /* player projectile -- travels in the facing direction  */
    ET_ITEM         /* collectible fruit (bonus score on player touch)       */
} EntityType;

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
    u8  anim;       /* anim-cell index (drives frame select; cadence below)  */
    u8  animTimer;  /* counts down to the next anim step                     */
    u16 tile;       /* absolute VRAM base tile (SPRITE_SIZE(2,2))            */
} Entity;

/* Pool size. The arcade main array ($0768) is 55 slots; we cap the NATIVE pool
 * smaller because the headline test is a BUSY scene held at 60fps, and the VDP
 * tops out at 80 sprites / 20 per scanline anyway (entity_render_all also caps
 * on-screen sprites). 24 is plenty of simultaneous enemies + shots + items. */
#define ENTITY_MAX          24

/* Hard cap on entities appended to the SAT in one frame (after the player).
 * Keeps us clear of the 80-hardware-sprite ceiling and the per-scanline limit
 * even if the pool is full and all on-screen. */
#define ENTITY_SAT_MAX      20

extern Entity g_entities[ENTITY_MAX];

/* ---- Lifecycle (called from game.c) -------------------------------------- */
void entity_init(void);                       /* clear the pool + spawn waves  */
Entity *entity_spawn(u8 type, s16 x, s16 y);  /* allocate + init a slot (or 0) */
void entity_update_all(void);                 /* AI + motion + collision + score*/

/* Append the visible pool to the SAT, starting at sprite slot `firstSlot`
 * (e.g. 1, right after the player at slot 0). Builds a properly linked chain
 * among the appended sprites (each links to the next; the LAST links to 0 to
 * terminate). Returns the number of sprites appended; on a nonzero return the
 * caller should link the player's sprite to `firstSlot`, otherwise terminate
 * the player's link with 0. Only on-screen entities are emitted, capped at
 * ENTITY_SAT_MAX. */
u16  entity_render_all(u16 *sat, u16 firstSlot);

/* ---- Player-driven hooks (game.c calls these) ---------------------------- */
/* Spawn a player shot from the player's position/facing (the arcade B25A fire).
 * Debouncing (fire-once-per-press) is handled in game.c at the input edge. */
void entity_player_fire(s16 px, s16 py, u8 facing);

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

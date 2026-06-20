#include <genesis.h>
#include "game.h"
#include "entity.h"
#include "ss_gfx.h"
#include "ss_anim.h"     /* baked player/enemy animation frames + table */
#include "ss_collision.h"/* SS_TIER_Y[6] -- the 6 terrain tiers (native sub_A847) */
#include "sound.h"       /* snd_sfx_enemy / snd_sfx_item (verified $3010 cmds) */
#include "ss_wave_sched.h"/* arcade $FCD9 87-wave spawn schedule (RE'd: $EF89/$EFBB) */

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
 *
 *  BATCH 2 -- four more SPAWNED archetypes (census `spawn=true` families), AI
 *  re-expressed from their RE handlers (the leaf movers under each class's
 *  dispatch table; the transliterated jump-tables in ../sonson/port/src are the
 *  oracle for which leaf does what):
 *    - CREATURE_G (classG, spawn $C6DA, idle $C6EA, walk/patrol $C744, attack/
 *      throw $C7F2, hop $C920, death $C9A0): a ground creature that walks toward
 *      the player and, in range, periodically THROWS an arcing projectile. The
 *      attack handler $C7F2/$C80D sets a child-spawn (stb $D,x=#$10; inc -$E,x)
 *      -- it spawns a projectile child into the $0ED8 pool (census: "spawns a
 *      projectile into $0ED8"). We re-express: march toward the player; within
 *      range, lob an ENEMY_SHOT on a cadence (reusing the classI projectile).
 *      Distinct from THROWER (a creature, not the humanoid thrower). One shot kills.
 *    - CREATURE_C (classC, spawn $BC20, family dispatch $BC6C, walk $BC74): a
 *      creature-family member; the idx1 sub-dispatch ($BC6C) routes the "flyer"
 *      family but the on-screen body the spawner places is the walker ($BC74).
 *      We re-express the common spawned form: a lane-seeking ground walker (like
 *      enemyA but a distinct creature sprite + a gentle vertical wander). 1 shot.
 *    - COMPOSITE_E (classE, on-screen-gate spawn $C306, walker AI $C350/$C35E,
 *      multi-sprite emitter $F2B3): a LARGE composite entity. $C35E reads the
 *      lane byte (<$24), parity-gates, and loads a $0200 (2.0px) horizontal
 *      velocity -- a lane-walker that periodically shifts. Being "large/composite"
 *      it is the TOUGHER archetype: 2 HP (two shots to kill). We re-express a
 *      brisk lane walker that takes two hits. (The literal 4-sprite composite
 *      render is a later task -- TODO; here it is one 16x16 body.)
 *    - HOPPER (classH, spawn $CF39, run $CF4E, tracking-walk $D02D, leaping
 *      hopper $D0A2, effect $D122): a grounded AI that LEAPS in ballistic arcs.
 *      The hop leaf $D0B5 adds a per-frame velocity delta to the 16-bit Y accum
 *      (ldd -$8,x; addd $17,u; std -$8,x), checks a land bound (jsr $A847; bcs ->
 *      recover state idx6), and refreshes a $0400 (4.0px) horizontal velocity
 *      (ldd #$0400; std -$F,x). We re-express: a grounded creature that springs
 *      up+forward toward the player (gravity arc), lands on its lane, pauses, and
 *      leaps again. Body contact hurts. One shot kills it (in the air or grounded).
 *
 *  BATCH 3 -- the iconic FRUIT VARIETY + three more SPAWNED archetypes:
 *    - FRUIT (ET_ITEM): SonSon's signature scoring is EATING FRUIT. One round
 *      fruit shape recoloured into four types (cherry/orange/apple/gold) with
 *      ESCALATING point values (k_fruit_score[] = 300/500/1000/2000 -- the
 *      arcade bonus-number tiers, codes 404-415). On collect: score by type +
 *      the item SFX (snd_sfx_item, cmd 0x02) + a floating "+N" POPUP (the arcade
 *      classK $0ED8 score-child, sub_CBC8/sub_CCB1, re-expressed as a cheap
 *      drifting HUD-font label -- see ET_POPUP).
 *    - LARGE (obj16x16, root $D57F -> init $D58B, move $D5F3, seeded by $D54B):
 *      a TOUGH multi-HP patroller. $D58B sets HP $8,x=#$07 + a 16px box; $D5F3
 *      counts a timer then reloads a velocity vector from the $DB4B table = a
 *      periodic-velocity march that reverses on a cadence. We re-express: a
 *      brisk lane patroller that periodically flips direction, taking 3 hits
 *      (native HP 3 vs arcade 7 -- a fair first cut; see TODO). The literal
 *      4-sprite composite body ($F2B3 builds it from the $F2F8 program-ROM
 *      sprite table -- not in the gfx bake) is a TODO; here it is one 16x16.
 *    - EMITTER (classD, root $BF6B idx0, periodic-fire idx2 $BFE5->$C00F, launch
 *      leaf $BF75): a STATIONARY emplacement. $BF75 counts a fire-countdown
 *      $D,x down and at 0 spawns a projectile child (stb -$10,x=1; inc -$F,x)
 *      then reloads. We re-express: a fixed insect that hovers in place and
 *      FIRES a horizontal shot at the player every EMIT_FIRE_PERIOD frames
 *      (reusing the classI ENEMY_SHOT, launched flat). One shot kills it.
 *    - ORB (objSingle, spawn $E1C4 -> behaviour $E1F5): a parent-linked unique
 *      object that settles relative to the scroll + runs its own anim. We
 *      re-express a slow HOVERING lethal hazard: it drifts toward the player
 *      with a vertical bob, lethal on contact, kept visually distinct from the
 *      COLLECTIBLE fruit by its motion (never collected). One shot kills it.
 * =========================================================================== */

Entity g_entities[ENTITY_MAX];

/* Player hit/life-loss state is now PER-PLAYER (Player.hurt / .invuln / .deadReq
 * in game.h) so each co-op hero has an independent hurt-flash + death request.
 * game.c owns the life economy; collision_pass sets each hit player's deadReq. */

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

/* ---- CREATURE_G (classG $C744 walk / $C7F2 throw) tunables -----------------
 * A ground creature that marches toward the player (like enemyA) and, once the
 * horizontal gap closes inside CREG_RANGE, periodically LOBS a projectile (its
 * $C7F2 child-spawn) -- reusing the classI ENEMY_SHOT. It keeps marching while it
 * throws (a creature, not the stand-and-fire humanoid thrower). One shot kills. */
#define CREG_HP             1
#define CREG_SPEED          0x00E0      /* ~0.875 px/frame march toward the player */
#define CREG_RANGE          140         /* start lobbing once within this gap (px) */
#define CREG_FIRE_PERIOD    90          /* frames between throws (~1.5s cadence)   */
#define CREG_THROW_ANIM     12          /* frames the throw-lunge pose shows        */

/* ---- CREATURE_C (classC $BC6C family / $BC74 walk) tunables -----------------
 * A creature-family walker: tracks the player on X on the ground like enemyA and
 * rides its spawn tier (the arcade classC walker sub_BC74 has no player-Y term --
 * see the seat_on_tier RE; the old vertical "wander" was unfaithful, removed).
 * One shot kills it. */
#define CREC_HP             1
#define CREC_SPEED          0x0110      /* ~1.06 px/frame seek                      */

/* ---- COMPOSITE_E (classE $C35E walker) tunables ----------------------------
 * The TOUGHER archetype: a brisk lane walker that takes TWO shots (the arcade's
 * "large/composite" entity). $C35E loads a $0200 (2.0px) horizontal velocity on
 * its parity gate; we march it at COMPE_SPEED and give it COMPE_HP = 2. */
#define COMPE_HP            2           /* large composite: two shots to kill       */
#define COMPE_SPEED         0x00D0      /* ~0.81 px/frame (heavier than the walker)  */

/* ---- HOPPER (classH $D0B5 leaping hopper) tunables -------------------------
 * Springs up+forward toward the player in a gravity arc, lands on its lane,
 * pauses, then leaps again (the $D0B5 ballistic state: per-frame Y accel + a
 * $0400 horizontal velocity refresh + a land check). Re-expressed with a native
 * gravity vy and a forward vx set at each launch; lands when it falls back to the
 * launch lane Y, waits HOP_LAND_PAUSE, then re-launches. One shot kills it. */
#define HOP_HP              1
#define HOP_GRAVITY         0x0020      /* per-frame downward accel (1/256 px)      */
#define HOP_VY0             (s16)(-0x0320) /* initial UP velocity (~-3.1 px/frame)  */
#define HOP_VX              0x0140      /* forward speed toward the player (~1.25px) */
#define HOP_LAND_PAUSE      26          /* frames grounded between leaps (~0.43s)   */

/* ---- LARGE (obj16x16 $D57F/$D58B/$D5F3) tunables ---------------------------
 * The arcade large object inits HP $8,x=#$07 and a 16px box, then $D5F3 counts a
 * timer down and reloads a velocity vector from the $DB4B table on each expiry --
 * a periodic-velocity patrol. We re-express a brisk lane patroller that FLIPS its
 * march direction every LARGE_TURN frames (the periodic velocity reload), taking
 * LARGE_HP hits. NATIVE HP is 3, not the arcade 7 -- a fair first cut so it reads
 * as the tough archetype without becoming a bullet-sponge slog (see TODO). */
#define LARGE_HP            3           /* tough: three shots (arcade $8,x=#$07 = 7) */
#define LARGE_SPEED         0x00B0      /* ~0.69 px/frame heavy patrol march         */
#define LARGE_TURN          64          /* frames between march-direction flips      */

/* ---- EMITTER (classD $BF6B/$BFE5/$BF75) tunables ---------------------------
 * The arcade emitter ($BF75) counts a fire-countdown $D,x down and on 0 spawns a
 * projectile child then reloads. We re-express a STATIONARY emplacement that
 * FIRES a flat horizontal shot at the player every EMIT_FIRE_PERIOD frames (a
 * short EMIT_FIRE_ANIM lunge each shot). One shot kills it. */
#define EMIT_HP             1
#define EMIT_FIRE_PERIOD    80          /* frames between shots (~1.33s cadence)     */
#define EMIT_FIRE_ANIM      14          /* frames the fire pose shows                */
#define EMIT_SHOT_VX        0x0260      /* flat shot speed toward the player (~2.4px)*/

/* ---- ORB (objSingle $E1C4/$E1F5) tunables ----------------------------------
 * A slow HOVERING lethal hazard: drifts toward the player with a vertical bob.
 * Kept visually distinct from the collectible fruit by this motion (it is never
 * collected -- it hurts). One shot kills it. */
#define ORB_HP              1
#define ORB_SPEED           0x0080      /* ~0.5 px/frame slow leftward drift (scroll)*/
#define ORB_BOB_PIX         10          /* +-px of the sine hover-bob around spawn Y  */

/* ---- FRUIT (ET_ITEM) point values -- the SonSon SIGNATURE scoring ----------
 * EATING FRUIT is SonSon's headline mechanic. One round fruit shape recoloured
 * into four types with ESCALATING values.
 *
 * RE (now arcade-EXACT, 2026-06-17): the arcade's score-award amounts come from a
 * BCD addend table at ROM $4D84 (decoded from rom/ss.01e; the score-add core that
 * indexes it -- bcd_score_add @ $4A96/$4CBE -- is Capcom's SWI-obfuscated routine,
 * but the DATA table is plaintext). The table holds stored-BCD tiers 10,20,30,40,
 * 50,60,70,80 then 1000,2000,...  The on-screen score is the stored value x10 (the
 * arcade default high score is the documented 100,000 = stored BCD $00010000 = 10000,
 * confirming the x10 display). So the award TIERS the game can show are
 * 100/200/.../800 and 10000/20000/...  The player-facing FRUIT popups are exactly
 * the documented bonus-number sprite tiers (codes 404-415 = 100/300/500/1000/2000/
 * 10000). The four collectible fruit map to 300/500/1000/2000 -- four of those exact
 * popup tiers (unchanged from the prior values, now RE-CONFIRMED against $4D84 + the
 * 404-415 sprite set, no longer "best candidate"). Indexed by FruitType. */
static const u16 k_fruit_score[FRUIT_TYPE_COUNT] =
{
    300,    /* FRUIT_CHERRY (red)    -- bonus-tile/$4D84 tier (disp 300)            */
    500,    /* FRUIT_ORANGE (orange) -- bonus-tile/$4D84 tier (disp 500)            */
    1000,   /* FRUIT_APPLE  (green)  -- bonus-tile tier (disp 1000)                 */
    2000    /* FRUIT_GOLD   (gold)   -- bonus-tile tier (disp 2000)                 */
};
/* Baked sprite tile per fruit type (the FRUIT_* anim frame-0 tiles). */
static const u8 k_fruit_anim[FRUIT_TYPE_COUNT] =
{
    SS_ANIM_FRUIT_CHERRY, SS_ANIM_FRUIT_ORANGE,
    SS_ANIM_FRUIT_APPLE,  SS_ANIM_FRUIT_GOLD
};

/* ---- POPUP (floating "+N" score, the classK $0ED8 score-child re-expressed) -
 * The arcade spawns a child into the $0ED8 pool on a kill/pickup that draws the
 * floating point value (sub_CBC8 animates it, sub_CCB1 dispatches its states).
 * We re-express it cheaply as a short-lived entity that DRIFTS UPWARD and is
 * drawn as a HUD-font "+N" label by game_render (not a hardware sprite -- it
 * carries no tile). POPUP_LIFE frames, rising POPUP_RISE px total. */
#define POPUP_LIFE          34          /* ~0.57s on screen (short -> few co-resident)*/
#define POPUP_RISE_VY       (s16)(-0x0100) /* ~1.0 px/frame upward drift             */

/* ---- ENEMY point values -- MEASURED from the live arcade (displayed points) -----
 * The score-add core (bcd_score_add @ $4A96) is Capcom's SWI-obfuscated routine with
 * zero static callers, so per-archetype values could not be pinned statically. They
 * were instead MEASURED empirically: drive the MAME `sonson` driver into a credited
 * game (DSW1 Coinage=0x0F, wait md@$0002==1, pulse :SYSTEM Coin1 then 1-Player-Start,
 * md->0x02), SAVE a mid-game state, then read the per-kill BCD score DELTA at maincpu
 * $006C-$006F. NB: the score memory holds the DISPLAYED value 1:1 -- proven by dumping
 * the HUD digit tilemap (videoram) right after a natural award: stored 0x00000150 ->
 * on-screen "150", 0x00000490 -> "490". (The "x10" relationship is only in the $4D84
 * ADDEND table -- an addend byte 0x10 adds 100 to the displayed score; it is NOT a
 * factor on the $006C-6F score itself.) So a $006C-6F delta == displayed points awarded.
 *
 * MEASURED (high confidence):
 *   - GROUND WALKER (enemyMain $0828, sprite bank2, death codes 304-315) = 150.
 *       Isolated cleanly: in the early walker-only segment (classF/G/H pools empty) every
 *       score delta with only bank2 on screen was +150 (8 isolated kills); across deep
 *       play every +150 award coincided with a bank2 sprite entering its death-code range.
 *   - THROWER (enemyMain $0828 idx4 $BAB3 -- SAME pool & sprite bank as the walker) = 150.
 *       Every bank2 death awarded exactly 150 (no bank2 kill ever deviated), and the
 *       thrower lives in the walker's pool, so it shares the 150 award.
 *   - FLYER (classF $0BA8, sprite bank9) = 50.
 *       Isolated by the death signal: every +50 award occurred while a flyer (bank9/15)
 *       was on screen AND no walker was in its death-code range ("dyingBanks" empty),
 *       i.e. no walker kill could account for it -- 26+ such events, all +50.
 *
 * REMAINING ARCHETYPES -- MEASURED 2026-06-17 by FORCING single-type spawns in arcade `sonson`
 * (the never-spawning pools classC/D/E/G never appear in any reachable forward run -- proven by
 * arch_stage_*.txt, max=0 -- so they were FORCED). The arcade wave system has, per enemy CLASS, a
 * dedicated wave-control RECORD whose state-1 handler is that class's batch SPAWNER (decoded from
 * ss.03e: $0648->F2AC=classC, $0658->F39B->F3A1=classD($0AE8), $0668->F479->F47F=classE($0B68),
 * $0678->classF, $0688->F838->F83E=classH($0CE8), $0698->F6C5->F6CB=classG($0C28)). Writing
 * record+0=1 (active), record+2=1 (state) makes that class's REAL spawner emit a full, 100%-game-
 * correct batch (correct sprite/HP/collision/death/score-tier). Isolation harness (tools/mame/
 * ss_id_measure.lua): zero every OTHER enemy pool each frame so only the forced class can be
 * killed+score, drive the player to REALLY shoot them (a state-poke "kill" does NOT award -- the
 * score fires from the live shot-collision, confirmed), and histogram the $006C-6F deltas while
 * the target pool is the SOLE populated enemy pool. The per-pool deltas came out MUTUALLY
 * EXCLUSIVE (a strong signal they're real per-archetype awards, not background): classC->+50,
 * classD->+100, classG->+150(+a +30 secondary), classH->+150. Validation: the SAME harness on the
 * walker (enemyMain, no force) reproduces +150 and on the flyer (classF) +50 -- matching the
 * committed values. NB: a periodic +500/+1000 aggregate (progress/aggregate bonus) pollutes deep
 * windows; measurements taken from the clean early window + sole-pool gate exclude it.
 *
 * MEASURED this pass (force method + $006C-6F delta + attribution):
 *   - CREATURE_C (classC $0A28, creature sprite bank5 codes 16-24) = 50. Forced via record $0648;
 *       SOLE +50 x13 (count visibly 6->5->4 per +50), mutually exclusive vs every other pool.
 *   - EMITTER (classD $0AE8, $BF75) = 100. Forced via record $0658; SOLE +100 x6 (dominant,
 *       classD-exclusive). Smaller sample (classD only 2 live) -> medium confidence.
 *   - CREATURE_G (classG $0C28, $C744) = 150. Forced via record $0698; SOLE shows +150 (x7-11)
 *       as the kill award alongside a recurring +30 secondary (unexplained sub-event; classG
 *       death anim holds the slot so a per-kill count-drop couldn't be pinned). 150 = the shared
 *       ground-enemy tier (walker/thrower/hopper all 150) -> medium confidence; +30 left unused.
 *   - HOPPER (classH $0CE8, $D0B5/$D02D) = 150. classH DOES spawn naturally; SOLE +150 x4 with the
 *       classH count visibly dropping 6->5->4 per +150 (codes 304-315 = the classH actor) -> high
 *       confidence. (HOPPER was the "leaping" classH leaf; shares the classH/ground-enemy tier.)
 *
 * STILL UNMEASURED (could NOT be cleanly forced -- kept at documented tier, SAID SO):
 *   - COMPOSITE_E (classE $0B68): arming record $0668 did NOT populate classE in reachable stages
 *       (the F443->F479->F47F path has an extra gate / needs specific level data); 2-slot pool
 *       never became sole-occupied. Kept at 500.
 *   - LARGE (obj16x16 $0E48): spawner sub_D54B is called from ONE site ($A619) gated by level
 *       data, NOT by an armable wave record -> not forceable headlessly. Kept at 800.
 *   - ORB (objSingle $0E28): spawn $E1C4 has zero static callers (computed dispatch / parent-link)
 *       -> no armable trigger. Kept at 300.
 * TODO(arcade-exact): COMPOSITE_E/LARGE/ORB need an .inp playthrough or a context that spawns them
 * (level-script driven), and the classG +30 secondary wants explaining. */
#define SCORE_ENEMY         150         /* ground walker: MEASURED arcade award  (disp 150) */
#define SCORE_FLYER          50         /* flyer (classF/bank9): MEASURED        (disp 50)  */
#define SCORE_THROWER       150         /* thrower (enemyMain idx4, walker pool): MEASURED 150 */
#define SCORE_CREATURE_G    150         /* classG creature: MEASURED (force rec $0698, +150)  */
#define SCORE_CREATURE_C     50         /* classC creature: MEASURED (force rec $0648, +50 x13)*/
#define SCORE_COMPOSITE_E   500         /* classE: UNMEASURED (arm $0668 didn't spawn) -- documented */
#define SCORE_HOPPER        150         /* classH hopper: MEASURED (SOLE +150, count-drop)     */
#define SCORE_LARGE         800         /* obj16x16: UNMEASURED (level-gated spawn) -- documented */
#define SCORE_EMITTER       100         /* classD emitter: MEASURED (force rec $0658, +100 x6) */
#define SCORE_ORB           300         /* objSingle: UNMEASURED (computed-dispatch spawn) -- documented */
/* (ET_ITEM/fruit score is per-type, k_fruit_score[]; no flat SCORE_ITEM.)      */

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

/* ---- Per-STAGE difficulty ramp (arcade $005A, saturating) -----------------
 * Each stage nudges the enemies up a notch on TOP of the difficulty base, so the
 * game gets subtly harder as you travel. We add ROUND_SPEED_STEP (1/256-px/frame)
 * per stage to the march speed, capped at ROUND_SPEED_MAX_BONUS so even a deep run
 * keeps the chasers under the player's ~2px/frame run (he can always out-pace a
 * single one). stage 0 adds nothing (the base feel). Because g_game.stage SATURATES
 * at STAGE_MAX (game.c), the ramp plateaus exactly like the arcade -- it never runs
 * away no matter how long you survive. */
static u16 round_speed_bonus(void)
{
    u16 s = g_game.stage;                 /* saturating $005A (0..STAGE_MAX)        */
    u16 bonus = (u16)(s * ROUND_SPEED_STEP);
    if (bonus > ROUND_SPEED_MAX_BONUS) bonus = ROUND_SPEED_MAX_BONUS;
    return bonus;
}

/* Resolve the current enemy march speed from g_config.difficulty (clamped) plus
 * the per-round ramp bonus. Read at spawn/recycle time so a fresh wave each round
 * picks up that round's pace. */
static u16 enemy_march_speed(void)
{
    u8 d = g_config.difficulty;
    if (d > 3) d = 3;
    return (u16)(k_enemy_speed_by_diff[d] + round_speed_bonus());
}

/* Thrower fire cadence shortens slightly per stage (more frequent lobs = the
 * thrower's difficulty ramp; its threat is the projectiles, not its march). 4
 * frames quicker per stage, floored so it never machine-guns. stage saturates at
 * STAGE_MAX so the cadence plateaus (arcade-faithful, never runs away). */
#define THROWER_FIRE_PERIOD_MIN  34         /* never lob faster than this (frames) */
static u8 thrower_fire_period(void)
{
    u16 s = g_game.stage;                 /* saturating $005A                       */
    s16 p = (s16)(THROWER_FIRE_PERIOD - (s16)(s * 4));
    if (p < THROWER_FIRE_PERIOD_MIN) p = THROWER_FIRE_PERIOD_MIN;
    return (u8)p;
}

/* The ENEMY now ANIMATES: its tile each frame comes from the baked faithful
 * animation set (ss_anim.c -- ENEMY_WALK / ENEMY_DEATH), driven by the Anim
 * cursor e->av. ENEMY_TILE is just the cold-start frame (walk frame 0).
 *
 * The player SHOT now animates off the baked ESHOT graphic (the arcade player
 * bullet = sprite codes 70/71 bank 2, RE-confirmed -- see ET_SHOT spawn). */
#define ENEMY_TILE          (ss_anims[SS_ANIM_ENEMY_WALK].frames[0].tile)
/* New archetypes animate off baked anim sets (cold-start = frame 0). */
#define FLYER_TILE          (ss_anims[SS_ANIM_FLYER_FLY].frames[0].tile)
#define THROWER_TILE        (ss_anims[SS_ANIM_THROWER_IDLE].frames[0].tile)
#define ESHOT_TILE          (ss_anims[SS_ANIM_ESHOT_FLY].frames[0].tile)
#define ELOB_TILE           (ss_anims[SS_ANIM_ELOB].frames[0].tile)  /* classI enemy lob */
/* Batch-2 archetypes (cold-start = walk/hop frame 0). */
#define CREG_TILE           (ss_anims[SS_ANIM_CREG_WALK].frames[0].tile)
#define CREC_TILE           (ss_anims[SS_ANIM_CREC_WALK].frames[0].tile)
#define COMPE_TILE          (ss_anims[SS_ANIM_CMPE_WALK].frames[0].tile)
#define HOP_TILE            (ss_anims[SS_ANIM_HOP_HOP].frames[0].tile)
/* Batch-3 archetypes (cold-start = frame 0). ET_ITEM cold-starts at the fruit
 * frame chosen by its FruitType (set explicitly at spawn, see entity_spawn). */
#define LARGE_TILE          (ss_anims[SS_ANIM_LARGE_WALK].frames[0].tile)
#define EMIT_TILE           (ss_anims[SS_ANIM_EMIT_IDLE].frames[0].tile)
#define ORB_TILE            (ss_anims[SS_ANIM_ORB_FLOAT].frames[0].tile)
#define FRUIT_TILE(ft)      (ss_anims[k_fruit_anim[(ft)]].frames[0].tile)

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
        e->fruit     = 0;
        e->popupLen  = 0;
        e->fromSched = 0;                           /* scheduler tags its own    */
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
                /* PLAYER BULLET sprite. RE'd from the arcade hardware spriteram
                 * (tools/arc_bullet.lua, mame sonson): firing emits the small
                 * projectile at sprite codes 70/71 bank 2 -- the SAME graphic as the
                 * enemy lobbed shot (SS_ANIM_ESHOT_FLY). The old code left e->av at
                 * the entity_spawn default (SS_ANIM_ENEMY_WALK) and only set a static
                 * SHOT_TILE, so anim_step() below animated the bullet as the ENEMY
                 * WALK character -- the "shooting a character as a bullet" bug. */
                anim_restart(&e->av, SS_ANIM_ESHOT_FLY);
                e->tile = ESHOT_TILE;
                /* vx/facing set by entity_player_fire (direction = player facing) */
                break;

            case ET_ITEM:
                e->hp    = 0;
                e->fruit = FRUIT_CHERRY;   /* default; caller sets the type+tile  */
                anim_restart(&e->av, k_fruit_anim[FRUIT_CHERRY]);
                e->tile  = FRUIT_TILE(FRUIT_CHERRY);   /* stationary collectible  */
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
                e->timer  = thrower_fire_period();  /* first-lob cadence (per round) */
                anim_restart(&e->av, SS_ANIM_THROWER_IDLE);
                e->tile   = THROWER_TILE;
                break;

            case ET_ENEMY_SHOT:
                e->hp   = 0;
                /* ENEMY lobbed shot = classI, the green creature codes 96-109 bank9
                 * (RE'd from the $0DA8 pool capture) -- a DIFFERENT sprite from the
                 * player bullet (70/71). Uses its own SS_ANIM_ELOB. */
                anim_restart(&e->av, SS_ANIM_ELOB);
                e->tile = ELOB_TILE;
                /* vx / vy set by the thrower at launch (toward the player) */
                break;

            case ET_CREATURE_G:            /* classG ground creature: walk + throw */
                e->hp     = CREG_HP;
                e->facing = 1;             /* marches LEFT toward the player        */
                e->state  = 0;             /* 0 = walk/throw active, 1 = dying      */
                e->vx     = (s16)(-CREG_SPEED);
                e->timer  = CREG_FIRE_PERIOD;       /* first-throw cadence          */
                anim_restart(&e->av, SS_ANIM_CREG_WALK);
                e->tile   = CREG_TILE;
                break;

            case ET_CREATURE_C:            /* classC creature-family walker         */
                e->hp     = CREC_HP;
                e->facing = 1;
                e->state  = 0;             /* 0 = seek active, 1 = dying            */
                e->vx     = (s16)(-CREC_SPEED);
                anim_restart(&e->av, SS_ANIM_CREC_WALK);
                e->tile   = CREC_TILE;
                break;

            case ET_COMPOSITE_E:           /* classE tough large-composite walker   */
                e->hp     = COMPE_HP;      /* 2 HP: two shots                       */
                e->facing = 1;
                e->state  = 0;             /* 0 = walk active, 1 = dying            */
                e->vx     = (s16)(-COMPE_SPEED);
                anim_restart(&e->av, SS_ANIM_CMPE_WALK);
                e->tile   = COMPE_TILE;
                break;

            case ET_HOPPER:                /* classH leaping hopper                 */
                e->hp     = HOP_HP;
                e->facing = 1;
                e->state  = 0;             /* 0 = grounded(pause), 1 = airborne, 2 dying */
                e->vx     = 0;             /* set per-leap toward the player        */
                e->vy     = 0;
                e->timer  = HOP_LAND_PAUSE;         /* short pause then first leap   */
                e->launchY= y;                      /* its ground lane Y             */
                anim_restart(&e->av, SS_ANIM_HOP_HOP);
                e->tile   = HOP_TILE;
                break;

            case ET_LARGE:                 /* obj16x16 tough patroller ($D57F)      */
                e->hp     = LARGE_HP;      /* 3 HP (arcade $8,x=#$07)               */
                e->facing = 1;             /* starts marching LEFT                  */
                e->state  = 0;             /* 0 = patrol, 1 = dying                 */
                e->vx     = (s16)(-LARGE_SPEED);
                e->timer  = LARGE_TURN;             /* frames to next direction flip */
                anim_restart(&e->av, SS_ANIM_LARGE_WALK);
                e->tile   = LARGE_TILE;
                break;

            case ET_EMITTER:               /* classD stationary shot emitter ($BF6B)*/
                e->hp     = EMIT_HP;
                e->facing = 1;             /* faces left toward the player          */
                e->state  = 0;             /* 0 = idle/firing, 1 = dying            */
                e->vx     = 0;             /* STATIONARY                            */
                e->timer  = EMIT_FIRE_PERIOD;       /* first-shot cadence            */
                anim_restart(&e->av, SS_ANIM_EMIT_IDLE);
                e->tile   = EMIT_TILE;
                break;

            case ET_ORB:                   /* objSingle hovering hazard ($E1C4)     */
                e->hp     = ORB_HP;
                e->facing = 1;
                e->state  = 0;             /* 0 = hover, 1 = dying                  */
                e->vx     = (s16)(-ORB_SPEED);
                e->launchY= y;             /* spawn row -- bob around it (no chase) */
                anim_restart(&e->av, SS_ANIM_ORB_FLOAT);
                e->tile   = ORB_TILE;
                break;

            case ET_POPUP:                 /* floating "+N" score popup (no sprite) */
                e->hp    = 0;
                e->vy    = POPUP_RISE_VY;  /* drift upward                          */
                e->timer = POPUP_LIFE;
                e->tile  = 0;              /* drawn as HUD text, not a sprite       */
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
/* Spawn a FRUIT of a specific type (sets the type, its recoloured sprite, and
 * its anim). The SonSon signature collectible -- score on touch is per-type
 * (k_fruit_score[]). Returns the entity or 0 if the pool is full. */
static Entity *spawn_fruit(FruitType ft, s16 x, s16 y)
{
    Entity *e = entity_spawn(ET_ITEM, x, y);
    if (!e) return 0;
    e->fruit = (u8)ft;
    anim_restart(&e->av, k_fruit_anim[ft]);
    e->tile  = FRUIT_TILE(ft);
    return e;
}

/* Spawn a floating "+N" score POPUP at (x,y) showing `val` points. The arcade's
 * classK $0ED8 score-child (sub_CBC8 anim / sub_CCB1 dispatch) re-expressed as a
 * cheap drifting font-glyph label (font sprites, no body tile). The "+N" text is
 * formatted ONCE here (not per frame) into e->popupStr, so the per-frame render
 * does zero intToStr/strlen work -- just steps the cached glyphs across. */
static void spawn_popup(s16 x, s16 y, u16 val)
{
    Entity *e = entity_spawn(ET_POPUP, x, y);
    char tmp[8];
    u8   n;
    if (!e) return;
    tmp[0] = '+';
    intToStr((s32)val, tmp + 1, 1);
    n = (u8)strlen(tmp);
    if (n > 5) n = 5;                         /* popupStr holds up to 5 glyphs + NUL */
    {
        u8 k;
        for (k = 0; k < n; k++) e->popupStr[k] = tmp[k];
        e->popupStr[n] = 0;
    }
    e->popupLen = n;
}

/* ===========================================================================
 *  FAITHFUL per-level WAVE SCHEDULER -- the FULL arcade enemy variety
 * ===========================================================================
 *  This used to schedule ONLY the $0828 ground-walker wave (3 variants). The
 *  arcade actually spawns a far richer mix PER LEVEL from SEVERAL pools, each
 *  with its own per-stage placement table. Re-engineered here to reproduce that.
 *
 *  RE SOURCE (static, ../sonson/port/src + census_semantic.toml [spawn_mechanism]):
 *   - GROUND WAVE ($0828 enemyMain): wave dispatch sub_EFED($0638) -> sub_F019/
 *     F065/F080 spawn from the $F0C9 list (12 (Y,type) pairs; type -> the slot's
 *     dispatch index +3: 0=walker-A, 1=walker-B, 2=lane-shift, 4=THROWER) at
 *     worldX=scroll($90)+$F8 (right edge), capped at $00A6==$10. (spawn_sched.json)
 *   - The OTHER classes are NOT placed by sub_F080. Each has a DEDICATED per-stage
 *     spawner keyed on the stage byte $0096, firing a whole batch when its wave
 *     event triggers (it then latches `$2,x=0` done):
 *       classF flyers  ($0BA8): sub_F51C, 5 (1 if $96&0x20), list@$F57A, idx5 spawn
 *       classG creature($0C28): sub_F6CB, 6, list@$F72B, idx3
 *       classH actors  ($0CE8): sub_F83E, 6, list@$F8B2 by ($96&0x30)>>3, idx3
 *       classE large   ($0B68): sub_F47F, 2, list@$F4BC by ($96&0x38)>>1
 *       classD emitter ($0AE8): sub_F3A1, 4, list@$F403; also event sub_ED30
 *       classI lobs    ($0DA8): sub_D352 children fired by throwers/creatures
 *       objSingle      ($0E28): sub_E1C4 parent-linked unique
 *
 *  LIVE CAPTURE (tools/arch_census.txt md=02, NOT pinning $0067 so $0096 advances;
 *  tools/arch_stage_{10,20,30}.txt forcing deep stages) -- GROUND TRUTH of the mix:
 *   - EVERY level opens with the ground wave ($0828, up to 10), idx 0/1/2.
 *   - Then DOMINANT classH creature waves ($0CE8, up to 6): idx 4 tracking-walker
 *     + idx 5 leaping-HOPPER + idx 2 falling -- the leaping/special creatures.
 *   - classI lobbed projectiles ($0DA8, up to 4) fly alongside (the creatures throw).
 *   - classF FLYERS ($0BA8, up to 4) appear from LEVEL 2 (gfx 01) onward and
 *     DOMINATE deeper stages (stage 0x20/0x30: 4 flyers, idx 3 = homing dive).
 *   - objSingle (1) drifts in occasionally (the unique hazard).
 *   - classC/classD/classE/classG/obj16x16 = NEVER populated by a normal forward
 *     run (special-context: level-end / boss / scripted events) -> rare/TODO here.
 *
 *  NATIVE RE-EXPRESSION (this file):
 *   The native engine runs 8 levels (round R -> ss_levels[(R-1)%8]); the spawn
 *   "stage" escalates with the round exactly like the arcade $0096 increments. We
 *   re-express each level as a PROGRAM of wave SEGMENTS (k_wave_program[]): an
 *   ordered list of (archetype, count, spacing) that the scroll cursor walks --
 *   the arcade's opener -> creature-wave -> flyer-wave progression. Arcade class ->
 *   native ET_*:
 *       $0828 walker A/B/lane / THROWER  -> ET_ENEMY_A / ET_CREATURE_C /
 *                                           ET_COMPOSITE_E / ET_THROWER
 *       classH idx5 hopper / idx4 walker -> ET_HOPPER / ET_CREATURE_G (walk+throw,
 *                                           = the classH+classI throw combo)
 *       classI lob                       -> ET_ENEMY_SHOT (fired by THROWER/CREG)
 *       classF flyer                     -> ET_FLYER (level >=2, denser deeper)
 *       objSingle                        -> ET_ORB (rare drift-in)
 *       classE large / classD emitter    -> ET_LARGE / ET_EMITTER (rare, deep
 *                                           levels only -- special-context; TODO).
 *   Each segment spawns at the RIGHT edge (worldX ~ cameraX + SCREEN_W, the arcade
 *   scroll($90)+$F8) as the camera advances `spacing` px, and ONLY while the live
 *   scheduled count is under SPAWN_ONSCREEN_CAP (the arcade $00A6==$10 gate). No
 *   recycling -- a scheduled enemy that exits left despawns for good; population
 *   is replenished only from the program. The program LOOPS (its tail segments
 *   repeat) so a long level keeps a faithful, bounded mix coming.
 * ========================================================================= */

/* The 6 arcade spawn rows (Y_byte from the $F0C9 lists), used directly as the
 * native world-Y (both the arcade and the native screen are 224px tall; these
 * sit inside the lane band). Index 0 = top row .. 5 = bottom row. */
static const s16 k_spawn_row_y[6] = { 40, 72, 104, 136, 168, 200 };

/* The captured 12-entry placement lists, one representative list per stage-group
 * (phase-slot 0 of each group from tools/spawn_sched.json -- the real arcade
 * order). Each entry = a row index 0..5 (the Y_byte decoded to a row) paired with
 * the group's enemy type. We store just the ROW here; the TYPE comes from the
 * group (groups 0/1/3 are single-type; group 2 is the 00/01 mix, encoded per
 * entry in k_wave_mix). The row sequences are transcribed from the $F0C9 lists:
 *   group0 list@F0E9: C8 68 A8 88 A8 48 C8 48 C8 28 28 88  -> rows below
 *   group1 list@F149: 28 48 68 48 28 68 88 C8 28 C8 A8 48
 *   group2 list@F1A9: A8 88 28 88 28 A8 (mix types) ...
 *   group3 list@F209: C8 A8 88 48 28 68 88 C8 28 C8 A8 48   */
#define WAVE_LEN  12
static const u8 k_wave_row[4][WAVE_LEN] =
{
    /* group0 (Y: C8 68 A8 88 A8 48 C8 48 C8 28 28 88) */
    { 5,2,4,3,4,1,5,1,5,0,0,3 },
    /* group1 (Y: 28 48 68 48 28 68 88 C8 28 C8 A8 48) */
    { 0,1,2,1,0,2,3,5,0,5,4,1 },
    /* group2 (Y: A8 A8 88 48 28 68 88 C8 28 C8 A8 48) -- MIX type per k_wave_mix */
    { 4,4,3,1,0,2,3,5,0,5,4,1 },
    /* group3 (Y: C8 A8 88 48 28 68 88 C8 28 C8 A8 48) */
    { 5,4,3,1,0,2,3,5,0,5,4,1 },
};
/* (k_wave_mix / sched_type_to_et removed: enemyMain now spawns ET_THROWER directly,
 *  so the old type-byte->variant mapping is no longer used. k_wave_row still supplies
 *  enemyMain's faithful $F0C9 row placement.) */

/* stage -> ground-wave group, faithful to the arcade's ((stage&0x30)>>1) deriv.
 * The stage byte $005A (mirrored to the wave selector $0096) selects the group:
 * stages 0x00-0x0F -> g0, 0x10-0x1F -> g1, 0x20-0x2F -> g2, 0x30+ -> g3. Because
 * $005A SATURATES at 0x1C (28; STAGE_MAX), only g0 (0x00-0x0F) and g1 (0x10-0x1C)
 * are EVER reachable in normal play -- exactly the arcade (its $0096 also caps at
 * 0x1C, so the tougher g2/g3 ground variants never appear from forward scroll; the
 * captures confirm the tough composite is special-context only). Reads g_game.stage
 * directly now (it IS the arcade $005A), not the unbounded round. */
static u8 sched_group_for_round(void)
{
    u16 stage = g_game.stage;              /* arcade $0096 = $005A (saturating)     */
    u8  g = (u8)(((stage & 0x30) >> 1) / 8);
    if (g > 3) g = 3;
    return g;
}

/* ===========================================================================
 *  WAVE SCHEDULE -- driven by the arcade $FCD9 table (see ss_wave_sched.h).
 * ===========================================================================
 *  The hand-authored per-level wave PROGRAM (k_wp_l1/l2/deep + WaveStep) is gone;
 *  enemy presence now comes straight from the arcade's RE'd 87-wave schedule. The
 *  helpers kept ABOVE (k_spawn_row_y, k_wave_row, k_wave_mix, sched_type_to_et,
 *  sched_group_for_round) still supply the byte-faithful $F0C9 ground placement
 *  for the enemyMain class; everything else is decoded from the wave mask +
 *  placement bytes by the driver BELOW (wave_id_for_camera / wave_spawn_bit /
 *  entity_schedule_tick). ======================================================= */

/* ---- Scheduler state (file-static; reset by entity_schedule_init) ---------- */
static u8  s_wave_id;           /* last loaded arcade wave-id (0xFF = none yet)    */
static u8  s_sched_gcursor;     /* enemyMain $F0C9 ground-list cursor (wraps)      */

/* Count the live SCHEDULED wave enemies (the arcade $00A6 live-count gate). Body
 * enemies + the orb count; the projectiles they lob are NOT scheduled (they're
 * children, like the arcade $0ED8/$0DA8 pool) so they don't gate the wave. */
static u16 sched_live_count(void)
{
    u16 i, n = 0;
    for (i = 0; i < ENTITY_MAX; i++)
        if (g_entities[i].active && g_entities[i].fromSched) n++;
    return n;
}

/* (Re)arm the wave scheduler. The arcade reloads a wave only when the scroll
 * counter $90 ticks ($EF98: CMPB ,X / BEQ $EFBA); we mirror that by caching the
 * last loaded wave-id and invalidating it here so the next tick loads a wave, and
 * reset the enemyMain $F0C9 ground cursor. */
void entity_schedule_init(void)
{
    s_wave_id       = 0xFF;     /* != any real wave-id -> first tick loads a wave */
    s_sched_gcursor = 0;
}

/* Map the (wrapped) camera position to an arcade wave-id. The arcade wave-id IS
 * the scroll position: $EF98 does `LDB <$90; ANDB #$7F`, and $90 is the high byte
 * of the 16-bit world-scroll ($90:$91; spawn worldX = $90:$91 + $F8) -- i.e. a new
 * wave every 256px of arcade scroll, indexing the 87-entry table $FCD9. The native
 * ribbon LOOPS at g_ribbonW px (a shorter pixel run than the arcade's for the same
 * scenery), so we re-express that cadence by mapping the ribbon's full length onto
 * all 87 waves: one ribbon loop = one full pass of the arcade wave sequence. Using
 * the WRAPPED cameraX, the wave-id wraps 86->0 exactly as cameraX wraps the ribbon
 * width ->0 (game.c) -- no seam, no separate accumulator to shift on the loop.
 * (g_levelW is the ribbon pixel width = the internal g_ribbonW; game.h.) */
static u8 wave_id_for_camera(void)
{
    s16 cx = g_game.cameraX;
    u32 w;
    if (g_levelW == 0) return 0;
    if (cx < 0) cx = 0;
    w = ((u32)(u16)cx * SS_WAVE_COUNT) / (u32)g_levelW;
    if (w >= SS_WAVE_COUNT) w = SS_WAVE_COUNT - 1;
    return (u8)w;
}

/* Spawn one entity for a set mask-bit. `place` is the arcade placement byte (the
 * consumer $EFBB writes it to the class record's param $E). What param $E MEANS is
 * per-class (decoded from the spawners in ss.03e):
 *   - classG ($F682) and classH ($F7F3) read ONLY its HIGH BIT: clear -> enter at
 *     the RIGHT edge (worldX = $90+$F8); set -> enter at the LEFT edge (worldX =
 *     $90-$08, $F6F5 / $F87E) -- i.e. from BEHIND. Our native classG/classH are
 *     player-X trackers (entity.c CREATURE_G $1368 / HOPPER $1497 home on the
 *     player), so a left-edge spawn just walks back in toward the player; no extra
 *     direction/despawn handling is needed.
 *   - enemyMain ($EFED), classC/D/E/F/I do NOT read param $E at all -- their spawn
 *     Y comes from per-class formation tables (e.g. classG's $F72B), not the
 *     placement byte. The native single-archetype model doesn't reproduce those
 *     6-wide formations 1:1, so for those classes we just spread entries across the
 *     6 tier bands (k_spawn_row_y) deterministically by the placement low bits --
 *     varied lanes, not byte-exact arcade Y. enemyMain keeps its byte-faithful
 *     $F0C9 ground placement + walker variant ($F01F: stage&0x30 -> $F0C9 list).
 * bit->class per the RE'd table (header SS_WAVE_BIT_*). Tagged fromSched so the
 * cap/recycle logic treats it as a wave body. */
static void wave_spawn_bit(u8 bit, u8 place)
{
    s16 sy;
    u8  type;
    u8  fromLeft = 0;       /* arcade param-$E high bit: enter from behind (classG/H) */
    Entity *e;

    if (bit == SS_WAVE_BIT_ENEMYMAIN)
    {
        /* enemyMain ($0828) = the white ghost/skull-faced enemy, codes 136-143 b10
         * (RE'd: isolated bit0-only-wave pool capture). That IS the native THROWER
         * sprite (marches + lobs), so enemyMain spawns ET_THROWER. Row placement
         * stays the byte-faithful $F0C9 ground list ($F01F: stage&0x30 -> $F0C9). */
        u8 grp = sched_group_for_round();
        u8 row = k_wave_row[grp][s_sched_gcursor];
        s_sched_gcursor = (u8)((s_sched_gcursor + 1) % WAVE_LEN);
        sy   = k_spawn_row_y[row];
        type = ET_THROWER;
    }
    else
    {
        u8 row = (u8)((place & 0x7F) % 6);      /* deterministic tier-band spread     */
        sy = k_spawn_row_y[row];
        switch (bit)
        {
            case SS_WAVE_BIT_CLASSC: type = ET_CREATURE_C;  break; /* classC walker  */
            case SS_WAVE_BIT_CLASSD: type = ET_EMITTER;     break; /* classD emitter */
            case SS_WAVE_BIT_CLASSE: type = ET_COMPOSITE_E; break; /* classE tough   */
            case SS_WAVE_BIT_CLASSF: type = ET_FLYER;
                                     sy = k_spawn_row_y[0];  break; /* classF: high lane */
            /* classG/classH honor param $E's high bit: enter from the left (behind) */
            /* classH is fundamentally a TRACKING WALKER (its arcade states $CF4E
             * "main run" + $D02D "tracking walker" dominate; the leaping $D0A2 is
             * secondary), and it renders the walker sprite 304-306 ($D1BD). So it
             * spawns ET_ENEMY_A (tracking walker, same 304-306 walk anim) -- NOT the
             * native's always-leaping HOPPER, which over-emphasized the rare hop. */
            case SS_WAVE_BIT_CLASSH: type = ET_ENEMY_A;    fromLeft = (place & 0x80); break;
            case SS_WAVE_BIT_CLASSG: type = ET_CREATURE_G; fromLeft = (place & 0x80); break;
            case SS_WAVE_BIT_CLASSI: type = ET_THROWER;     break; /* classI lobbed-shot wave -> native lobs via a thrower (a bare projectile has no parent arc) */
            case SS_WAVE_BIT_OBJ16:  type = ET_LARGE;       break; /* obj16x16        */
            default: return;   /* bits 8/10 (rec06B8 gated $9D / rec06D8): special-context, not armed */
        }
    }

    /* arcade right-edge worldX = $90+$F8 (~cameraX+SCREEN_W); left-edge = $90-$08 */
    {
        s16 sx = fromLeft ? (s16)(g_game.cameraX - 24)
                          : (s16)(g_game.cameraX + SCREEN_W + 8);
        e = entity_spawn(type, sx, sy);
    }
    if (e) e->fromSched = 1;
}

/* Per-frame: derive the wave-id from the camera; when it ADVANCES, load that wave
 * from the arcade $FCD9 schedule and spawn its classes. Faithful to the arcade
 * producer/consumer ($EF89/$EFBB): read ss_wave_mask[wid], walk the bits LSB-first
 * (the arcade LSR/ROR order), consuming exactly one placement byte per SET bit.
 * Honours the on-screen cap ($00A6==$10): a capped bit still consumes its placement
 * byte (keeping the stream aligned) but skips the spawn -- the arcade likewise
 * advances the wave phase rather than stalling. Lobbed child projectiles are not
 * fromSched, so they don't gate the wave. Called from entity_update_all. */
void entity_schedule_tick(void)
{
    u8  wid = wave_id_for_camera();
    u16 mask;
    const u8 *place;
    u8 pidx, bit;

    if (wid == s_wave_id) return;       /* same wave: nothing new (arcade BEQ $EFBA) */
    s_wave_id = wid;

    mask  = ss_wave_mask[wid];
    place = &ss_wave_place[ss_wave_place_off[wid]];
    pidx  = 0;
    for (bit = 0; bit < 12; bit++)      /* arcade scans 12 control records ($8=$0C) */
    {
        if (mask & (u16)(1u << bit))
        {
            u8 pb = place[pidx++];      /* one placement byte per set bit (LSB-first) */
            if (sched_live_count() < SPAWN_ONSCREEN_CAP)
                wave_spawn_bit(bit, pb);
        }
    }
}

/* SEAMLESS-RIBBON coordinate wrap (see entity.h). Shift every live entity's world
 * X by dx. Called from game.c the frame it wraps cameraX by the ribbon width;
 * because cameraX gets the same dx, screen positions (x - cameraX) are unchanged
 * -> the wrap is invisible. (The wave scheduler needs no shift: it derives its
 * wave-id from the wrapped cameraX, which wraps in lockstep -- see wave_id_for_camera.) */
void entity_shift_worldx(s16 dx)
{
    u16 i;
    for (i = 0; i < ENTITY_MAX; i++)
        if (g_entities[i].active)
            g_entities[i].x = (s16)(g_entities[i].x + dx);
}

/* ------------------------------------------------------------------------- */
/* Seed the AMBIENT, non-wave entities (the SonSon FRUIT variety) at level start.
 * The marching enemy WAVES come from the faithful scheduler (entity_schedule_*);
 * fruit are stationary collectibles placed across the level. Kept few + spread so
 * the scanline load stays low; the scheduler owns the dynamic enemy population. */
static void spawn_ambient(void)
{
    #define LANE_Y(l)   ((s16)(LANE_BASE_Y - (s16)(l) * LANE_PITCH))

    /* ---- FRUIT VARIETY (the SonSon signature collectible). Four types escalate
     * in value (cherry 300 / orange 500 / apple 1000 / gold 2000); the gold is
     * rarer + on a high lane (a reward for climbing). Eating one scores by type +
     * pops a floating "+N". Spread across the level so they're never stacked. */
    spawn_fruit(FRUIT_CHERRY, 420,  LANE_Y(1));
    spawn_fruit(FRUIT_ORANGE, 820,  LANE_Y(0));
    spawn_fruit(FRUIT_APPLE,  1080, LANE_Y(2));
    spawn_fruit(FRUIT_GOLD,   960,  LANE_Y(5));   /* high-lane reward */

    #undef LANE_Y
}

void entity_init(void)
{
    u16 i;
    for (i = 0; i < ENTITY_MAX; i++) g_entities[i].active = 0;
    /* per-player hurt/invuln/deadReq are reset by game_init (in Player). */
    spawn_ambient();            /* stationary fruit collectibles                */
    entity_schedule_init();     /* arm the faithful per-level wave scheduler     */
}

/* ------------------------------------------------------------------------- */
/* Player FIRE -- arcade sub_B25A re-expressed. The arcade player has a SINGLE shot
 * slot ($0E28; sub_B25A @ $B26C does a literal 'ldu #$0E28' -- a fixed address, NOT
 * a pool scan), so exactly ONE player shot exists at a time and the next can't fire
 * until it despawns (off-screen / on hit). We match that here: refuse to fire while
 * a player shot is still live. Faithful AND it removes the fire-spam entity pile-up
 * (many coexisting shots was the ~30fps cliff). game.c handles the press-edge
 * debounce; this returns TRUE iff a shot was actually fired. */
bool entity_player_fire(u8 owner, s16 px, s16 py, u8 facing)
{
    u8 i;
    /* PER-PLAYER single-slot gate: the arcade gives each player its OWN shot pool
     * (P1 $04E0, P2 $0520), so each may have one shot live independently. Refuse
     * only while THIS owner's shot is still in flight. */
    for (i = 0; i < ENTITY_MAX; i++)
        if (g_entities[i].active && g_entities[i].type == ET_SHOT &&
            g_entities[i].owner == owner)
            return FALSE;
    {
        Entity *s = entity_spawn(ET_SHOT, px, py);
        if (!s) return FALSE;
        s->owner  = owner;                         /* credit this player on a kill */
        s->facing = facing;
        s->vx     = facing ? (s16)(-SHOT_SPEED) : (s16)SHOT_SPEED;
        s->vy     = 0;
    }
    return TRUE;
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

/* ---- TIER SEATING -- the arcade ground-enemy vertical model (sub_A847) ------
 * RE VERDICT (cited): the arcade GROUND enemies do NOT chase the player's tier.
 * Their VERTICAL position is governed by SCRIPTED motion + terrain SEATING via
 * sub_A847 ($A847) -- NOT a player-Y homing term. The decode:
 *   - enemyMain/$0828 walkers + thrower share the mover sub_BB46 ($BB46): it adds
 *     a per-state velocity VECTOR [dx@$1,dy@$3] read from a 5-byte-stride table
 *     indexed by the state/direction byte -$6,x. The DIRECTION (-$6,x) is chosen
 *     ONLY from the player's X -- sub_DC7D ($DC7D: cmpd $4,u player-X) and the
 *     classH tracker sub_D083 ($D091: cmpd -$B,u player-X) pick left/right. The
 *     dy component is a FIXED script value per state, never derived from player Y.
 *   - classG sub_C744 ($C744) does `dec -$8,x` (climb up 1px) then sub_C734
 *     ($C734) calls sub_A847: carry-clear (no tier) => set a fall velocity $0400;
 *     carry-set (solid) => SEAT (b + -$8,x, b = A847's push-out). classH sub_CFA8
 *     / sub_D06B / leap sub_D0B5 ($D0D9) do the same gravity + A847 land-seat.
 *   - The ONLY player-Y reference in any ground handler is the CONTACT-overlap
 *     TEST sub_DCAC ($DCC?-: -$8,u - -$8,x, +-8px window) -- a hit test, never a
 *     move. A grep of the whole port src for a player-Y read driving a velocity
 *     finds NONE. (census_semantic.toml labels D02D "tracking walker" and BA89
 *     "lane-shift", but the code tracks the player on X; the vertical is scripted
 *     bob + terrain seat.)
 * So a faithful native ground marcher STAYS on the tier it entered on (the arcade
 * tier bands are universally solid -> A847 keeps a flat marcher seated on its
 * row). seat_on_tier() snaps Y to the nearest of the 6 SS_TIER_Y bands -- the
 * native equivalent of the A847 reseat -- so a marcher rides its spawn tier
 * exactly (and never drifts between tiers). It does NOT move toward the player. */
static inline void seat_on_tier(Entity *e)
{
    /* nearest SS_TIER_Y band to the entity's current Y (the A847 push-out). The 6
     * tiers are {200,168,136,104,72,40}; pick the closest and snap to it. */
    u8  best = 0;
    s16 bestD = 0x7FFF;
    u8  t;
    for (t = 0; t < SS_COLLISION_TIERS; t++)
    {
        s16 d = (s16)(e->y - SS_TIER_Y[t]);
        if (d < 0) d = (s16)(-d);
        if (d < bestD) { bestD = d; best = t; }
    }
    e->y    = SS_TIER_Y[best];
    e->ySub = 0;
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
static inline s16 flyer_bob_vy_amp(u8 phase, u16 amp)
{
    /* phase advances every 4 frames (see update); >>2 gives a ~64-frame period. */
    s32 s = k_sin16[(phase >> 2) & 0x0F];
    return (s16)((s * (s32)amp) >> 6);
}
static inline s16 flyer_bob_vy(u8 phase)
{
    return flyer_bob_vy_amp(phase, FLYER_BOB_AMP);
}

/* ---- THROWER lob: spawn a lobbed projectile arcing toward the player --------
 * Re-expresses the classI launch (sub_D352): the projectile starts at the
 * thrower's mid-body with an UP initial velocity (ESHOT_VY0) and a horizontal
 * velocity toward the player; gravity (applied each frame in the ESHOT update)
 * pulls it down into a parabola. launchY records the lob height so the shot
 * despawns when it falls back to that level -- the native land/impact (arcade
 * sub_D446), plus a hard max-life guard. */
/* The arcade lobbed-projectile pool is classI $0DA8 = exactly 4 slots
 * ($0DA8 + 4*$20 = $0E28, the player-shot slot), so at most 4 enemy lobs exist at
 * once. Cap live ET_ENEMY_SHOT at 4 to match (faithful + bounds the projectile load,
 * the same theme as the player's single-shot $0E28 cap). */
#define ESHOT_MAX  4
static u8 enemy_shot_count(void)
{
    u8 i, n = 0;
    for (i = 0; i < ENTITY_MAX; i++)
        if (g_entities[i].active && g_entities[i].type == ET_ENEMY_SHOT) n++;
    return n;
}

static void thrower_lob(s16 sx, s16 sy, const Player *p)
{
    if (enemy_shot_count() >= ESHOT_MAX) return;        /* arcade $0DA8 4-slot pool */
    Entity *s = entity_spawn(ET_ENEMY_SHOT, sx, sy);
    if (!s) return;
    s->vy      = ESHOT_VY0;                              /* up -- the arc start    */
    s->facing  = (p->x < sx) ? 1 : 0;                    /* aim at the player      */
    s->vx      = s->facing ? (s16)(-ESHOT_VX) : (s16)ESHOT_VX;
    s->timer   = 180;                                    /* ~3s hard max-life      */
    s->launchY = sy;                                     /* land when it falls back */
}

/* ---- EMITTER fire: spawn a FLAT horizontal shot toward the player -----------
 * Re-expresses the classD launch ($BF75 spawning a projectile child). Unlike the
 * thrower's parabolic lob, the emitter's shot starts with NO upward velocity and
 * a fast horizontal velocity (gravity then only gently bows it), so it reads as a
 * straight emplacement shot. launchY is parked well BELOW so it crosses the screen
 * rather than "landing" immediately; the off-screen / max-life guard despawns it. */
static void emitter_fire(s16 sx, s16 sy, const Player *p)
{
    if (enemy_shot_count() >= ESHOT_MAX) return;        /* arcade $0DA8 4-slot pool */
    Entity *s = entity_spawn(ET_ENEMY_SHOT, sx, sy);
    if (!s) return;
    s->vy      = 0;                                      /* flat -- no arc start   */
    s->facing  = (p->x < sx) ? 1 : 0;                    /* aim at the player      */
    s->vx      = s->facing ? (s16)(-EMIT_SHOT_VX) : (s16)EMIT_SHOT_VX;
    s->timer   = 150;                                    /* ~2.5s hard max-life    */
    s->launchY = (s16)(SCREEN_H + 64);                   /* far below: never lands */
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
                if (anim_finished(&e->av)) e->active = 0;
                break;
            }

            /* face + move toward the player's world X (the seek) */
            if (p->x < e->x) { e->facing = 1; if (e->vx > 0) e->vx = -e->vx; }
            else             { e->facing = 0; if (e->vx < 0) e->vx = -e->vx; }
            integrate(&e->x, &e->xSub, e->vx);

            /* VERTICAL: ride the spawn tier (arcade sub_A847 reseat). The arcade
             * enemyMain walker (mover sub_BB46) tracks the player on X only -- its
             * Y is the scripted-vector dy + terrain seating, NOT a player-Y chase.
             * On the universally-solid tier bands a flat marcher stays on its row,
             * so we just keep it seated on its entry tier. (See seat_on_tier RE.) */
            seat_on_tier(e);

            /* faithful walk cycle (captured arcade cadence) */
            anim_set(&e->av, SS_ANIM_ENEMY_WALK);
            e->tile = anim_tick(&e->av);

            /* off-screen-left (arcade DD52). SCHEDULED wave enemies DESPAWN for
             * good (the scheduler replenishes from the per-level list -- no
             * right-edge recycle, so they never pile up beyond the arcade count).
             * A non-scheduled straggler recycles as before. */
            if (e->x < g_game.cameraX - 32)
            {
                if (e->fromSched) { e->active = 0; break; }
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
                if (anim_finished(&e->av)) e->active = 0;
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

            /* off the left edge: scheduled flyers despawn (scheduler-replenished);
             * a non-scheduled one re-seeds high on the right, gliding again. */
            if (e->x < g_game.cameraX - 32)
            {
                if (e->fromSched) { e->active = 0; break; }
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
                if (anim_finished(&e->av)) e->active = 0;
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
                /* VERTICAL: ride the spawn tier. The arcade thrower (enemyMain
                 * idx4, mover sub_BB46) marches toward the player on X and clamps
                 * the approach by X distance (sub_BAC3/BAE5 cmpd) -- no player-Y
                 * pursuit. It threatens with its lobbed arc, not by changing tier. */
                seat_on_tier(e);
                anim_set(&e->av, SS_ANIM_THROWER_IDLE);
            }
            else                               /* FIRE-STANCE: in range -> lob    */
            {
                e->state = 1;
                if (e->fireTimer == 0 && --e->timer == 0)
                {
                    /* LOB from the thrower's mid-body toward the player */
                    thrower_lob((s16)(e->x + 4), (s16)(e->y + 4), p);
                    e->timer     = thrower_fire_period(); /* next-lob cadence (round) */
                    e->fireTimer = THROWER_THROW_ANIM;    /* show the throw pose   */
                    anim_restart(&e->av, SS_ANIM_THROWER_THROW);
                }
                if (e->fireTimer == 0)
                    anim_set(&e->av, SS_ANIM_THROWER_IDLE);
            }

            if (e->fireTimer) e->fireTimer--;
            e->tile = anim_tick(&e->av);

            /* off the left edge: scheduled despawn / non-scheduled recycle */
            if (e->x < g_game.cameraX - 32)
            {
                if (e->fromSched) { e->active = 0; break; }
                e->x     = (s16)(g_game.cameraX + SCREEN_W + 16);
                e->xSub  = 0;
                e->state = 0;
                e->timer = thrower_fire_period();
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
            anim_set(&e->av, SS_ANIM_ELOB);                /* classI green-creature lob */
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

        /* ----- CREATURE_G (classG $C744 walk / $C7F2 throw): march + throw -----
         * RE: the creature walks/patrols ($C744) toward the player; its attack
         * phase ($C7F2 -> $C80D) spawns a projectile child into the $0ED8 pool.
         * We re-express: march toward the player (lane-homing like enemyA); once
         * the gap closes inside CREG_RANGE, LOB an ENEMY_SHOT every CREG_FIRE_
         * PERIOD frames (a brief throw-lunge anim each lob) -- and keep marching
         * the whole time (a creature, unlike the stand-and-fire THROWER). Death
         * (state 1) is the shared burst then despawn ($C9A0). */
        case ET_CREATURE_G:
        {
            s16 gap;
            if (e->state == 1)                 /* dying */
            {
                anim_set(&e->av, SS_ANIM_CREG_DEATH);
                e->tile = anim_tick(&e->av);
                if (anim_finished(&e->av)) e->active = 0;
                break;
            }

            /* face + march toward the player's world X (the seek) */
            if (p->x < e->x) { e->facing = 1; if (e->vx > 0) e->vx = -e->vx; }
            else             { e->facing = 0; if (e->vx < 0) e->vx = -e->vx; }
            integrate(&e->x, &e->xSub, e->vx);
            /* VERTICAL: ride the spawn tier. classG walk/patrol (sub_C744) climbs
             * + seats via sub_A847 (sub_C734) -- gravity/terrain, not a player-Y
             * chase; it tracks the player on X (sub_C726 edge-turn). Keep it on its
             * row (the native flat tiers = the arcade's seated march). */
            seat_on_tier(e);

            /* throw a lobbed projectile when within range, on a cadence */
            gap = (s16)((p->x < e->x) ? (e->x - p->x) : (p->x - e->x));
            if (gap <= CREG_RANGE && e->fireTimer == 0 && --e->timer == 0)
            {
                thrower_lob((s16)(e->x + 4), (s16)(e->y + 4), p);
                e->timer     = CREG_FIRE_PERIOD;
                e->fireTimer = CREG_THROW_ANIM;
                anim_restart(&e->av, SS_ANIM_CREG_THROW);
            }
            if (e->fireTimer)
                e->fireTimer--;
            else
                anim_set(&e->av, SS_ANIM_CREG_WALK);
            e->tile = anim_tick(&e->av);

            if (e->x < g_game.cameraX - 32)        /* off-screen-left              */
            {
                if (e->fromSched) { e->active = 0; break; }
                e->x = (s16)(g_game.cameraX + SCREEN_W + 16);
                e->xSub = 0;
                e->timer = CREG_FIRE_PERIOD;
            }
            break;
        }

        /* ----- CREATURE_C (classC $BC6C family / $BC74 walk): seek + wander -----
         * RE: a creature-family walker ($BC74). We re-express a ground seeker like
         * enemyA but with a gentle vertical WANDER (a small sine bob on top of the
         * lane-homing) so it reads as a distinct, slightly erratic creature.
         * Death (state 1) = shared burst then despawn. */
        case ET_CREATURE_C:
        {
            if (e->state == 1)                 /* dying */
            {
                anim_set(&e->av, SS_ANIM_CREC_DEATH);
                e->tile = anim_tick(&e->av);
                if (anim_finished(&e->av)) e->active = 0;
                break;
            }

            if (p->x < e->x) { e->facing = 1; if (e->vx > 0) e->vx = -e->vx; }
            else             { e->facing = 0; if (e->vx < 0) e->vx = -e->vx; }
            integrate(&e->x, &e->xSub, e->vx);

            /* VERTICAL: ride the spawn tier. classC (walker sub_BC74) is a plain
             * ground walker tracking the player on X -- no player-Y pursuit. Keep
             * it seated on its entry tier (was a player-Y home + sine wander; both
             * removed -- the arcade walker neither chases the player's tier nor
             * wanders vertically off its row). */
            seat_on_tier(e);

            anim_set(&e->av, SS_ANIM_CREC_WALK);
            e->tile = anim_tick(&e->av);

            if (e->x < g_game.cameraX - 32)        /* off-screen-left              */
            {
                if (e->fromSched) { e->active = 0; break; }
                e->x = (s16)(g_game.cameraX + SCREEN_W + 16);
                e->xSub = 0;
            }
            break;
        }

        /* ----- COMPOSITE_E (classE $C35E walker): tough lane walker (2 HP) ------
         * RE: $C35E reads the lane byte, parity-gates, and loads a 2.0px walk
         * velocity -- a lane walker that periodically shifts. As the "large/
         * composite" entity it is the TOUGHER archetype (2 HP). We re-express a
         * brisk lane walker; the second hit is handled in the collision pass
         * (hp>1 decrements). Death (state 1) = shared burst then despawn. */
        case ET_COMPOSITE_E:
        {
            if (e->state == 1)                 /* dying */
            {
                anim_set(&e->av, SS_ANIM_CMPE_DEATH);
                e->tile = anim_tick(&e->av);
                if (anim_finished(&e->av)) e->active = 0;
                break;
            }

            if (p->x < e->x) { e->facing = 1; if (e->vx > 0) e->vx = -e->vx; }
            else             { e->facing = 0; if (e->vx < 0) e->vx = -e->vx; }
            integrate(&e->x, &e->xSub, e->vx);
            /* VERTICAL: ride the spawn tier. classE walker (sub_C35E) loads a
             * horizontal lane velocity on a parity gate -- no player-Y term. Keep
             * it seated on its entry tier (the arcade large/composite walker marches
             * its row; the multi-sprite body is a separate TODO). */
            seat_on_tier(e);

            anim_set(&e->av, SS_ANIM_CMPE_WALK);
            e->tile = anim_tick(&e->av);

            if (e->x < g_game.cameraX - 32)        /* off-screen-left              */
            {
                if (e->fromSched) { e->active = 0; break; }
                e->x = (s16)(g_game.cameraX + SCREEN_W + 16);
                e->xSub = 0;
            }
            break;
        }

        /* ----- HOPPER (classH $D0B5 leaping hopper): ballistic leaps -----------
         * RE: the hop leaf $D0B5 adds a per-frame velocity delta to the Y accum
         * (gravity), checks a land bound ($A847 -> recover), and refreshes a 4.0px
         * horizontal velocity. We re-express two phases:
         *   state 0 GROUNDED: hold on the lane for HOP_LAND_PAUSE frames, then
         *      LAUNCH -- spring UP (HOP_VY0) and forward toward the player.
         *   state 1 AIRBORNE: gravity pulls vy down (the arc); on falling back to
         *      the launch lane Y, LAND -> back to grounded (refresh launchY to the
         *      current lane in case the camera/player drifted).
         *   state 2 DYING: shared burst then despawn. */
        case ET_HOPPER:
        {
            if (e->state == 2)                 /* dying */
            {
                anim_set(&e->av, SS_ANIM_HOP_DEATH);
                e->tile = anim_tick(&e->av);
                if (anim_finished(&e->av)) e->active = 0;
                break;
            }

            e->facing = (p->x < e->x) ? 1 : 0;

            if (e->state == 0)                 /* GROUNDED: pause then leap        */
            {
                e->anim = 0;                   /* hold the grounded cell           */
                anim_set(&e->av, SS_ANIM_HOP_HOP);
                e->av.frame = 0; e->av.timer = ss_anims[SS_ANIM_HOP_HOP].frames[0].dur;
                e->tile = anim_tile(&e->av);
                if (e->timer == 0 || --e->timer == 0)
                {
                    /* LAUNCH: spring up + forward toward the player */
                    e->state  = 1;
                    e->vy     = HOP_VY0;
                    e->vx     = e->facing ? (s16)(-HOP_VX) : (s16)HOP_VX;
                    e->launchY = e->y;         /* land back at this lane           */
                }
            }
            else                               /* AIRBORNE: gravity arc            */
            {
                anim_set(&e->av, SS_ANIM_HOP_HOP);
                e->tile = anim_tick(&e->av);   /* flap while airborne              */
                e->vy += HOP_GRAVITY;
                integrate(&e->x, &e->xSub, e->vx);
                integrate(&e->y, &e->ySub, e->vy);
                /* land: descending (vy>0) and fallen back to/below the launch lane */
                if (e->vy > 0 && e->y >= e->launchY)
                {
                    e->y     = e->launchY;     /* snap to the lane                 */
                    e->ySub  = 0;
                    e->vy    = 0;
                    e->state = 0;
                    e->timer = HOP_LAND_PAUSE;
                }
            }

            if (e->x < g_game.cameraX - 32)        /* off-screen-left              */
            {
                if (e->fromSched) { e->active = 0; break; }
                e->x = (s16)(g_game.cameraX + SCREEN_W + 16);
                e->xSub = 0;
                e->state = 0;
                e->vy = 0;
                e->timer = HOP_LAND_PAUSE;
            }
            break;
        }

        /* ----- LARGE (obj16x16 $D57F/$D58B/$D5F3): tough patroller -------------
         * RE: $D58B inits HP $8,x=#$07 + a 16px box; the move handler $D5F3 counts
         * a timer $D,x down and on expiry reloads a velocity vector from the $DB4B
         * table -- a periodic-velocity march. We re-express: a brisk lane patroller
         * that FLIPS its march direction every LARGE_TURN frames (the periodic
         * velocity reload), lane-homing onto the player like the heavy walkers.
         * Takes 3 hits (collision pass decrements hp). Death (state 1) = burst. */
        case ET_LARGE:
        {
            if (e->state == 1)                 /* dying */
            {
                anim_set(&e->av, SS_ANIM_LARGE_DEATH);
                e->tile = anim_tick(&e->av);
                if (anim_finished(&e->av)) e->active = 0;
                break;
            }

            /* periodic direction flip (the $D5F3 velocity-table reload) */
            if (--e->timer == 0)
            {
                e->timer = LARGE_TURN;
                e->vx    = (s16)(-e->vx);       /* reverse march direction          */
            }
            /* but always bias toward the player so it presses the attack */
            if (p->x < e->x) e->facing = 1; else e->facing = 0;
            integrate(&e->x, &e->xSub, e->vx);
            /* VERTICAL: ride the spawn tier. obj16x16 (move sub_D5F3) reloads a
             * periodic velocity VECTOR from the $DB4B table (the direction flip we
             * model) -- a scripted patrol, not a player-Y chase. Keep it seated on
             * its entry tier. */
            seat_on_tier(e);

            anim_set(&e->av, SS_ANIM_LARGE_WALK);
            e->tile = anim_tick(&e->av);

            /* off either edge (it patrols both ways): a SCHEDULED large despawns
             * once it clears the left; a non-scheduled one recycles. */
            if (e->x < g_game.cameraX - 48)
            {
                if (e->fromSched) { e->active = 0; break; }
                e->x = (s16)(g_game.cameraX + SCREEN_W + 16);
                e->xSub = 0;
                e->vx = (s16)(-LARGE_SPEED);
                e->timer = LARGE_TURN;
            }
            else if (e->x > g_game.cameraX + SCREEN_W + 64)
            {
                e->x = (s16)(g_game.cameraX + SCREEN_W + 16);
                e->xSub = 0;
                e->vx = (s16)(-LARGE_SPEED);
                e->timer = LARGE_TURN;
            }
            break;
        }

        /* ----- EMITTER (classD $BF6B/$BF75): stationary periodic shot emitter ---
         * RE: $BF75 counts a fire-countdown $D,x down and on 0 spawns a projectile
         * child then reloads. We re-express: a FIXED emplacement (no march) that
         * fires a flat shot at the player every EMIT_FIRE_PERIOD frames (a brief
         * EMIT_FIRE pose each shot). Death (state 1) = burst. It holds station, so
         * it only leaves the scene by being killed or scrolling off the left. */
        case ET_EMITTER:
        {
            if (e->state == 1)                 /* dying */
            {
                anim_set(&e->av, SS_ANIM_EMIT_DEATH);
                e->tile = anim_tick(&e->av);
                if (anim_finished(&e->av)) e->active = 0;
                break;
            }

            e->facing = (p->x < e->x) ? 1 : 0;  /* face the player                 */

            /* fire on a cadence (the $BF75 countdown). Only when on-screen-ish so
             * off-screen emitters don't spam shots the player never sees. */
            {
                s16 sx = (s16)(e->x - g_game.cameraX);
                if (sx > -16 && sx < SCREEN_W + 16)
                {
                    if (e->fireTimer == 0 && --e->timer == 0)
                    {
                        emitter_fire((s16)(e->x + 4), (s16)(e->y + 4), p);
                        e->timer     = EMIT_FIRE_PERIOD;
                        e->fireTimer = EMIT_FIRE_ANIM;
                        anim_restart(&e->av, SS_ANIM_EMIT_FIRE);
                    }
                }
            }
            if (e->fireTimer)
                e->fireTimer--;
            else
                anim_set(&e->av, SS_ANIM_EMIT_IDLE);
            e->tile = anim_tick(&e->av);

            /* despawn only once it has scrolled well off the LEFT (it never marches) */
            if (e->x < g_game.cameraX - 32) e->active = 0;
            break;
        }

        /* ----- ORB (objSingle $E1C4/$E1F5): slow hovering lethal hazard --------
         * RE: a parent-linked unique object that settles relative to the scroll +
         * runs its own anim script (objSingle idx4 sub_E710 = "settle to floor").
         * It does NOT chase the player's tier. We re-express a slow leftward drift
         * (it enters from the right with the scroll) + a gentle vertical hover-bob
         * AROUND ITS SPAWN ROW (launchY) -- kept distinct from the COLLECTIBLE fruit
         * by its motion; it is lethal, never collected. Death (state 1) = burst. */
        case ET_ORB:
        {
            if (e->state == 1)                 /* dying */
            {
                anim_set(&e->av, SS_ANIM_ORB_DEATH);
                e->tile = anim_tick(&e->av);
                if (anim_finished(&e->av)) e->active = 0;
                break;
            }

            e->facing = (p->x < e->x) ? 1 : 0;
            integrate(&e->x, &e->xSub, (s16)(-ORB_SPEED));   /* drift in with scroll */

            /* gentle hover-bob around the spawn row (no player-Y tier chase): a
             * +-ORB_BOB_PIX sine offset from launchY, indexed by the phase. */
            e->phase++;
            {
                s16 off = (s16)(((s32)k_sin16[(e->phase >> 2) & 0x0F] * ORB_BOB_PIX) >> 6);
                e->y    = (s16)(e->launchY + off);
                e->ySub = 0;
            }

            anim_set(&e->av, SS_ANIM_ORB_FLOAT);
            e->tile = anim_tick(&e->av);

            if (e->x < g_game.cameraX - 32)        /* off-screen-left              */
            {
                if (e->fromSched) { e->active = 0; break; }
                e->x = (s16)(g_game.cameraX + SCREEN_W + 16);
                e->xSub = 0;
            }
            break;
        }

        /* ----- POPUP: floating "+N" score label (classK $0ED8 child) -----------
         * RE: the arcade spawns a score-child on a kill/pickup that draws the
         * floating value (sub_CBC8 anim / sub_CCB1 dispatch). We re-express it as a
         * short-lived entity that drifts UPWARD; game_render draws it as a HUD-font
         * "+N" (it carries no hardware sprite). Despawn when its life expires. */
        case ET_POPUP:
        {
            integrate(&e->y, &e->ySub, e->vy);    /* rise                          */
            if (e->timer == 0 || --e->timer == 0) e->active = 0;
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
static u8 lethal_contact(Player *p, const Entity *en)
{
    if (p->invuln || p->deadReq) return 0;      /* per-player i-frames / pending */

    /* For full-size enemies, re-test the body AABB here; the projectile path
     * already overlap-tested with its own box and passes an overlapping en. */
    if (en->type != ET_ENEMY_SHOT &&
        !aabb(p->x, p->y, PLAYER_W, PLAYER_H, en->x, en->y, BODY_W, BODY_H))
        return 0;

    p->deadReq = 1;                             /* game.c owns the consequence */
    if (p->hurt == 0) p->hurt = HURT_FRAMES;
    return 1;
}

/* Collision pass: SHOT<->ENEMY (defeat + score) and PLAYER<->ENEMY/ITEM. Runs
 * after motion so positions are current this frame. O(N*M) over a 24-slot pool
 * = trivial on the 68000. */
static void collision_pass(void)
{
    u16 i, j;
    u8  pk;

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

            /* select the killable archetypes + their death-state / score.
             * (walker/creatureC/compositeE/creatureG/large/emitter/orb use state
             * 1 = dying; the flyer/thrower/hopper use state 2 -- their state-
             * machine layouts.) */
            if      (en->type == ET_ENEMY_A)     { deathState = 1; award = SCORE_ENEMY;       }
            else if (en->type == ET_FLYER)       { deathState = 2; award = SCORE_FLYER;       }
            else if (en->type == ET_THROWER)     { deathState = 2; award = SCORE_THROWER;     }
            else if (en->type == ET_CREATURE_G)  { deathState = 1; award = SCORE_CREATURE_G;  }
            else if (en->type == ET_CREATURE_C)  { deathState = 1; award = SCORE_CREATURE_C;  }
            else if (en->type == ET_COMPOSITE_E) { deathState = 1; award = SCORE_COMPOSITE_E; }
            else if (en->type == ET_HOPPER)      { deathState = 2; award = SCORE_HOPPER;      }
            else if (en->type == ET_LARGE)       { deathState = 1; award = SCORE_LARGE;       }
            else if (en->type == ET_EMITTER)     { deathState = 1; award = SCORE_EMITTER;     }
            else if (en->type == ET_ORB)         { deathState = 1; award = SCORE_ORB;         }
            else continue;

            if (!en->active || en->state == deathState) continue;   /* dead/dying */

            if (aabb(s->x, s->y, SHOT_HALF_W * 2, SHOT_HALF_H * 2,
                     en->x, en->y, BODY_W, BODY_H))
            {
                if (en->hp > 1)
                {
                    en->hp--;                       /* tough enemy: another hit    */
                    snd_sfx_enemy();                /* hit-feedback SFX            */
                }
                else
                {
                    en->state = deathState;        /* enter death sequence      */
                    en->timer = ENEMY_DEATH_FRAMES;
                    en->vx    = 0;
                    /* credit the player whose shot scored the kill (co-op). */
                    g_game.players[s->owner & 1].score += award;
                    /* NOTE: the arcade shows a score child on enemy kills too
                     * (classK $0ED8), but we DON'T pop a "+N" per kill here -- in a
                     * busy fire-fight that would spawn many concurrent popup-glyph
                     * sprites and shave the frame budget. The iconic SonSon popup is
                     * the FRUIT one (low-frequency), which we DO show (see ET_ITEM
                     * collect below). Enemy kills still score + play the defeat SFX. */
                    snd_sfx_enemy();               /* enemy-defeat SFX (verified) */
                }
                s->active = 0;                      /* shot is consumed          */
                break;
            }
        }
    }

    /* PLAYER vs ENEMY/PROJECTILE (contact-hurt, arcade DCAC) + PLAYER vs ITEM
     * (collect), for EACH active co-op player. Every hostile body -- walker, flyer,
     * thrower, AND the lobbed projectile -- costs that player a life on overlap; the
     * lethal request is gated per-player to one life per touch (lethal_contact).
     * A player who is OUT (flow GAME_OVER) is skipped entirely. Fruit is credited
     * to whichever player collects it. */
    for (pk = 0; pk < g_game.numPlayers; pk++)
    {
        Player *p = &g_game.players[pk];
        if (p->flow == FLOW_GAME_OVER) continue;   /* out player: no contact/collect */

    for (j = 0; j < ENTITY_MAX; j++)
    {
        Entity *en = &g_entities[j];
        if (!en->active) continue;

        switch (en->type)
        {
            /* hostile bodies: lethal on contact unless already in their death
             * sequence (a dying/corpse enemy is harmless). Death-state index
             * differs by archetype (1 for the walkers/creatures/large/emitter/orb,
             * 2 for the flyer/thrower/hopper) -- match each so a corpse can't hurt. */
            case ET_ENEMY_A:
            case ET_CREATURE_G:
            case ET_CREATURE_C:
            case ET_COMPOSITE_E:
            case ET_LARGE:
            case ET_EMITTER:
            case ET_ORB:
                if (en->state != 1) lethal_contact(p, en);
                break;
            case ET_FLYER:
            case ET_THROWER:
            case ET_HOPPER:
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

            /* FRUIT collect: SonSon's signature scoring. Award by fruit TYPE
             * (escalating values), play the item SFX, pop a floating "+N", and
             * vanish. (The arcade spawns a classK $0ED8 score-child here -- the
             * popup; sub_CBC8 animates it.) */
            case ET_ITEM:
                if (aabb(p->x, p->y, PLAYER_W, PLAYER_H,
                         en->x, en->y, BODY_W, BODY_H))
                {
                    u16 val = k_fruit_score[en->fruit < FRUIT_TYPE_COUNT
                                            ? en->fruit : 0];
                    p->score += val;                 /* credit the collector (co-op) */
                    spawn_popup((s16)(en->x), (s16)(en->y), val);
                    /* Two arcade pickup tiers (latch-verified): 0x02 fires on the
                     * small +10 pickup, 0x0C on the high +500 pickup (a distinct,
                     * higher cue). Map the native fruit economy onto them by value:
                     * the high-value fruit (ORANGE 500 / APPLE 1000 / GOLD 2000)
                     * gets the bonus SFX, the low CHERRY (300) the plain item SFX. */
                    if (val >= 500) snd_sfx_bonus();  /* high-value pickup (cmd 0x0C) */
                    else            snd_sfx_item();    /* low-value pickup  (cmd 0x02) */
                    en->active = 0;                  /* fruit eaten                */
                }
                break;

            default:
                break;
        }
    }
    }   /* end per-player loop */
}

/* ------------------------------------------------------------------------- */
void entity_update_all(void)
{
    u16 i;
    u8  pk;

    /* decay each active player's hurt-flash window (per-player). */
    for (pk = 0; pk < g_game.numPlayers; pk++)
        if (g_game.players[pk].hurt) g_game.players[pk].hurt--;

    /* Faithful per-level wave scheduler: spawn the next scheduled enemies as the
     * camera advances, capped at the arcade on-screen count (no recycle pile-up). */
    entity_schedule_tick();

    /* Enemy AI tracks a LIVE player: P1 normally, P2 if P1 is out (co-op). (The
     * arcade tracks the player(s); nearest-of-two targeting is a later refinement.) */
    {
        const Player *track = &g_game.players[0];
        if (g_game.numPlayers > 1 && g_game.players[0].flow == FLOW_GAME_OVER)
            track = &g_game.players[1];

        for (i = 0; i < ENTITY_MAX; i++)
        {
            Entity *e = &g_entities[i];
            if (e->active) entity_update_one(e, track);
        }
    }

    collision_pass();
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
/* Cap on POPUP glyph sprites appended per frame (after the entity sprites). Each
 * popup "+N" is up to ~6 glyphs; capping the TOTAL glyphs keeps us well under the
 * 80-sprite ceiling alongside the player + entities. (Defined in entity.h so the
 * SAT buffer in game.c can be sized to match.) */
#define POPUP_GLYPH_MAX     ENTITY_POPUP_GLYPH_MAX

u16 entity_render_all(u16 *sat, u16 firstSlot)
{
    u16 count  = 0;            /* entity BODY sprites (capped at ENTITY_SAT_MAX) */
    u16 glyphs = 0;            /* popup glyph sprites (capped at POPUP_GLYPH_MAX)*/
    u16 slot   = firstSlot;
    u16 i;

    /* ONE pass over the pool: entity bodies (16x16) AND floating "+N" score-popup
     * glyphs (1x1 font-tile sprites), built into one linked SAT chain.
     *   - Bodies use PAL1 + the entity's baked tile (capped at ENTITY_SAT_MAX).
     *   - Popups carry NO body tile; they emit cached glyph sprites pointing at
     *     the SGDK font (TILE_FONT_INDEX + (ch-32), PAL3 -- the font palette
     *     game.c set up). The "+N" text was formatted ONCE at spawn (e->popupStr),
     *     so this does zero per-frame intToStr/strlen. Riding the same SAT DMA,
     *     the popups cost no active-display VRAM writes (the arcade's classK
     *     $0ED8 score-child, sub_CBC8 anim, re-expressed cheaply).
     * A single loop avoids a second 48-slot scan. */
    for (i = 0; i < ENTITY_MAX; i++)
    {
        Entity *e = &g_entities[i];
        s16 sx, sy;
        if (!e->active) continue;

        sx = (s16)(e->x - g_game.cameraX);
        sy = e->y;

        if (e->type == ET_POPUP)
        {
            u8 k;
            if (glyphs >= POPUP_GLYPH_MAX) continue;
            if (sy <= -8 || sy >= SCREEN_H) continue;
            for (k = 0; k < e->popupLen && glyphs < POPUP_GLYPH_MAX; k++)
            {
                s16 gx = (s16)(sx + k * 8);
                u16 ftile = (u16)(TILE_FONT_INDEX + ((u8)e->popupStr[k] - 32));
                if (gx <= -8 || gx >= SCREEN_W) continue;
                sat[slot * 4 + 0] = (u16)((0x80 + sy) & 0x3FF);
                sat[slot * 4 + 1] = (u16)((SPRITE_SIZE(1, 1) << 8) | (slot + 1));
                sat[slot * 4 + 2] = (u16)TILE_ATTR_FULL(PAL3, 1, 0, 0, ftile);
                sat[slot * 4 + 3] = (u16)((0x80 + gx) & 0x1FF);
                slot++; glyphs++;
            }
            continue;
        }

        /* entity body (16x16). Cap the BODY sprites; popups have their own cap. */
        if (count >= ENTITY_SAT_MAX) continue;
        /* cull off-screen sprites (with a small margin) so the SAT stays lean */
        if (sx <= -BODY_W || sx >= SCREEN_W || sy <= -BODY_H || sy >= SCREEN_H)
            continue;

        /* (dying enemies no longer blink: the baked ENEMY_DEATH animation -- the
         * 307->308->309->310 collapse, driven in entity_update_one -- IS the
         * death feedback now, so we draw every frame of it.) */
        sat[slot * 4 + 0] = (u16)((0x80 + sy) & 0x3FF);
        /* link to the NEXT slot; the loop tail patches the final link to 0 */
        sat[slot * 4 + 1] = (u16)((SPRITE_SIZE(2, 2) << 8) | (slot + 1));
        /* flipH = !facing: the arcade ENEMY/projectile sprite art is drawn facing
         * LEFT at flip0 (opposite the PLAYER art, which faces right at flip0 -- the
         * player render in game.c uses flipH=facing and is correct). The AI sets
         * e->facing toward the player CORRECTLY (verified live: facing values track
         * the player's side), so only the sprite orientation was inverted -- enemies
         * faced AWAY from the player, and throwers visually faced/"shot" backward
         * toward other enemies. Inverting the flip here makes every enemy face the
         * player, matching the arcade (white skulls face the hero, tools/arcdemo). */
        sat[slot * 4 + 2] = (u16)TILE_ATTR_FULL(PAL1, 0, 0, (u8)(!e->facing), e->tile);
        sat[slot * 4 + 3] = (u16)((0x80 + sx) & 0x1FF);

        slot++;
        count++;
    }

    /* terminate the chain: the last appended sprite links to 0 */
    if (slot != firstSlot) sat[(slot - 1) * 4 + 1] =
        (u16)((sat[(slot - 1) * 4 + 1] & 0xFF00) | 0);

    return (u16)(slot - firstSlot);   /* total sprites appended (bodies + glyphs) */
}

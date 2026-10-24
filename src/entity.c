#include <genesis.h>
#include "game.h"
#include "survival.h"
#include "entity.h"
#include "ss_gfx.h"
#include "ss_anim.h"     /* baked player/enemy animation frames + table */
#include "ss_collision.h"/* SS_TIER_Y[6] -- the 6 terrain tiers (native sub_A847) */
#include "arcade_food_bonus.h"
#include "arcade_food_world.h"
#include "sound.h"       /* snd_sfx_enemy / snd_sfx_item (verified $3010 cmds) */
#include "arcade_waves.h"
#include "arcade_foes.h"
#include "arcade_i.h"
#include "arcade_main_pool.h"
#include "arcade_c_pool.h"
#include "arcade_d_pool.h"
#include "arcade_d_combat.h"
#include "arcade_d_render.h"
#include "arcade_f_pool.h"
#include "arcade_f_combat.h"
#include "arcade_f_render.h"
#include "arcade_e_pool.h"
#include "arcade_e_combat.h"
#include "arcade_e_render.h"
#include "arcade_e_upload.h"
#include "arcade_tracking.h"
#include "arcade_c_combat.h"
#include "arcade_c_render.h"
#include "arcade_main_render.h"
#include "arcade_native_admission.h"
#include "arcade_large_present.h"
#include "arcade_k.h"
#include "arcade_collision.h"
#include "arcade_rewards.h"
#include "arcade_foe_bonus.h"
#include "arcade_shots.h"
#include "arcade_difficulty.h"
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
#define SHOT_SPEED          0x0600      /* 4.0 px/frame                          */
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

/* ---- LARGE (obj16x16 $D57F/$D58B/$D5F3) -------------------------------
 * D58B initializes remaining hits to 7; EE8E decrements once per accepted
 * shot and enters death only on the transition 1 -> 0. Verified by the
 * original CPU fixture arcade_large_damage_trace.lua. Movement below remains
 * an approximation pending replacement of the complete D54B state machine. */
#define LARGE_HP            7
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
/* Monotonic until reset: direct allocations keep the legacy range observable. */
static u8 legacy_pool_used;
static Entity *entity_spawn_range(u8 type,s16 x,s16 y,u16 first,u16 end)
{
    u16 i;
    for (i = first; i < end; i++)
    {
        Entity *e = &g_entities[i];
        if (e->active) continue;

        if(i>=24)legacy_pool_used=1;
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
        e->owner     = 0;
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
                e->hp     = LARGE_HP;      /* D58B: seven accepted hits             */
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

/* G/H own six slots each. Ordinary allocations cannot steal reserved members. */
Entity *entity_spawn(u8 type,s16 x,s16 y)
{
    return entity_spawn_range(type,x,y,g_survivalRun?0:24,ENTITY_MAX);
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

/* Arcade waves: verified native producer/G/H controllers below. Remaining
 * classes retain provisional one-body spawns and approximate combat behavior. */

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

/* Wave groups read combat grade $96, which can exceed the scenery base cap. */
static u8 sched_group_for_round(void)
{
    u16 stage = g_survivalRun ? g_game.stage : (g_arcadeCombatDifficulty>>8);
    u8  g = (u8)(((stage & 0x30) >> 1) / 8);
    if (g > 3) g = 3;
    return g;
}

/* ---- Scheduler state (file-static; reset by entity_schedule_init) ---------- */
static u16 survival_delay;
static u8 survival_cursor;
static ArcadeWaves arcade_waves;
static ArcadeRng arcade_rng;
void entity_arcade_title_seed(void)
{
    /* 933E owns reseeding. New-game8F5B preserves78:79 and7A. */
    arcade_rng.seed=0x87cd;arcade_rng.last=0;
}
static u8 arcade_bonus_next,arcade_bonus_limit;
u8 entity_arcade_conversion_contact(u16 x,u8 y,u8 owner)
{
    return arcade_powerup_collect_conversion(&arcade_food_conversion,x,y,owner,
        &arcade_bonus_limit,&arcade_bonus_next,&arcade_food_conversion_pulse);
}
static ArcadeFoe arcade_foes[12];
static ArcadeFoeWeapon arcade_weapons[4];
static ArcadeFoeWorld arcade_world;
static ArcadeLargeActor arcade_large_actors[3];
static ArcadeKProjectile arcade_k_actors[4];
static u16 large_last_sat[32*4],large_last_count,large_last_visible;
static u8 large_last_resident;
static ArcadeIActor arcade_i_actors[4];
static ArcadeIWave arcade_i_wave;
static ArcadeMainPool arcade_main;
static ArcadeCPool arcade_c;
static ArcadeDPool arcade_d;
static ArcadeEPool arcade_e;
static ArcadeFPool arcade_f;
static u16 arcade_e_ready[2];
static u8 arcade_main_ready;
static u8 arcade_i_live,arcade_i_reservation;
static u8 arcade_shot_budget;
static ArcadeAdmissionCache arcade_admission_cache;
#define arcade_admission (arcade_admission_cache.result)
/* Map real pool owners once, producing both render admission and readiness.
 * Legacy families still have provisional populations, ordered by source family. */
static void entity_sprite_admission(void)
{
    ArcadeAdmissionInput input;u8 k,active;
    const u8 end=legacy_pool_used?48:24;
    if(!legacy_pool_used)
        for(k=0;k<6;k++){input.words[6+k]=0;input.words[12+k]=0xffffffffUL;}
    for(k=0;k<2;k++)input.state.e[k]=arcade_e.actors[k].active;
    for(k=0;k<4;k++)input.state.f[k]=arcade_f.actors[k].active;
    input.state.reserved=0;
    static const u8 rank[ET_POPUP+1]={6,0,6,6,4,6,6,6,1,3,6,6,2,5,255};
    const Entity *e=g_entities;
    active=arcade_food_pickup.active;if(active && !(active&128))input.state.reserved++;
    active=arcade_food_conversion.active;if(active && !(active&128))input.state.reserved++;
    active=arcade_food_carrier.active;if(active && !(active&128))input.state.reserved++;
    for(k=0;k<g_game.numPlayers;k++)
    {active=(u8)(game_player_target_flags(k)>>8);if(active && !(active&128))input.state.reserved++;}
    for(k=0;k<16;k++)input.state.main[k]=arcade_main.actors[k].active;
    for(k=0;k<6;k++)input.state.c[k]=arcade_c.actors[k].active;
    for(k=0;k<4;k++)input.state.d[k]=arcade_d.actors[k].active;
    for(k=0;k<end;k++,e++)
    {
        active=e->active;
        if(!active || (active&128))
        {input.state.entities[k]=0;if(k>=24)input.state.ranks[k-24]=255;continue;}
        input.state.entities[k]=e->type==ET_POPUP?0:active;
        if(k>=24)input.state.ranks[k-24]=e->type<=ET_POPUP?rank[e->type]:6;
    }
    for(k=0;k<3;k++)input.state.large[k]=arcade_large_actors[k].body.active;
    for(k=0;k<4;k++)input.state.children[k]=arcade_weapons[k].active?arcade_weapons[k].active:arcade_k_actors[k].active;
    arcade_native_admission_refresh(&input,&arcade_admission_cache);
    arcade_main_ready=arcade_admission.remaining;
    arcade_shot_budget=(u8)(arcade_main_ready!=0);
}
/* Logical readiness can be published between catch-up steps without SAT/DMA.
 * Rendering uses the same mapper, so collision/spawn budgets stay identical. */
void entity_arcade_prepare_admission(void){entity_sprite_admission();}
u8 entity_arcade_sprite_ready(void){return arcade_shot_budget;}
/* Source23:24 is one counter. Keep the byte symbols for source-state probes;
 * advance both through this owner, never derive one from host VBlank time.
 * Boot/menu phase and source-rate pacing are separate integration work. */
static u8 wave_clock, wave_clock_high;
u8 entity_arcade_clock(void){return wave_clock;}
u16 entity_arcade_clock_full(void){return ((u16)wave_clock_high<<8)|wave_clock;}
void entity_arcade_clock_advance(void)
{
    if(++wave_clock==0)++wave_clock_high;
}
static u8 formation_reservation[2];
static u8 player_shot_available[2];
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

/* Reset once for a new game. Visual segment seams do not reset controllers. */
static void entity_schedule_reset(u8 cold)
{
    survival_delay = 0; survival_cursor = 0;
    if(cold)arcade_waves_reset(&arcade_waves,255);
    else arcade_waves_course_begin(&arcade_waves);
    arcade_bonus_next=0;arcade_bonus_limit=255; /* Original8F6E: first FF pickup wraps to0. */
    /* RNG belongs to the title/session lifecycle, not pool initialization. */
    /* Source new-game8F5B does not reset23:24. */
    if(cold)
    {
        memset(arcade_i_actors,0,sizeof(arcade_i_actors));
        memset(arcade_large_actors,0,sizeof(arcade_large_actors));
        memset(arcade_k_actors,0,sizeof(arcade_k_actors));
    }
    else
    {
        /* 9793 clears only each actor's active byte on course startup. */
        u8 i;
        for(i=0;i<4;i++){arcade_i_actors[i].body.active=0;arcade_k_actors[i].active=0;}
        for(i=0;i<3;i++)arcade_large_actors[i].body.active=0;
    }
    arcade_large_upload_reset();large_last_count=large_last_visible=0;large_last_resident=0;
    if(cold)memset(&arcade_i_wave,0,sizeof(arcade_i_wave));
    else arcade_i_wave.control=arcade_waves.controls[SS_WAVE_BIT_CLASSI];
    arcade_i_live=arcade_i_reservation=0;
    arcade_main_pool_reset(&arcade_main,cold);
    arcade_c_pool_reset(&arcade_c,cold);
    arcade_c.wave.control=arcade_waves.controls[SS_WAVE_BIT_CLASSC];
    arcade_d_pool_reset(&arcade_d,cold);
    arcade_e_pool_reset(&arcade_e,cold);
    arcade_f_pool_reset(&arcade_f,cold);
    arcade_f.wave.control=arcade_waves.controls[SS_WAVE_BIT_CLASSF];
    arcade_e.wave=arcade_waves.controls[SS_WAVE_BIT_CLASSE];
    arcade_e_upload_reset();arcade_e_ready[0]=arcade_e_ready[1]=65535;
    arcade_d.wave.control=arcade_waves.controls[SS_WAVE_BIT_CLASSD];
    arcade_main.wave.control=arcade_waves.controls[SS_WAVE_BIT_ENEMYMAIN];
    arcade_main.wave.sequence=arcade_main.wave.cursor=arcade_main.wave.remaining=0;
    arcade_main_ready=24;
    {
        u8 i;for(i=0;i<12;i++){arcade_foes[i].body.active=0;if(cold){arcade_foes[i].body.xf=arcade_foes[i].body.yf=0;arcade_foes[i].target=0;}}
        for(i=0;i<4;i++){arcade_weapons[i].active=0;if(cold)arcade_weapons[i].xf=arcade_weapons[i].yf=0;}
        arcade_shot_budget=1;
    }
    formation_reservation[0]=formation_reservation[1]=0;
    s_sched_gcursor = 0;
}

void entity_schedule_init(void){entity_schedule_reset(1);}

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
            case SS_WAVE_BIT_CLASSI: return; /* Dedicated verified four-slot controller below. */
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

/* Verified producer and G/H controllers. Other class controllers below still
 * use the older single-birth fallback until their state machines are recovered.
 * This runs after all actors/collisions, as in $A5E2 -> $EF51. */
static void formation_schedule(u8 family,u16 tracked_x)
{
    const u8 bit=family?SS_WAVE_BIT_CLASSH:SS_WAVE_BIT_CLASSG;
    const u16 first=family?6:0;
    u8 i,live=0;
    ArcadeBirth births[6];
    for(i=0;i<6;i++)if(g_entities[first+i].active)live++;
    if(!arcade_formation_step(&arcade_waves.controls[bit],family,live,wave_clock,
       (u8)(g_arcadeCombatDifficulty>>8),(u16)g_game.cameraX,tracked_x,&arcade_rng,births,
       &formation_reservation[family]))return;
    for(i=0;i<6;i++)
    {
        Entity *e=entity_spawn_range(family?ET_ENEMY_A:ET_CREATURE_G,
                       (s16)births[i].x,(s16)(239-births[i].y),first+i,first+i+1);
        ArcadeFormationActor *a=&arcade_foes[first+i].body;
        /* Dedicated slots are free when this controller's live count is zero. */
        if(!e)continue;
        e->active=128;e->fromSched=1;e->fruit=(u8)(bit+1);e->owner=128;
        e->facing=births[i].facing==3;
        a->x=births[i].x;a->y=births[i].y;
        a->active=128;a->state=a->phase=0;a->delay=births[i].delay;
        a->facing=births[i].facing;
        if(!family)arcade_foes[first+i].kind=0;
    }
}
void entity_schedule_tick(void)
{
    if(g_survivalRun)
    {
        static const u8 kinds[6]={1,5,1,6,4,5};
        if(survival_delay) {survival_delay--;return;}
        survival_delay=(u16)(120-g_survivalPressure*12);
        if(sched_live_count() < (u16)(4+g_survivalPressure*2))
        {
            wave_spawn_bit(kinds[survival_cursor%6],survival_cursor);
            survival_cursor++;
        }
        return;
    }
    s16 changed=arcade_waves_step(&arcade_waves,(u8)((u16)g_game.cameraX>>8),wave_clock);
    u8 tracked=arcade_tracking_player(g_game.numPlayers>1,
        (u16)g_game.players[0].x,game_player_target_flags(0),
        (u16)g_game.players[1].x,game_player_target_flags(1));
    const Player *track=&g_game.players[tracked];
    if(changed>=0 && changed!=SS_WAVE_BIT_ENEMYMAIN && changed!=SS_WAVE_BIT_CLASSC && changed!=SS_WAVE_BIT_CLASSD && changed!=SS_WAVE_BIT_CLASSE && changed!=SS_WAVE_BIT_CLASSF && changed!=SS_WAVE_BIT_CLASSG && changed!=SS_WAVE_BIT_CLASSH && changed!=SS_WAVE_BIT_CLASSI && changed!=SS_WAVE_BIT_OBJ16 &&
       arcade_waves.controls[changed].active && sched_live_count()<SPAWN_ONSCREEN_CAP)
        wave_spawn_bit((u8)changed,arcade_waves.controls[changed].parameter);
    /* EF51: ordinary pool controller precedes the other enemy controllers.
     * Both source23 and24 belong to the same logical counter. */
    arcade_main.wave.control=arcade_waves.controls[SS_WAVE_BIT_ENEMYMAIN];
    arcade_main_pool_schedule(&arcade_main,arcade_main_ready,wave_clock_high,wave_clock,
        (u8)(g_arcadeCombatDifficulty>>8),(u16)g_game.cameraX,&arcade_rng);
    arcade_waves.controls[SS_WAVE_BIT_ENEMYMAIN]=arcade_main.wave.control;
    arcade_c.wave.control=arcade_waves.controls[SS_WAVE_BIT_CLASSC];
    arcade_c_pool_schedule(&arcade_c,wave_clock,(u8)(g_arcadeCombatDifficulty>>8),(u16)g_game.cameraX);
    arcade_waves.controls[SS_WAVE_BIT_CLASSC]=arcade_c.wave.control;
    arcade_d.wave.control=arcade_waves.controls[SS_WAVE_BIT_CLASSD];
    arcade_d_pool_schedule(&arcade_d,wave_clock,(u8)(g_arcadeCombatDifficulty>>8),(u16)g_game.cameraX,(u16)track->x);
    arcade_waves.controls[SS_WAVE_BIT_CLASSD]=arcade_d.wave.control;
    arcade_e.wave=arcade_waves.controls[SS_WAVE_BIT_CLASSE];
    arcade_e_pool_schedule(&arcade_e,wave_clock,(u8)(g_arcadeCombatDifficulty>>8),(u16)g_game.cameraX);
    arcade_waves.controls[SS_WAVE_BIT_CLASSE]=arcade_e.wave;
    arcade_f.wave.control=arcade_waves.controls[SS_WAVE_BIT_CLASSF];
    arcade_f_pool_schedule(&arcade_f,wave_clock,(u8)(g_arcadeCombatDifficulty>>8),(u16)g_game.cameraX);
    arcade_waves.controls[SS_WAVE_BIT_CLASSF]=arcade_f.wave.control;
    /* $EF51 calls G before H, although the producer bits are ordered H then G. */
    formation_schedule(0,(u16)track->x);
    formation_schedule(1,(u16)track->x);
    {
        ArcadeBirth birth;
        u8 slot=arcade_i_wave.cursor;
        arcade_i_wave.control=arcade_waves.controls[SS_WAVE_BIT_CLASSI];
        if(arcade_i_wave_step(&arcade_i_wave,arcade_i_live,wave_clock,
                              (u16)g_game.cameraX,&arcade_i_reservation,&birth))
        {
            ArcadeIActor *a=&arcade_i_actors[slot];
            Entity *e=&g_entities[20+slot];
            /* F9E0 writes the fixed slot directly, retaining fractions. */
            a->body.x=birth.x;a->body.y=birth.y;a->body.facing=birth.facing;
            a->body.active=128;a->body.state=a->body.phase=0;
            e->active=128;e->type=ET_HOPPER;e->fromSched=1;e->owner=128;
            e->x=(s16)birth.x;e->y=(s16)(239-birth.y);e->state=0;
        }
        arcade_waves.controls[SS_WAVE_BIT_CLASSI]=arcade_i_wave.control;
    }
    arcade_large_wave_step(&arcade_waves.controls[SS_WAVE_BIT_OBJ16],
                           arcade_large_actors,(u16)g_game.cameraX,
                           (u8)(g_arcadeCombatDifficulty>>8));

}

/* SEAMLESS-RIBBON coordinate wrap (see entity.h). Shift every live entity's world
 * X by dx. Called from game.c the frame it wraps cameraX by the ribbon width;
 * because cameraX gets the same dx, screen positions (x - cameraX) are unchanged
 * -> the wrap is invisible. (The wave scheduler needs no shift: it derives its
 * wave-id from the wrapped cameraX, which wraps in lockstep -- see wave_id_for_camera.) */
void entity_shift_worldx(s16 dx)
{
    u16 i;
    if(!g_survivalRun)
    {
        for(i=0;i<6;i++)if(arcade_c.actors[i].active)arcade_c.actors[i].x+=dx;
        for(i=0;i<4;i++)if(arcade_d.actors[i].active)arcade_d.actors[i].x+=dx;
        for(i=0;i<2;i++)if(arcade_e.actors[i].active)arcade_e.actors[i].x+=dx;
        for(i=0;i<4;i++)if(arcade_f.actors[i].active)arcade_f.actors[i].x+=dx;
        for(i=0;i<16;i++)if(arcade_main.actors[i].active){arcade_main.actors[i].x+=dx;arcade_main.actors[i].origin+=dx;}
        for(i=0;i<3;i++)if(arcade_large_actors[i].body.active)arcade_large_actors[i].x+=dx;
        for(i=0;i<4;i++)if(arcade_k_actors[i].active){arcade_k_actors[i].x+=dx;arcade_k_actors[i].origin+=dx;}
    }
    for (i = 0; i < ENTITY_MAX; i++)
        if (g_entities[i].active)
        {
            g_entities[i].x = (s16)(g_entities[i].x + dx);
            if(!g_survivalRun && i<12)arcade_foes[i].body.x=(u16)(arcade_foes[i].body.x+dx);
            if(!g_survivalRun && i>=12 && i<16)
            {arcade_weapons[i-12].x=(u16)(arcade_weapons[i-12].x+dx);arcade_weapons[i-12].origin=(u16)(arcade_weapons[i-12].origin+dx);}
            if(!g_survivalRun && i>=20 && i<24)arcade_i_actors[i-20].body.x=(u16)(arcade_i_actors[i-20].body.x+dx);
            if(g_entities[i].type==ET_SHOT)g_entities[i].launchY=(s16)(g_entities[i].launchY+dx);
        }
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

static void entity_reset(u8 cold)
{
    u16 i;
    for (i = 0; i < ENTITY_MAX; i++) g_entities[i].active = 0;
    legacy_pool_used=0;
    player_shot_available[0]=player_shot_available[1]=2;
    if(cold && !g_survivalRun)
        for(i=16;i<20;i++)memset(&g_entities[i],0,sizeof(Entity));
    /* per-player hurt/invuln/deadReq are reset by game_init (in Player). */
    if(g_survivalRun)spawn_ambient(); /* Standard play uses the original food pool. */
    entity_schedule_reset(cold);
}

void entity_init(void){entity_reset(1);}

void entity_course_restart(u8 two_players,const u8 out[2])
{
    /* 9A18 returns through96A6/8FBC, not the new-game8F5B initializer.
     * In particular78/79/7A,24 and7B/7C survive. The existing native pool
     * reset also owns renderer caches; save these session values around it. */
    const u8 shots0=player_shot_available[0],shots1=player_shot_available[1];
    const ArcadeRng rng=arcade_rng;
    const u8 clock=wave_clock,clock_high=wave_clock_high,next=arcade_bonus_next,limit=arcade_bonus_limit;
    entity_reset(0);
    arcade_rng=rng;wave_clock=clock;wave_clock_high=clock_high;
    arcade_bonus_next=next;arcade_bonus_limit=limit;
    /* 9094 writes parent+$0C only on each eligible player branch. */
    player_shot_available[0]=out[0]?shots0:2;
    player_shot_available[1]=(two_players && !out[1])?2:shots1;
}

/* ------------------------------------------------------------------------- */
/* AD10: two fixed shot records per player, gated by the parent counter.
 * game.c supplies the rising edge and the pre-drift launch pose. */
bool entity_player_fire(u8 owner, s16 px, s16 py, u8 facing)
{
    u8 i,live=0,used=0;
    if(!g_survivalRun)
    {
        u16 first=(u16)(16+owner*2);
        ArcadeFireInput request;
        ArcadeShotBirth birth;
        Entity *shot;
        u8 selected;
        request.x=(u16)px;request.y=(u8)(239-py);request.facing=facing;
        /* Height correction already belongs to the captured AD10 fire pose. */
        request.state=0;request.kind=0;request.available=player_shot_available[owner];
        request.input=1;request.previous=0;
        selected=arcade_player_shot_fire(&request,g_entities[first].active,&birth);
        if(selected==255)return FALSE;
        shot=&g_entities[first+selected];
        shot->active=1;shot->type=ET_SHOT;shot->state=shot->phase=shot->fireTimer=0;
        shot->x=(s16)birth.x;shot->y=(s16)(239-birth.y);
        shot->lane=birth.facing;shot->facing=(u8)(birth.facing==3);
        shot->owner=owner;shot->fruit=selected;
        /* Integer coordinates are copied; fractions and unused fields survive. */
        return TRUE;
    }
    /* Two independent player slots: the arcade gives each player its OWN shot pool
     * (P1 $04E0, P2 $0520), so each may have one shot live independently. Refuse
     * only while THIS owner's shot is still in flight. */
    for (i = 0; i < ENTITY_MAX; i++)
        if (g_entities[i].active && g_entities[i].type == ET_SHOT &&
            g_entities[i].owner == owner)
        {live++;used|=(u8)(1<<(g_entities[i].fruit&1));}
    if(live>=2)return FALSE;
    {
        Entity *s = entity_spawn(ET_SHOT, px, py);
        if (!s) return FALSE;
        s->launchY=px; /* Shot-specific reuse: source launch X ($8,x), not Y. */
        s->owner  = owner;                         /* credit this player on a kill */
        s->fruit  = (used&1)?1:0; /* Stable source shot slot, retained during puff. */
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
    /* Equally spaced tiers, with ties favoring the earlier/lower tier just as
     * the scan below. Ordinary seated enemies stay in this admitted range. */
    if(e->y>=40 && e->y<=200)
    {
        e->y=SS_TIER_Y[(u16)(215-e->y)>>5];e->ySub=0;return;
    }
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
    /* Positive box sizes: overlap admits differences [-aw+1,bw-1].
     * Keep subtraction wide so opposite s16 extremes cannot wrap together. */
    return (u8)((u32)((s32)ax-bx+aw-1)<(u32)(aw+bw-1) &&
                (u32)((s32)ay-by+ah-1)<(u32)(ah+bh-1));
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
static inline __attribute__((always_inline)) void native_foe_update(Entity *e,u16 i)
{

        ArcadeFoe *a=&arcade_foes[i];
        if(a->body.state==255)
            arcade_foe_bonus_step(a,arcade_world.camera,arcade_world.purge,
                                  &arcade_bonus_next,arcade_bonus_limit);
        else
        {
            u8 k,reserved=0;
            if(i<6 && a->body.state==3)
                for(k=0;k<4;k++)if(arcade_k_actors[k].active)reserved|=(u8)(1u<<k);
            if(i<6)arcade_foe_step_g(a,(u8)i,&arcade_world,
                            game_arcade_floor,0,arcade_weapons,reserved);
            else arcade_foe_step_h(a,(u8)(i-6),&arcade_world,
                            game_arcade_floor,0,arcade_weapons);
        }
        e->active=a->body.active;
}
static inline __attribute__((always_inline)) void native_i_update(Entity *e,u16 index)
{
        ArcadeIActor *a=&arcade_i_actors[index-20];
        arcade_i_actor_step(a,(u16)g_game.cameraX,arcade_world.scroll_stopped,&arcade_i_live);
        e->active=a->body.active;
}
static void entity_update_one(Entity *e, const Player *p,u8 survival)
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
            if(!survival)
            {
                ArcadePlayerShot a;
                a.x=(u16)e->x;a.origin=(u16)e->launchY;a.xf=(u8)e->xSub;
                a.y=(u8)(239-e->y);a.yf=(u8)e->ySub;a.active=e->active;
                a.state=e->phase;a.phase=e->fireTimer;a.facing=e->lane;
                a.timer=e->timer;a.anim_timer=e->av.timer;a.cursor=e->av._pad;a.pose=e->av.frame;
                if(e->state && a.state<2){a.state=2;a.phase=0;}
                arcade_player_shot_step(&a,(u16)g_game.cameraX,&player_shot_available[e->owner&1]);
                e->x=(s16)a.x;e->xSub=a.xf;e->launchY=(s16)a.origin;e->active=a.active;
                e->phase=a.state;e->fireTimer=a.phase;e->state=(u8)(a.state>=2);
                e->timer=a.timer;e->av.timer=a.anim_timer;e->av._pad=a.cursor;e->av.frame=a.pose;
                e->tile=ss_anims[a.pose<2?SS_ANIM_ESHOT_FLY:SS_ANIM_SHOT_END].frames[a.pose&1].tile;
                break;
            }
            if(e->state==1)
            {
                if(e->timer==0 || --e->timer==0){e->active=0;break;}
                anim_set(&e->av,SS_ANIM_SHOT_END);e->tile=anim_tick(&e->av);break;
            }
            if((u8)(e->x-g_game.cameraX)>=0xF0){e->active=0;break;}
            integrate(&e->x,&e->xSub,e->vx);
            anim_set(&e->av,SS_ANIM_ESHOT_FLY);e->tile=anim_tick(&e->av);
            if((u16)(e->x-e->launchY+0x60)>=0xC0)
            {e->state=1;e->timer=5;anim_restart(&e->av,SS_ANIM_SHOT_END);}
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

        /* ----- LARGE: legacy motion pending full D54B replacement ----------
         * Seven-hit durability is verified against D58B/EE8E. The lane patrol,
         * direction reversal and death animation below are still approximations.
         * Source $D,X=16 is an entry countdown, not a collision-box dimension;
         * D5F3 selects an entry vector once, then D612 advances to pursuit. */
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

/* E2BF/E373: main pool precedes all other ordinary shot families. */
static void main_shot_collisions(void)
{
    if(!arcade_main_contact_presence)return;
    u8 k,owner=wave_clock&1;
    for(k=0;k<2;k++)
    {
        Entity *s=&g_entities[16+owner*2+k];
        ArcadePlayerShot shot;ArcadeMainHit hit;u8 slot;
        if(s->active!=1)continue;
        slot=arcade_main_shot_target(arcade_main.actors,(u16)s->x,(u8)(239-s->y),s->active);
        if(slot==255)continue;
        shot.active=s->active;shot.state=s->phase;shot.phase=s->fireTimer;
        arcade_main_hit(&arcade_main.actors[slot],&shot,owner,g_arcadeCombatDifficulty,&hit);
        s->active=shot.active;s->phase=shot.state;s->fireTimer=shot.phase;s->state=1;
        game_score_add(&g_game.players[owner],hit.points);g_arcadeCombatDifficulty=hit.combat;
        snd_command(hit.sound_hit);if(hit.sound_kill)snd_command(hit.sound_kill);
    }
}

static void c_shot_collisions(void)
{
    if(!arcade_c_contact_presence)return;
    u8 k,owner=wave_clock&1;
    for(k=0;k<2;k++)
    {
        Entity *s=&g_entities[16+owner*2+k];
        ArcadePlayerShot shot;ArcadeCHit hit;u8 slot;
        if(s->active!=1)continue;
        slot=arcade_c_shot_target(arcade_c.actors,(u16)s->x,(u8)(239-s->y),s->active);
        if(slot==255)continue;
        shot.active=s->active;shot.state=s->phase;shot.phase=s->fireTimer;
        arcade_c_hit(&arcade_c.actors[slot],&shot,owner,&arcade_c.wave.reservation,g_arcadeCombatDifficulty,&hit);
        s->active=shot.active;s->phase=shot.state;s->fireTimer=shot.phase;s->state=1;
        game_score_add(&g_game.players[owner],hit.points+hit.bonus_points);g_arcadeCombatDifficulty=hit.combat;
        snd_command(hit.sound);if(hit.bonus_sound)snd_command(hit.bonus_sound);
    }
}

static void d_shot_collisions(void)
{
    if(!arcade_d_contact_presence)return;
    u8 k,owner=wave_clock&1;
    for(k=0;k<2;k++)
    {
        Entity *s=&g_entities[16+owner*2+k];
        ArcadePlayerShot shot;ArcadeDHit hit;u8 slot;
        if(s->active!=1)continue;
        slot=arcade_d_shot_target(arcade_d.actors,(u16)s->x,(u8)(239-s->y),s->active);
        if(slot==255)continue;
        shot.active=s->active;shot.state=s->phase;shot.phase=s->fireTimer;
        arcade_d_hit(&arcade_d.actors[slot],&shot,owner,&arcade_d.wave.reservation,g_arcadeCombatDifficulty,&hit);
        s->active=shot.active;s->phase=shot.state;s->fireTimer=shot.phase;s->state=1;
        game_score_add(&g_game.players[owner],hit.points+hit.bonus_points);g_arcadeCombatDifficulty=hit.combat;
        snd_command(hit.sound);if(hit.bonus_sound)snd_command(hit.bonus_sound);
    }
}

static void e_shot_collisions(void)
{
    if(!arcade_e_contact_presence)return;
    u8 k,owner=wave_clock&1;
    for(k=0;k<2;k++)
    {
        Entity *s=&g_entities[16+owner*2+k];
        ArcadePlayerShot shot;u8 mask,slot;
        if(s->active!=1)continue;
        mask=arcade_e_shot_targets(arcade_e.actors,(u16)s->x,(u8)(239-s->y),s->active);
        if(!mask)continue;
        shot.active=s->active;shot.state=s->phase;shot.phase=s->fireTimer;
        for(slot=0;slot<2;slot++)if(mask&(1u<<slot))
        {
            ArcadeEHit hit;
            arcade_e_hit(&arcade_e.actors[slot],&shot,owner,g_arcadeCombatDifficulty,&hit);
            game_score_add(&g_game.players[owner],hit.points);g_arcadeCombatDifficulty=hit.combat;
            snd_command(hit.sound);
        }
        s->active=shot.active;s->phase=shot.state;s->fireTimer=shot.phase;s->state=1;
    }
}

static void f_shot_collisions(void)
{
    if(!arcade_f_contact_presence)return;
    u8 k,owner=wave_clock&1;
    for(k=0;k<2;k++)
    {
        Entity *s=&g_entities[16+owner*2+k];
        ArcadePlayerShot shot;ArcadeFHit hit;u8 slot;
        if(s->active!=1)continue;
        slot=arcade_f_shot_target(arcade_f.actors,(u16)s->x,(u8)(239-s->y),s->active);
        if(slot==255)continue;
        shot.active=s->active;shot.state=s->phase;shot.phase=s->fireTimer;
        arcade_f_hit(&arcade_f.actors[slot],&shot,owner,&arcade_f.wave.reservation,g_arcadeCombatDifficulty,&hit);
        s->active=shot.active;s->phase=shot.state;s->fireTimer=shot.phase;s->state=1;
        game_score_add(&g_game.players[owner],hit.points+hit.bonus_points);g_arcadeCombatDifficulty=hit.combat;
        snd_command(hit.sound);if(hit.bonus_sound)snd_command(hit.bonus_sound);
    }
}

/* H then G, and stable shot 0 then 1 within each family, as E2B3. */
static void gh_shot_collisions(void)
{
    Entity *shots[2]={0,0};
    u16 i;
    u8 pass,k,owner=wave_clock&1;
    /* AD10 owns exactly two slots per player in ordinary mode. Preserve
     * slot order and eligibility without searching unrelated actor records. */
    for(k=0;k<2;k++)
    {
        Entity *s=&g_entities[16+owner*2+k];
        if(s->active==1 && s->type==ET_SHOT && s->owner==owner)
            shots[s->fruit&1]=s;
    }
    /* E2D1 scans I before H and G, with shot 0 before shot 1. */
    for(k=0;k<2;k++)
    {
        Entity *s=shots[k];u8 slot;ArcadeGHKill hit;ArcadeIActor *a;Entity *e;
        if(!s || s->active!=1)continue;
        slot=arcade_i_shot_target(arcade_i_actors,(u16)s->x,(u8)(239-s->y));
        if(slot==255)continue;
        a=&arcade_i_actors[slot];e=&g_entities[20+slot];
        arcade_i_kill(&hit,g_arcadeCombatDifficulty,arcade_i_reservation,owner);
        arcade_i_reservation=hit.remaining;g_arcadeCombatDifficulty=hit.combat;
        a->body.active=e->active=2;a->body.state=4;a->body.phase=hit.phase;a->hit_style=owner;
        e->state=2;game_score_add(&g_game.players[owner],hit.points);
        snd_command(0x0c);if(hit.bonus)snd_command(0x0e);
        s->active=2;s->state=1;s->phase=2;s->fireTimer=0;
    }
    for(pass=0;pass<2;pass++)for(k=0;k<2;k++)
    {
        u8 family=(u8)(1-pass),slot;
        Entity *s=shots[k],*e;
        ArcadeFoe *a;
        ArcadeGHKill hit;
        if(!s || s->active!=1)continue;
        slot=arcade_gh_shot_target(arcade_foes,family,(u16)s->x,(u8)(239-s->y));
        if(slot==255)continue;
        i=(u16)(family?6:0)+slot;e=&g_entities[i];a=&arcade_foes[i];
        arcade_gh_kill(&hit,g_arcadeCombatDifficulty,formation_reservation[family],family,owner);
        formation_reservation[family]=hit.remaining;g_arcadeCombatDifficulty=hit.combat;
        a->body.active=e->active=2;a->body.state=6;a->body.phase=hit.phase;a->hit_style=owner;
        e->state=1;e->timer=ENEMY_DEATH_FRAMES;e->vx=0;
        game_score_add(&g_game.players[owner],hit.points);
        snd_command(0x0d);if(hit.bonus)snd_command(0x0e);
        s->active=2;s->state=1;s->phase=2;s->fireTimer=0;
    }
}

/* Dedicated source K/large scans after ordinary shot families. */
static u8 shared_k_target(u16 x,u8 y,u8 shot)
{
    u8 k;
    for(k=0;k<4;k++)
    {
        const ArcadeFoeWeapon *w=&arcade_weapons[k];
        const ArcadeKProjectile *a=&arcade_k_actors[k];
        u8 active=w->active?w->active:a->active;
        u16 px;
        u8 py;
        if(!active || (shot?active!=1:(active&2)))continue;
        if(shot && ((w->active?w->state:a->state)&128))continue;
        px=w->active?w->x:a->x;
        py=w->active?w->y:a->y;
        if(shot)
        {
            if((u8)(y-py+7)<=14 &&
               (u16)(x-px+9)<=18)return k;
        }
        else if((u8)(y-py+6)<=12 &&
                (u16)(x-px+6)<=12)return k;
    }
    return 255;
}
static void large_shot_collisions(void)
{
    u8 pass,k,owner=wave_clock&1;
    for(pass=0;pass<2;pass++)for(k=0;k<2;k++)
    {
        Entity *s=&g_entities[16+owner*2+k];ArcadePlayerShot shot;u8 slot;
        if(s->active!=1)continue;
        shot.active=s->active;shot.state=s->phase;shot.phase=s->fireTimer;
        if(!pass)
        {
            slot=shared_k_target((u16)s->x,(u8)(239-s->y),1);
            if(slot==255)continue;
            if(arcade_weapons[slot].active)
            {
                ArcadeFoeWeapon *a=&arcade_weapons[slot];
                a->active=2;a->state=3;a->phase=0;
                g_entities[12+slot].active=2;
                shot.active=2;shot.state=2;shot.phase=0;
            }
            else arcade_k_cancel(&arcade_k_actors[slot],&shot,owner);
            /* Source event0430: four-byte BCD value00000030 at4EAA. */
            game_score_add(&g_game.players[owner],30);
        }
        else
        {
            ArcadeLargeHit hit;
            slot=arcade_large_shot_target(arcade_large_actors,(u16)s->x,(u8)(239-s->y),s->active);
            if(slot==255)continue;
            arcade_large_hit(&arcade_large_actors[slot],&shot,owner,g_arcadeCombatDifficulty,&hit);
            g_arcadeCombatDifficulty=hit.combat;game_score_add(&g_game.players[owner],hit.points);snd_command(hit.sound);
        }
        s->active=shot.active;s->phase=shot.state;s->fireTimer=shot.phase;s->state=1;
    }
}

/* Collision pass: SHOT<->ENEMY (defeat + score) and PLAYER<->ENEMY/ITEM. Runs
 * after motion so positions are current this frame. O(N*M) over a 24-slot pool
 * = trivial on the 68000. */
static inline __attribute__((always_inline)) void collision_mode(const u8 survival)
{
    u16 i, j;
    const u16 shot_first=survival?0:(u16)(16+(wave_clock&1)*2);
    const u16 shot_end=survival?ENTITY_MAX:(u16)(shot_first+2);
    u8 pk,pass;
    /* E2BF..E2CB: source pools0828,0A28,0AE8,0B68,0BA8.
     * Each family scans both shots before advancing to the next family.
     * Legacy/direct-spawn types retain a fallback pass after I/H/G. */
    u8 heads[6]={ENTITY_MAX,ENTITY_MAX,ENTITY_MAX,ENTITY_MAX,ENTITY_MAX,ENTITY_MAX};
    u8 next[ENTITY_MAX-24];
    static const u8 family_rank[ET_POPUP+1]={
        5,5,5,5,4,0,5,5,1,3,5,5,2,5,5
    };
    /* Every ordinary shot handler requires active==1. Skip only their
     * side-effect-free empty scans; player contact still runs below. */
    if(!survival && g_entities[shot_first].active!=1 &&
       g_entities[shot_first+1].active!=1)goto player_contacts;
    if(!survival)
    {
        main_shot_collisions();
        c_shot_collisions();
        d_shot_collisions();
        e_shot_collisions();
        f_shot_collisions();
        /* Prepend in reverse slot order: stable family lists in one pool walk.
         * This avoids six full rescans for each of the player's shots. */
        for(i=legacy_pool_used?ENTITY_MAX:24;i>24;)
        {
            const Entity *en=&g_entities[--i];u8 rank;
            if(en->active!=1 || en->owner==128)continue;
            rank=en->type<=ET_POPUP?family_rank[en->type]:5;
            next[i-24]=heads[rank];heads[rank]=(u8)i;
        }
    }
    /* Native actors were handled above and in the shared G/H/I scan.
     * If no legacy slot has ever been allocated, there is no family-list work.
     * Native combat does not allocate the legacy pool during this interval. */
    if(!survival && !legacy_pool_used)gh_shot_collisions();
    else {
    for(pass=0;pass<(survival?1:6);pass++)
    {
    if(!survival && pass==5)gh_shot_collisions();

    /* SHOT vs ENEMY: a hit defeats the enemy and consumes the shot, awarding
     * score (arcade: a shot poking an enemy triggers its death sequence
     * sub_DD78). One loop handles every killable archetype -- the ground walker,
     * the flyer, and the thrower -- each with its own death-state index + score.
     * (The flyer/thrower use state 2 = dying; the walker uses state 1.) */
    for (i = shot_first; i < shot_end; i++)
    {
        Entity *s = &g_entities[i];
        if (s->type != ET_SHOT)continue;
        if(survival?(!s->active || s->state):(s->active!=1))continue;
        /* Original $E2B3 selects P1 on even phases, P2 on odd phases. */
        if(!survival && s->owner!=(wave_clock&1))continue;

        /* Slots 0..11 were handled above; 12..15 are enemy weapons, not targets. */
        for (j=survival?0:heads[pass]; j<ENTITY_MAX;
             j=survival?j+1:next[j-24])
        {
            Entity *en = &g_entities[j];
            u8  deathState;
            u32 award;
            /* Reject empty/reserved slots before decoding an enemy type. This
             * scan repeats per player shot; the type tests have no side effects. */
            if(en->active!=1 || en->owner==128)continue;

            /* Same type metadata and scan order as the former comparison chain. */
            {
                static const u8 death_states[ET_POPUP+1]={
                    [ET_ENEMY_A]=1,[ET_FLYER]=2,[ET_THROWER]=2,[ET_CREATURE_G]=1,
                    [ET_CREATURE_C]=1,[ET_COMPOSITE_E]=1,[ET_HOPPER]=2,
                    [ET_LARGE]=1,[ET_EMITTER]=1,[ET_ORB]=1
                };
                static const u16 awards[ET_POPUP+1]={
                    [ET_ENEMY_A]=SCORE_ENEMY,[ET_FLYER]=SCORE_FLYER,
                    [ET_THROWER]=SCORE_THROWER,[ET_CREATURE_G]=SCORE_CREATURE_G,
                    [ET_CREATURE_C]=SCORE_CREATURE_C,[ET_COMPOSITE_E]=SCORE_COMPOSITE_E,
                    [ET_HOPPER]=SCORE_HOPPER,[ET_LARGE]=SCORE_LARGE,
                    [ET_EMITTER]=SCORE_EMITTER,[ET_ORB]=SCORE_ORB
                };
                u16 type=en->type;
                if(type>ET_POPUP)continue;
                deathState=death_states[type];
                if(!deathState)continue;
                award=awards[type];
            }

            if (en->state == deathState) continue;   /* dead/dying */

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
                    if (en->type == ET_LARGE) en->hp = 0; /* $EEBE: 1 -> 0 */
                    en->state = deathState;        /* enter death sequence      */
                    en->timer = ENEMY_DEATH_FRAMES;
                    en->vx    = 0;
                    /* credit the player whose shot scored the kill (co-op). */
                    game_score_add(&g_game.players[s->owner & 1],award);
                    /* NOTE: the arcade shows a score child on enemy kills too
                     * (classK $0ED8), but we DON'T pop a "+N" per kill here -- in a
                     * busy fire-fight that would spawn many concurrent popup-glyph
                     * sprites and shave the frame budget. The iconic SonSon popup is
                     * the FRUIT one (low-frequency), which we DO show (see ET_ITEM
                     * collect below). Enemy kills still score + play the defeat SFX. */
                    snd_sfx_enemy();
                }
                if(!survival){s->active=2;s->phase=2;s->fireTimer=0;}
                s->state=1;s->timer=5; /* Impact retains its slot through the end puff. */
                if(survival)anim_restart(&s->av,SS_ANIM_SHOT_END);
                break;
            }
        }
    }

    } /* source shot-family order */
    }

player_contacts:
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
        if(!survival && pk!=(wave_clock&1))continue;
        if(!survival)game_arcade_food_contact(pk); /* E2E0/E334. */
        if(!survival)
        {
            u8 k;
            /* E2E3/E337: both stable shot slots scan the shared background pool. */
            for(k=0;k<2;k++)
            {
                Entity *s=&g_entities[16+pk*2+k];ArcadePlayerShot shot;
                if(s->active!=1 || s->type!=ET_SHOT || s->owner!=pk)continue;
                shot.x=(u16)s->x;shot.y=(u8)(239-s->y);shot.active=s->active;
                shot.state=s->phase;shot.phase=s->fireTimer;
                game_score_add(p,arcade_food_world_shot(&shot));
                s->active=shot.active;s->phase=shot.state;s->fireTimer=shot.phase;
                if(shot.active!=1)s->state=1;
            }
            large_shot_collisions();
        }
        /* Food/background displacement is complete. These five families only
         * mutate actor/award state and deadReq/hurt, not player lifecycle.
         * Keep death requests live between families; snapshot eligibility once. */
        if(!survival && p->flow==FLOW_ALIVE && !p->invuln && !p->deadReq)
        {
            const u8 contact_active=(u8)(game_player_target_flags(pk)>>8);
            /* ECB7: main pool contacts precede K and the formation families. */
            if(!p->deadReq && arcade_main_contact_presence)
            {
                u8 slot=arcade_main_contact_target(arcade_main.actors,(u16)p->x,
                    (u8)(239-p->y),contact_active);
                if(slot!=255)
                {
                    ArcadeMainActor *a=&arcade_main.actors[slot];
                    if(a->active&64)
                    {
                        ArcadeBonusAward award;
                        if(arcade_main_bonus_collect(a,pk,g_arcadeCombatDifficulty,&award))
                        {game_score_add(p,award.points);g_arcadeCombatDifficulty=award.combat;snd_command(award.sound);}
                    }
                    else {p->deadReq=1;if(!p->hurt)p->hurt=HURT_FRAMES;}
                }
            }
            if(!p->deadReq && arcade_c_contact_presence)
            {
                u8 slot=arcade_c_contact_target(arcade_c.actors,(u16)p->x,
                    (u8)(239-p->y),contact_active);
                if(slot!=255)
                {
                    ArcadeCActor *a=&arcade_c.actors[slot];
                    if(a->active&64)
                    {
                        ArcadeBonusAward award;
                        if(arcade_c_bonus_collect(a,pk,g_arcadeCombatDifficulty,&award))
                        {game_score_add(p,award.points);g_arcadeCombatDifficulty=award.combat;snd_command(award.sound);}
                    }
                    else {p->deadReq=1;if(!p->hurt)p->hurt=HURT_FRAMES;}
                }
            }
            if(!p->deadReq && arcade_d_contact_presence)
            {
                u8 slot=arcade_d_contact_target(arcade_d.actors,(u16)p->x,
                    (u8)(239-p->y),contact_active);
                if(slot!=255)
                {
                    ArcadeDActor *a=&arcade_d.actors[slot];
                    if(a->active&64)
                    {
                        ArcadeBonusAward award;
                        if(arcade_d_bonus_collect(a,pk,g_arcadeCombatDifficulty,&award))
                        {game_score_add(p,award.points);g_arcadeCombatDifficulty=award.combat;snd_command(award.sound);}
                    }
                    else {p->deadReq=1;if(!p->hurt)p->hurt=HURT_FRAMES;}
                }
            }
            if(!p->deadReq && arcade_e_contact_presence)
            {
                u8 slot=arcade_e_contact_target(arcade_e.actors,(u16)p->x,
                    (u8)(239-p->y),contact_active);
                if(slot!=255)
                {
                    ArcadeEActor *a=&arcade_e.actors[slot];
                    if(a->active&64)
                    {
                        ArcadeBonusAward award;
                        if(arcade_e_bonus_collect(a,pk,g_arcadeCombatDifficulty,&award))
                        {game_score_add(p,award.points);g_arcadeCombatDifficulty=award.combat;snd_command(award.sound);}
                    }
                    else {p->deadReq=1;if(!p->hurt)p->hurt=HURT_FRAMES;}
                }
            }
            if(!p->deadReq && arcade_f_contact_presence)
            {
                u8 slot=arcade_f_contact_target(arcade_f.actors,(u16)p->x,
                    (u8)(239-p->y),contact_active);
                if(slot!=255)
                {
                    ArcadeFActor *a=&arcade_f.actors[slot];
                    if(a->active&64)
                    {
                        ArcadeBonusAward award;
                        if(arcade_f_bonus_collect(a,pk,g_arcadeCombatDifficulty,&award))
                        {game_score_add(p,award.points);g_arcadeCombatDifficulty=award.combat;snd_command(award.sound);}
                    }
                    else {p->deadReq=1;if(!p->hurt)p->hurt=HURT_FRAMES;}
                }
            }
        }
        /* E2F8 precedes E2FE/E301/E304: consume a touching K projectile
         * before an overlapping H/G/I body can raise the death request. */
        if(!survival && p->flow==FLOW_ALIVE && !p->invuln && !p->deadReq)
        {
            u8 slot=shared_k_target((u16)p->x,(u8)(239-p->y),0);
            if(slot!=255)
            {
                if(arcade_weapons[slot].active)
                {arcade_weapons[slot].state=3;arcade_weapons[slot].phase=2;}
                else
                {arcade_k_actors[slot].state=3;arcade_k_actors[slot].phase=2;}
                p->deadReq=1;
                if(!p->hurt)p->hurt=HURT_FRAMES;
            }
        }
        if(!survival)game_arcade_special_contact(pk); /* E2FB/E34F, after K. */
        if(!survival && p->flow==FLOW_ALIVE && !p->invuln && !p->deadReq)
        {
            /* E2FE/E301/E304 visit H6..11, G0..5, then I20..23. */
            static const u8 contact_order[16]={6,7,8,9,10,11,0,1,2,3,4,5,20,21,22,23};
            const u16 px=(u16)p->x;const u8 py=(u8)(239-p->y);
            u8 k;
            /* E2FE/E301/E304: H, G, I. Ordinary contact does not test AI state. */
            for(k=0;k<16;k++)
            {
                u16 index=contact_order[k];
                Entity *en=&g_entities[index];
                const ArcadeFormationActor *body;
                if(!en->active || (en->active&2))continue;
                body=index<12?&arcade_foes[index].body:&arcade_i_actors[index-20].body;
                if(en->active&64)
                {
                    if(index<12 && arcade_body_contact(px,py,
                        body->x,body->y,1,1))
                    {
                        ArcadeBonusAward award;
                        if(arcade_gh_bonus_collect(&arcade_foes[index],pk,g_arcadeCombatDifficulty,&award))
                        {
                            en->active=2;game_score_add(p,award.points);
                            g_arcadeCombatDifficulty=award.combat;snd_command(award.sound);
                            /* ECE9 returns after one hit, then the dispatcher
                             * advances to the next family, not the next slot. */
                            k=k<6?5:11;
                        }
                    }
                    continue;
                }
                if(arcade_body_contact(px,py,
                     body->x,body->y,1,en->active))
                {
                    p->deadReq=1;
                    if(!p->hurt)p->hurt=HURT_FRAMES;
                    break;
                }
            }
        }

        if(!survival && p->flow==FLOW_ALIVE && !p->invuln && !p->deadReq)
        {
            if(arcade_large_contact_target(arcade_large_actors,(u16)p->x,(u8)(239-p->y),1)!=255)p->deadReq=1;
            if(p->deadReq && !p->hurt)p->hurt=HURT_FRAMES;
        }

    /* Native slots16..19 contain only player shots (no fallback contact case),
     * and20..23 are excluded below. No fallback allocation means no possible
     * side effects in this traversal. Recheck once per contacted player; the
     * monotonic latch remains set after any direct allocation until reset. */
    if(survival || legacy_pool_used)
    /* Ordinary slots0..15 were unconditionally excluded below. */
    for (j = survival?0:16; j < ENTITY_MAX; j++)
    {
        Entity *en = &g_entities[j];
        if(!survival && (j<16 || (j>=20 && j<24)))continue;
        if (en->active != 1 || en->owner==128) continue;

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
                    if (lethal_contact(p, en))
                    {
                        en->active=0;
                        if(!survival && j>=12 && j<16)
                            arcade_weapons[j-12].active=0;
                    }  /* spent on a hit */
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
                    game_score_add(p,val);                 /* credit the collector (co-op) */
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
static inline __attribute__((always_inline)) void entity_update_mode(const u8 survival)
{
    u16 i;
    u8  pk;

    /* decay each active player's hurt-flash window (per-player). */
    for (pk = 0; pk < g_game.numPlayers; pk++)
        if (g_game.players[pk].hurt) g_game.players[pk].hurt--;

    if(!survival)
    {
        /* Native pool updates below cannot mutate player lifecycle flags. */
        const u16 target_flags[2]={game_player_target_flags(0),game_player_target_flags(1)};
        arcade_world.camera=(u16)g_game.cameraX;arcade_world.clock=wave_clock;
        arcade_world.stage=(u8)(g_arcadeCombatDifficulty>>8);arcade_world.tracking=g_game.numPlayers>1;
        arcade_world.scroll_stopped=game_arcade_stopped();arcade_world.purge=arcade_food_conversion_pulse;
        arcade_world.shot_budget=arcade_shot_budget;
        arcade_main_pool_update(&arcade_main,arcade_world.camera,wave_clock,
            arcade_world.scroll_stopped,arcade_world.purge,&arcade_bonus_next,arcade_bonus_limit);
        arcade_c_pool_update(&arcade_c,arcade_world.camera,arcade_world.scroll_stopped,
            arcade_world.purge,&arcade_bonus_next,arcade_bonus_limit);
        {
            u8 target=arcade_tracking_player(g_game.numPlayers>1,
                (u16)g_game.players[0].x,target_flags[0],
                (u16)g_game.players[1].x,target_flags[1]);
            const Player *p=&g_game.players[target];
            arcade_d_pool_update(&arcade_d,arcade_world.camera,wave_clock,
                (u8)(g_arcadeCombatDifficulty>>8),arcade_world.scroll_stopped,arcade_world.purge,
                (u16)p->x,(u8)(239-p->y),target,&arcade_bonus_next,arcade_bonus_limit);
            {
                u16 flags=target_flags[target];
                u8 eligible=(u8)(((flags>>8)&~flags)&1);
                arcade_e_pool_update(&arcade_e,arcade_world.camera,wave_clock,
                    arcade_world.scroll_stopped,arcade_world.purge,(u16)p->x,(u8)(239-p->y),
                    target,eligible,&arcade_main_ready,&arcade_bonus_next,arcade_bonus_limit);
                arcade_shot_budget=(u8)(arcade_main_ready!=0);
                arcade_world.shot_budget=arcade_shot_budget;
            }
        }
        arcade_f_pool_update(&arcade_f,arcade_world.camera,arcade_world.scroll_stopped,
            arcade_world.purge,&arcade_bonus_next,arcade_bonus_limit);
        for(pk=0;pk<2;pk++)
        {
            const Player *p=&g_game.players[pk];
            ArcadeFoePlayer *q=&arcade_world.players[pk];
            u16 flags=target_flags[pk];
            q->x=(u16)p->x;q->y=(u8)(239-p->y);
            q->active=(u8)(flags>>8);q->kind=(u8)flags;
        }
    }

    /* Enemy AI tracks a LIVE player: P1 normally, P2 if P1 is out (co-op). (The
     * arcade tracks the player(s); nearest-of-two targeting is a later refinement.) */
    {
        const Player *track = &g_game.players[0];
        if (g_game.numPlayers > 1 && g_game.players[0].flow == FLOW_GAME_OVER)
            track = &g_game.players[1];

        if(!survival)
        {
            /* Preserve source slot order while removing family dispatch from
             * every occupied slot. Allocations into later slots remain visible. */
            for(i=0;i<12;i++)if(g_entities[i].active)
                native_foe_update(&g_entities[i],i);
            for(i=12;i<16;i++)
            {
                Entity *e=&g_entities[i];
                ArcadeFoeWeapon *a=&arcade_weapons[i-12];
                const ArcadeFoeFrame *pose;
                if(!a->active){e->active=0;continue;}
                if(a->active && !e->active)
                    e=entity_spawn_range(ET_ENEMY_SHOT,(s16)a->x,(s16)(239-a->y),i,i+1);
                arcade_foe_weapon_step(a,&arcade_world,arcade_foes);
                arcade_k_actors[i-12].xf=a->xf;arcade_k_actors[i-12].yf=a->yf;
                if(e)
                {
                    e->active=a->active;e->x=(s16)a->x;e->xSub=a->xf;
                    e->y=(s16)(u8)(239-a->y);if(e->y>=240)e->y-=256;
                    e->ySub=a->yf;e->state=a->state==3?1:0;
                    pose=&arcade_foe_frames[a->pose];e->tile=pose->tile;
                    e->facing=(u8)((pose->attr&64)!=0);
                }
            }
            for(i=16;i<20;i++)if(g_entities[i].active)
                entity_update_one(&g_entities[i],track,0);
            {
                u8 k;for(k=0;k<4;k++)
                {
                    ArcadeKProjectile *a=&arcade_k_actors[k];
                    if(!a->active)continue;
                    arcade_k_projectile_step(a,arcade_world.camera,
                        arcade_world.scroll_stopped,arcade_world.purge);
                    /* Shared source bytes7/9 survive cleanup and kind changes. */
                    arcade_weapons[k].xf=a->xf;arcade_weapons[k].yf=a->yf;
                }
            }
            for(i=20;i<24;i++)if(g_entities[i].active)
                native_i_update(&g_entities[i],i);
            /* Native updates above can allocate this tail during the tick. */
            if(legacy_pool_used)
                for(i=24;i<ENTITY_MAX;i++)if(g_entities[i].active)
                    entity_update_one(&g_entities[i],track,0);
        }
        else
        {
            for(i=0;i<ENTITY_MAX;i++)if(g_entities[i].active)
                entity_update_one(&g_entities[i],track,1);
        }

    }

    __asm__ volatile(".globl profile_large_update_begin_%=\nprofile_large_update_begin_%=:" ::: "memory");
    if(!survival)
    {
        u8 k,j;
        for(k=0;k<3;k++)
        {
            u8 occupied[4];ArcadeLargeShotBirth birth;
            if(!arcade_large_actors[k].body.active)continue;
            for(j=0;j<4;j++)occupied[j]=(u8)(arcade_k_actors[j].active || arcade_weapons[j].active);
            if(arcade_large_step(&arcade_large_actors[k],&arcade_world,
                (u8)(g_arcadeCombatDifficulty>>8),&arcade_rng.seed,&arcade_rng.last,occupied,&birth))
            {
                ArcadeKProjectile *a=&arcade_k_actors[birth.slot];
                a->active=1;a->state=a->phase=0;a->x=birth.x;a->y=birth.y;a->facing=birth.facing;
            }
        }
    }
    __asm__ volatile(".globl profile_collision_begin_%=\nprofile_collision_begin_%=:" ::: "memory");
    if(!survival)
    {
        arcade_food_carrier_step((u16)g_game.cameraX); /* A61C */
        arcade_food_conversion_pulse=0; /* A61F before E2B3 contacts. */
    }
    collision_mode(survival);
    __asm__ volatile(".globl profile_schedule_begin_%=\nprofile_schedule_begin_%=:" ::: "memory");
    entity_schedule_tick();
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

static inline __attribute__((always_inline)) u16 entity_render_mode(u16 *sat, u16 firstSlot, const u8 survival)
{
    u16 count  = 0;            /* entity BODY sprites (capped at ENTITY_SAT_MAX) */
    u16 glyphs = 0;            /* popup glyph sprites (capped at POPUP_GLYPH_MAX)*/
    u16 slot   = firstSlot;
    u16 i,visit;u8 players_written=0;
    if(!survival)
    {
        entity_sprite_admission();
        slot+=arcade_food_bonus_sat((u16)g_game.cameraX,sat,slot,
            MAX_PLAYERS+ENTITY_SAT_MAX+ENTITY_POPUP_GLYPH_MAX);
        count=(u16)(slot-firstSlot); /* Wrapped cells share the body budget. */
        /* Hidden/inactive actors cannot request dynamic art. Empty requests
         * leave both upload residency and ready-cell identities unchanged. */
        if((arcade_e.actors[0].active && arcade_e.actors[0].active<128) ||
           (arcade_e.actors[1].active && arcade_e.actors[1].active<128))
        {
            u16 cells[2]={65535,65535};u8 k;
            for(k=0;k<arcade_admission.count;k++)
            {
                u8 id=arcade_admission.order[k];
                if(id>=74 && id<76)cells[id-74]=arcade_e_render_request(&arcade_e.actors[id-74]);
            }
            if(arcade_e_upload(cells,704,2))
            {
                for(k=0;k<2;k++)if(cells[k]!=65535)arcade_e_ready[k]=cells[k];
            }
            else arcade_e_ready[0]=arcade_e_ready[1]=65535;
        }
    }

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
    for (visit=0;visit<(survival?ENTITY_MAX:arcade_admission.count);visit++)
    {
        Entity *e;
        s16 sx, sy;u16 body_tile;u8 body_facing;
        i=survival?visit:arcade_admission.order[visit];
        if(!survival && !players_written && (i<16 || i>=20))
        {
            slot+=game_render_players(sat,slot,MAX_PLAYERS+ENTITY_SAT_MAX+ENTITY_POPUP_GLYPH_MAX);
            players_written=1;
        }
        if(!survival && i>=12 && i<16)continue; /* Shared K follows large. */
        if(!survival && i>=76)
        {
            u16 n=arcade_f_render_actor(&arcade_f.actors[i-76],
                (u16)g_game.cameraX,sat,slot,firstSlot+ENTITY_SAT_MAX);
            slot+=n;count+=n;continue;
        }
        if(!survival && i>=74)
        {
            u16 n=arcade_e_render_actor(&arcade_e.actors[i-74],(u8)(i-74),arcade_e_ready[i-74],
                (u16)g_game.cameraX,sat,slot,firstSlot+ENTITY_SAT_MAX);
            slot+=n;count+=n;continue;
        }
        if(!survival && i>=70)
        {
            u16 n=arcade_d_render_actor(&arcade_d.actors[i-70],
                (u16)g_game.cameraX,sat,slot,firstSlot+ENTITY_SAT_MAX);
            slot+=n;count+=n;continue;
        }
        if(!survival && i>=64)
        {
            u16 n=arcade_c_render_actor(&arcade_c.actors[i-64],
                (u16)g_game.cameraX,sat,slot,firstSlot+ENTITY_SAT_MAX);
            slot+=n;count+=n;continue;
        }
        if(!survival && i>=48)
        {
            u16 n=arcade_main_render_actor(&arcade_main.actors[i-48],
                (u16)g_game.cameraX,sat,slot,firstSlot+ENTITY_SAT_MAX);
            slot+=n;count+=n;continue;
        }
        e=&g_entities[i];
        /* Source active=2 remains visible throughout the hit/death sequence. */
        if(!e->active)continue;
        if(e->active>2 && (survival || i>=12 ||
           !(arcade_foes[i].pose&ARCADE_BONUS_POSE) || (e->active&128)))continue;

        /* Native actors own position and pose. Materialize the view only when
         * a sprite is actually drawn, not after every simulation update. */
        if(!survival && i<12) {
            const ArcadeFoe *a=&arcade_foes[i];const ArcadeFoeFrame *pose;
            sx=(s16)(a->body.x-g_game.cameraX);
            sy=(u8)(239-a->body.y);if(sy>=240)sy-=256;
        if(a->pose&ARCADE_BONUS_POSE)
        {
            const ArcadeBonusFrame *bonus=&arcade_bonus_frames[a->pose&0x3fff];
            u8 alternate=(u8)((a->pose&ARCADE_BONUS_OWNER2)!=0);
            body_tile=alternate?bonus->owner2_tile:bonus->tile;
            body_facing=(u8)(((alternate?0xc3:bonus->attr)&64)!=0);
        }
        else
        {
            pose=&arcade_foe_frames[a->pose];body_tile=pose->tile;
            body_facing=(u8)((pose->attr&64)!=0);
        }
        } else if(!survival && i>=20 && i<24) {
            const ArcadeIActor *a=&arcade_i_actors[i-20];
            sx=(s16)(a->body.x-g_game.cameraX);
            sy=(u8)(239-a->body.y);if(sy>=240)sy-=256;
            body_tile=arcade_i_tile(a->code,a->attr);body_facing=(a->attr&64)!=0;
        } else {
            sx=(s16)(e->x-g_game.cameraX);sy=e->y;
            body_tile=e->tile;body_facing=e->facing;
        }

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
        sat[slot * 4 + 2] = (u16)TILE_ATTR_FULL(PAL1, 0, 0, (u8)((!survival && e->type==ET_SHOT)?0:!body_facing), body_tile);
        sat[slot * 4 + 3] = (u16)((0x80 + sx) & 0x1FF);

        slot++;
        count++;
    }

    if(!survival)
    {
        ArcadeLargeCell cells[16];u16 visible=0,n,k,j;
        u16 resident_tiles[4]={65535,65535,65535,65535};
        u8 remaining,resident_mask=0;
        if(!players_written)slot+=game_render_players(sat,slot,
            MAX_PLAYERS+ENTITY_SAT_MAX+ENTITY_POPUP_GLYPH_MAX);
        /* Existing world-to-screen convention maps source Y to Y-1.
         * Whole-game viewport parity remains a separate integration concern. */
        static const ArcadeSpriteView view={0,1,256,224};
        for(k=0;k<3;k++)if(arcade_admission.large&(1u<<k))
        {
            remaining=4;
            if(arcade_large_cells(&arcade_large_actors[k],(u16)g_game.cameraX,
                &remaining,cells+k*4))visible|=(u16)(15u<<(k*4));
        }
        for(k=0;k<4;k++)
        {
            const ArcadeKProjectile *a=&arcade_k_actors[k];
            if(arcade_admission.children&(1u<<k))
            {
                ArcadeLargeCell *c=&cells[12+k];
                const ArcadeFoeWeapon *w=&arcade_weapons[k];
                if(w->active)
                {
                    const ArcadeFoeFrame *f=&arcade_foe_frames[w->pose];
                    c->x=(u8)(w->x-g_game.cameraX);c->y=(u8)(-w->y-16);
                    c->code=f->code;c->attr=f->attr;
                    resident_tiles[k]=f->tile;resident_mask|=(u8)(1u<<k);
                }
                else
                {c->x=(u8)(a->x-g_game.cameraX);c->y=(u8)(-a->y-16);c->code=a->code;c->attr=a->attr;}
                visible|=(u16)(1u<<(12+k));
            }
        }
        n=arcade_large_present_mixed(cells,visible,resident_tiles,&view,sat,slot,
            MAX_PLAYERS+ENTITY_SAT_MAX+ENTITY_POPUP_GLYPH_MAX,512,1);
        if(n==65535)
        {
            n=(visible==large_last_visible && resident_mask==large_last_resident)?large_last_count:0;
            if(slot+n>MAX_PLAYERS+ENTITY_SAT_MAX+ENTITY_POPUP_GLYPH_MAX)n=0;
            memcpy(sat+slot*4,large_last_sat,(u16)(n*8));
        }
        else
        {
            large_last_count=n;large_last_visible=visible;large_last_resident=resident_mask;
            memcpy(large_last_sat,sat+slot*4,(u16)(n*8));
        }
        if(n && slot>firstSlot)sat[(slot-1)*4+1]=(u16)((sat[(slot-1)*4+1]&0xff00)|slot);
        for(k=0;k<n;k++)sat[(slot+k)*4+1]=(u16)(0x500|(slot+k+1));
        slot+=n;
    }

    /* terminate the chain: the last appended sprite links to 0 */
    if (slot != firstSlot) sat[(slot - 1) * 4 + 1] =
        (u16)((sat[(slot - 1) * 4 + 1] & 0xFF00) | 0);

    return (u16)(slot - firstSlot);   /* total sprites appended (bodies + glyphs) */
}

/* Select once so the compiler removes the other mode's per-actor branches. */
u16 entity_render_all(u16 *sat,u16 firstSlot)
{
    if(g_survivalRun)return entity_render_mode(sat,firstSlot,1);
    return entity_render_mode(sat,firstSlot,0);
}

void entity_update_all(void){
    if(g_survivalRun)entity_update_mode(1);
    else entity_update_mode(0);
}

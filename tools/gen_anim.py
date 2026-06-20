#!/usr/bin/env python3
"""gen_anim.py -- bake the SonSon PLAYER + ENEMY animation frames into native
Genesis 4bpp sprite tiles + an animation table, for the native engine's
faithful per-actor animation (replacing the single static frames).

REUSES gen_assets.py's verified, byte-faithful sprite bake (bake_sprite_tiles,
proven byte-identical to the port's build_sprite by verify_bake.py) and the
verbatim arcade sprite palette (PAL1). Each animation frame is an arcade
(sprite_code, color_bank) pair -> 4 column-major 8x8 Genesis tiles. Frames are
deduped by baked-byte equality across ALL anims (a frame shared by two anims
costs VRAM once); each anim references frames by their baked tile-base index.

PROVENANCE OF THE FRAME SEQUENCES (authoritative = the arcade):
  Captured EMPIRICALLY from the live arcade (MAME `sonson`, NTSC) attract-demo
  gameplay by logging the per-frame spriteram (D:/portForge/tools/mame/
  ss_anim_cap.lua -> ss_anim_cap_raw.txt) and clustering active sprites into
  actor tracks (tools/analyze_anim.py / analyze_anim2.py). Cross-checked against
  the ROM anim-script tables read live (tools dump $DDE1 enemy walk, $B327
  player table) and the verified stepper sub_9617.c (../sonson/port/src):
  a script frame = [sprite_code, sprite_attr] + a control byte (bit7=jump,
  low7=frame-duration). Codes/banks below are the exact spriteram values the
  arcade emitted.

  PLAYER (the SonSon hero). Two on-screen palettes appear in the demo (P1/P2):
  bank 0 codes {64,65,80 run; 66..69 climb; 83 action} (the RED hero -- verified
  the canonical look in tools/preview_anim.py) and bank 1 the same shapes in the
  alt palette ({72,73,81; 74..77; 84}, == bank-0 codes + 8). We use BANK 0.
  Empirically:
    RUN   : 64 -> 65 -> 80 -> 65   (~4-5 frames/cell; classic 4-step contact cycle)
    CLIMB : 66 -> 67 -> 68 -> 69   (~3 frames/cell; ladder climb -- observed as a
            clean 4-cycle exactly while the actor's Y stepped between lanes)
    FIRE  : 83 (distinct crouch/action pose, observed held at a fixed lane) then
            back to the run-pass frame -- a short 2-cell shoot stab.
    DEATH : RE-EXTRACTED from real on-camera deaths (tools/ss_pdeath_cap.lua +
            ss_pdeath_all.lua, 6 forced deaths, tools/death_anim_cap.json). The
            arcade hero has NO on-screen tumble: on a lethal hit the player sprite
            VANISHES instantly (~4 frames), then RESPAWNS holding sprite code 83
            (the standby/arms pose, ~17f) before resuming the run. The all-sprite
            dump confirms no death-specific sprite of ANY code/bank plays during
            the gap (sheet codes 74-77 are scrambled garbage, 88-90 decorative).
            So faithful PLAYER_DEATH = HOLD pose 83 (what the arcade shows at the
            death/respawn beat); the engine already blinks the hero during DYING.

  ENEMY (the bank-2 ground enemy, the most common chaser in the demo):
    WALK  : 304 -> 305 -> 306 -> 305   (0x130..0x132; ~4-7 frames/cell)
    DEATH : RE-EXTRACTED from real shot-kills (tools/ss_edeath_cap.lua +
            ss_edeath2.lua, 32 walker death-tails, tools/death_anim_cap.json):
            307 -> 308 -> 309 (the stagger/recoil, 2f each) -> 310 (the knocked-
            down corpse, held ~17-34f, median ~23) -> 312 -> 313 -> 314 -> 315
            (the white-debris explosion-scatter that fades the slot out, ~4/8/8/8f).
            This is the FULL faithful collapse+explosion (the old bake stopped at
            310 with a short 9f hold).

  TWO MORE ARCHETYPES (the native-enrichment task). The attract DEMO only ever
  exercises the ground walker, so the flyer (classF $DF18) + thrower (enemyMain
  $BAB3) + lobbed projectile (classI $D352) never appear in ss_anim_cap_raw.txt.
  Their FRAMES are therefore taken directly from the authentic arcade SPRITE
  SHEET (bank 2, the enemy palette -- the same sheet the walker lives on; codes
  eyeball-verified distinct + non-blank via tools/cand2.png), and the AI is the
  faithful behavioural RE (see entity.c headers + ../sonson/port/src):
    FLYER_FLY  : 296 -> 297  (the orange spread-arm flyer; 2-frame wing-flap,
                 a clean glide-flap loop -- distinct from the red walker)
    FLYER_DEATH: 110..119 (bank 9) -- the GREEN flyer's OWN distinct death burst:
                 its body breaks apart with YELLOW SPARKS + scattering debris,
                 visually different from the walker's white-armor collapse. The
                 frame ORDER is the authoritative arcade sprite sheet (bank 9,
                 sheet_browse.py); headless AI could not reliably shoot an evasive
                 gliding flyer so the live cadence wasn't captured -- the per-frame
                 durations are modeled on the captured walker-burst timing (see
                 tools/death_anim_cap.json + the TODO there).
    THROWER_*  : 320 -> 321  (the blue/purple humanoid; THROWER_IDLE is the
                 standing/approach cell pair, THROWER_THROW a brief 321 lunge
                 on the lob -- a short non-looping stab like the player FIRE)
    ESHOT      : 70 -> 71    (the small orange fireball; 2-frame flicker -- the
                 lobbed projectile / enemy shot, tiny + round)
  These drive the SAME native anim runtime (anim_tick / sub_9617 cadence) so the
  flyer flaps, the thrower's throw plays, and the fireball flickers in flight.

DEATH-CAPTURE STATUS (tools/death_anim_cap.json, MAME `sonson` NTSC):
  CAPTURED LIVE: player death (6 forced deaths -> the hero VANISHES then holds pose
    83, no tumble) + walker death (32 shot-kills -> 307,308,309 stagger / 310 corpse
    ~22f / 312-315 explosion). Both baked faithfully + verified stepping in-game.
  SHEET-VERIFIED (frames), cadence MODELED: flyer death (bank 9 codes 110..119 burst
    -- headless AI couldn't reliably shoot an evasive gliding flyer, so the per-frame
    durations are modeled on the captured walker burst).
  TODO(re-capture): (1) the live flyer-death CADENCE (intercept a flyer); (2) a
    distinct death for the bank-15 fast-dive creature (183-185) -- it evaded shots
    headless, so it falls back to the shared walker burst (SHARED_BURST). All other
    white-armored archetypes (thrower/creatures/composite/hopper/large/emitter/orb)
    GENUINELY reuse the shared walker burst in the arcade -> SHARED_BURST is faithful.

DETERMINISTIC, Python 3 stdlib only. Emits src/ss_anim.c + inc/ss_anim.h.

USAGE:
    python gen_anim.py [--arcade DIR] [--base N] [--out-c P] [--out-h P]
  base = VRAM tile index to load the anim tiles at (default 346 = right after
  ss_gfx scene tiles [0..135) + ss_level tiles [135..346)). The emitted anim
  table tile indices are ABSOLUTE (already include base).
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_assets as G          # reuse Roms + bake_sprite_tiles (verified)

# ---------------------------------------------------------------------------
# The animation definitions. Each frame = (code, bank, dur_frames). `code` and
# `bank` are the exact arcade spriteram values (the bake key); dur_frames is the
# on-screen hold (cadence) at 60fps, from the capture (rounded to the observed
# median hold). Order is the on-screen frame ORDER. Anims LOOP (the engine wraps
# the cursor) except where noted (death holds the last frame).
#
# Player hero = color bank 1 (the P1 monkey palette). Enemy = bank 2.
# ---------------------------------------------------------------------------
PLAYER_BANK = 0
ENEMY_BANK  = 2
FLYER_BANK  = 9     # the green flyer's color bank (its death burst 110..119)

# SHARED walker-style death burst (the FAITHFUL collapse+explosion captured from
# 32 real shot-kills -- see header + death_anim_cap.json). Every white-armored
# enemy archetype (thrower / creatures / composite / hopper / large / emitter /
# orb) reuses this in the arcade, so they all reference it here (deduped by baked
# bytes -> the 8 frames cost VRAM ONCE). 307-309 stagger (2f), 310 corpse (~22f),
# 312-315 the debris-scatter that fades the slot out (4/8/8/8f).
SHARED_BURST = [(307, ENEMY_BANK, 2), (308, ENEMY_BANK, 2),
                (309, ENEMY_BANK, 2), (310, ENEMY_BANK, 22),
                (312, ENEMY_BANK, 4), (313, ENEMY_BANK, 8),
                (314, ENEMY_BANK, 8), (315, ENEMY_BANK, 8)]

ANIMS = [
    # name,            loop, [ (code, bank, dur), ... ]
    ("PLAYER_RUN",   True,  [(64, PLAYER_BANK, 5), (65, PLAYER_BANK, 5),
                             (80, PLAYER_BANK, 5), (65, PLAYER_BANK, 5)]),
    ("PLAYER_CLIMB", True,  [(66, PLAYER_BANK, 4), (67, PLAYER_BANK, 4),
                             (68, PLAYER_BANK, 4), (69, PLAYER_BANK, 4)]),
    ("PLAYER_FIRE",  False, [(83, PLAYER_BANK, 6), (65, PLAYER_BANK, 4)]),
    # DEATH: FAITHFUL (re-extracted from 6 real on-camera deaths -- see header +
    # death_anim_cap.json). The arcade hero shows NO tumble: he vanishes then holds
    # the code-83 standby pose at the death/respawn beat. We bake that as the held
    # pose 83 (one-shot, never loops); game.c blinks the sprite through the DYING
    # window (DEATH_FRAMES) so it reads as the authentic vanish+death-pose beat.
    ("PLAYER_DEATH", False, [(83, PLAYER_BANK, 64)]),

    ("ENEMY_WALK",   True,  [(304, ENEMY_BANK, 5), (305, ENEMY_BANK, 5),
                             (306, ENEMY_BANK, 7), (305, ENEMY_BANK, 5)]),
    # DEATH: FAITHFUL (re-extracted from 32 real shot-kills -- see header +
    # death_anim_cap.json). The full walker collapse+explosion (SHARED_BURST):
    # 307,308,309 stagger/recoil -> 310 knocked-down corpse held ~22f ->
    # 312,313,314,315 white-debris explosion-scatter (fades out). Non-looping; the
    # engine despawns at the end of the last frame.
    ("ENEMY_DEATH",  False, SHARED_BURST),

    # ---- FLYER (classF): the BODY anim script, read from the arcade spawn handler
    # $DED8: `LDD #$E040; STD -$2,X` -> script $E040 = codes 183,184,185 bank15
    # (3-frame wing-flap loop 183->184->185->184; variant $E032 is the same codes
    # mirrored). RE-CONFIRMED from code + rendered ROM gfx (a blue/orange winged
    # creature). The old 296/297 bank2 was a wrong "eyeballed" pick (a red cluster).
    ("FLYER_FLY",    True,  [(183, 15, 4), (184, 15, 4), (185, 15, 4), (184, 15, 4)]),
    # FLYER DEATH: the flyer's OWN distinct burst (bank 9 codes 110..119 -- green
    # body parts + yellow sparks scattering, NOT the walker's white debris). Frame
    # order is the authoritative arcade sheet; cadence modeled on the walker burst
    # (live flyer-kill cadence uncaptured headless -- see death_anim_cap.json TODO).
    ("FLYER_DEATH",  False, [(110, FLYER_BANK, 3), (111, FLYER_BANK, 3),
                             (112, FLYER_BANK, 3), (113, FLYER_BANK, 4),
                             (114, FLYER_BANK, 4), (115, FLYER_BANK, 4),
                             (116, FLYER_BANK, 4), (117, FLYER_BANK, 4),
                             (118, FLYER_BANK, 4), (119, FLYER_BANK, 6)]),

    # ---- THROWER (enemyMain idx4 $BAB3): IDLE = the standing/approach pair the
    # walker-class thrower shows while closing on the player; THROW = a brief
    # one-shot lunge (321 held) on the lob, mirroring the player FIRE stab. ----
    ("THROWER_IDLE", True,  [(320, ENEMY_BANK, 6), (321, ENEMY_BANK, 6)]),
    ("THROWER_THROW",False, [(321, ENEMY_BANK, 8), (320, ENEMY_BANK, 6)]),
    ("THROWER_DEATH",False, SHARED_BURST),

    # ---- ENEMY SHOT / lobbed projectile (classI $D352): a small fireball that
    # flickers between two cells as it arcs (2-frame loop). ----
    # ESHOT_FLY = the PLAYER bullet (codes 70/71, the small dot -- confirmed). The
    # ENEMY lobbed shot (classI) is a DIFFERENT sprite: pool capture ($0DA8) shows
    # the green creature codes 96-109 bank9 (4 lobbed instances at varied Y). So the
    # enemy lob gets its own ELOB anim; player keeps ESHOT_FLY.
    ("ESHOT_FLY",    True,  [(70, ENEMY_BANK, 4), (71, ENEMY_BANK, 4)]),
    ("ELOB",         True,  [(96, 9, 4), (108, 9, 4)]),

    # =======================================================================
    #  BATCH 2 -- four more SPAWNED archetypes (census `spawn=true` families),
    #  faithful AI re-expressed from the RE handlers (see entity.c headers).
    #  Like batch 1, the attract DEMO never exercises these, so their FRAMES are
    #  taken from the authentic arcade bank-2 enemy SPRITE SHEET (eyeball-verified
    #  distinct + non-blank via tools/sheet_browse.py + sheet_pick.py); the AI is
    #  the behavioural RE. All drive the same anim runtime (anim_tick / sub_9617).
    # =======================================================================
    # ---- CREATURE_G (classG ground creature): the BODY anim script classG runs is
    #   $CB7A (set via the handler's table at $C805 -> $CB7A/$CB88), which decodes to
    #   codes 168,169,170 bank12 -- a looping 3-frame walk cycle (168->169->170->169)
    #   of a red/tan creature. RE-CONFIRMED from code + render (tools/cmp_creg.py).
    #   The old 272-274 bank2 was a wrong "eyeballed" pick. THROW reuses the same
    #   creature frames (the exact arcade lunge pose is a refinement). ----
    # classG body is codes 160-162 bank12 (definitive wave-swept pool capture,
    # tools/arc_roster.txt: $0C28 dominant 161 b12). 168-170 was a wrong sub-state.
    ("CREG_WALK",   True,  [(160, 12, 6), (161, 12, 6), (162, 12, 6), (161, 12, 6)]),
    ("CREG_THROW",  False, [(162, 12, 8), (161, 12, 6)]),
    ("CREG_DEATH",  False, SHARED_BURST),

    # ---- CREATURE_C (classC creature family): the body anim script is set at spawn
    #   $BC20 ($BC2F: `LDD #$BD01; STD -$2,X`) and the move handler $BC6C does NOT
    #   change it -> script $BD01 = codes 16,17,18 bank5, a LOOPING anim (16->17->18
    #   ->17). RE'd from code; rendered it's a small creature with an extended
    #   beam/tongue element (an unusual but genuine arcade sprite -- this is what
    #   classC renders throughout). The old 264/265 bank2 was a wrong "eyeballed"
    #   pick (an unrelated robot). Code is ground truth over appearance. ----
    ("CREC_WALK",   True,  [(16, 5, 6), (17, 5, 6), (18, 5, 6), (17, 5, 6)]),
    ("CREC_DEATH",  False, SHARED_BURST),

    # ---- COMPOSITE_E (classE large entity): the WALK body. classE's walker AI
    #   $C350 calls $C4B5, which sets the anim script from table $C4C8 (indexed by
    #   variant): scripts $C4E8.. decode to codes 110-121 bank4 (4 variants, each a
    #   looping 3-frame walk 110->111->112->111). RE-CONFIRMED from code (the script
    #   LOOPS -> walk, not a one-shot death) + render (tools/cmp_cmpe.py). The old
    #   352/353 bank2 was a wrong "eyeballed" pick. Variant 0 = 110-112. ----
    ("CMPE_WALK",   True,  [(110, 4, 6), (111, 4, 6), (112, 4, 6), (111, 4, 6)]),
    ("CMPE_DEATH",  False, SHARED_BURST),

    # ---- HOPPER (classH): the body anim script, set at spawn $CF39 -> $D1A2 via
    #   table $D1B5 -> script $D1BD = codes 304,305,306 bank2 -- i.e. classH renders
    #   the SAME walker sprite as enemyMain (its death script $D1CB even uses the
    #   walker death frames 307-310). RE-CONFIRMED from code + the live pool read of
    #   $0CE8. The old 388/389 (a bird) was a wrong "eyeballed" pick. classH is the
    #   2nd-most-common class (34 waves) and is fundamentally a tracking walker that
    #   sometimes hops -- so its sprite is the walker. (Behavioral note: the native's
    #   always-leaping HOPPER AI is a separate, deeper mismap -- classH mostly WALKS
    #   ($D02D tracking-walker) -- left for a later behavioral pass.) ----
    ("HOP_HOP",     True,  [(304, ENEMY_BANK, 6), (305, ENEMY_BANK, 6), (306, ENEMY_BANK, 6), (305, ENEMY_BANK, 6)]),
    ("HOP_DEATH",   False, SHARED_BURST),

    # =======================================================================
    #  BATCH 3 -- the iconic SonSon FRUIT variety + three more SPAWNED archetypes
    #  (obj16x16 $D57F, classD $BF6B, objSingle $E1C4). FRAME provenance, like
    #  batches 1-2: the attract DEMO never exercises these, so codes are taken
    #  from the authentic arcade SPRITE SHEET (eyeball-verified distinct + non-
    #  blank via tools/sheet_browse.py / fruit_pick.py / bank_scan.py); AI is the
    #  behavioural RE (see entity.c headers + ../sonson/port/src handlers).
    # =======================================================================
    # ---- FRUIT (the SonSon signature collectible). SonSon's fruit are ONE round
    #   fruit SHAPE recoloured by COLOR BANK into distinct fruit types with
    #   escalating point values -- exactly the arcade design (verified: sprite
    #   code 233 is a clean round fruit with a leaf+highlight; banks 0/1/9/30 bake
    #   it red / orange / green / gold, each using a distinct PAL1 colour set, so
    #   the four read as cherry / orange / apple / gold-fruit). Single static
    #   frame each (fruit don't animate); the point value lives in entity.c.
    #   The popup-number sprites (arcade codes 404-415 = 100/300/500/1000/2000/
    #   10000) confirm the value tiers; the native popup is a floating "+N" via
    #   the HUD font (the arcade's classK $0ED8 score-child mechanic, sub_CBC8/
    #   sub_CCB1, re-expressed cheaply). ----
    ("FRUIT_CHERRY", False, [(233, 0,  1)]),   # red    -> 300
    ("FRUIT_ORANGE", False, [(233, 1,  1)]),   # orange -> 500
    ("FRUIT_APPLE",  False, [(233, 9,  1)]),   # green  -> 1000
    ("FRUIT_GOLD",   False, [(233, 30, 1)]),   # gold   -> 2000

    # ---- LARGE object (obj16x16, seeded by sub_D54B into $0E48 3 slots stride
    #   $30; root $D57F -> init $D58B sets HP $8,x=#$07 + a 16px box $D,x=#$10;
    #   move $D5F3 counts a timer then reloads a velocity vector from the $DB4B
    #   table = a periodic-velocity PATROL; the multi-sprite body is assembled by
    #   the $F2B3 composite emitter). The TOUGH archetype. WALK = a distinct
    #   robed/armoured humanoid pair 322/323 (blue-caped, white body, red trim);
    #   THROW-less. Death = shared burst. (Native HP is 3, not the arcade 7 --
    #   see entity.c TODO; the literal 4-sprite composite render needs the $F2F8
    #   program-ROM sprite-assembly table, not in the gfx bake -- TODO there too.)
    # obj16x16 is a multi-piece composite; pool capture ($0E48) shows codes
    # 360 b0 / 361 b1 / 362 b2 (distinct banks/positions per piece). The native
    # renders ONE 16x16, so we use the dominant piece 362 b2 as a representative
    # (a lossy simplification of the composite; was a wrong 322/323 pick).
    ("LARGE_WALK",  True,  [(362, 2, 8)]),
    ("LARGE_DEATH", False, SHARED_BURST),

    # ---- EMITTER (classD projectile/thrower family, spawn $BF6B idx0; the
    #   periodic-fire emitter is idx2 $BFE5 -> $C00F; the launch leaf $BF75
    #   counts a fire-countdown $D,x down and on 0 spawns a projectile child
    #   (stb -$10,x=1; inc -$F,x) then reloads). A STATIONARY emplacement that
    #   periodically FIRES a projectile (re-using the classI ENEMY_SHOT). FLY =
    #   a distinct white dragonfly/stinger insect pair 318/319 (it hovers in
    #   place, wings flicking, and looses shots). Death = shared burst. ----
    # classD body = code 23 bank25 (definitive pool capture, $0AE8 dominant 23 b25;
    # same shape family as classC but the bank-25 color). Was a wrong 318/319 pick.
    ("EMIT_IDLE",   True,  [(23, 25, 6), (22, 25, 6)]),
    ("EMIT_FIRE",   False, [(23, 25, 8), (22, 25, 6)]),
    ("EMIT_DEATH",  False, SHARED_BURST),

    # ---- ORB (objSingle unique object, spawn $E1C4 -> behaviour $E1F5; a
    #   parent-linked single-instance object that settles relative to the scroll
    #   and runs its own anim script $E27D/$E24D). Re-expressed as a slow HOVERING
    #   hazard -- a glowing orb (round sprite 231 in bank 14 -> orange glow, kept
    #   visually distinct from the COLLECTIBLE fruit by its motion: it drifts +
    #   bobs and is LETHAL on contact, never collected). 2-frame shimmer. Death =
    #   shared burst. ----
    ("ORB_FLOAT",   True,  [(231, 14, 6), (230, 14, 6)]),
    ("ORB_DEATH",   False, SHARED_BURST),
]


def bake(roms, base):
    """Bake every distinct (code,bank) frame, deduped by baked bytes. Returns
    (tiles[list u32], anim_table[list of (name,loop,[(tileBaseAbs,dur),...])],
     n_frames_total, n_distinct_sprites)."""
    tiles = []                 # flat u32 (8 per 8x8 tile)
    dedup = {}                 # baked 32-long tuple -> local base tile index
    combo = {}                 # (code,bank) -> local base tile index
    n_frames = 0

    def frame_tile_base(code, bank):
        nonlocal n_frames
        key = (code, bank)
        if key in combo:
            return combo[key]
        baked = G.bake_sprite_tiles(roms, code, bank)   # 32 longs (4 tiles)
        if baked in dedup:
            local = dedup[baked]
        else:
            local = len(tiles) // 8
            tiles.extend(baked)
            dedup[baked] = local
        combo[key] = local
        return local

    anim_table = []
    for name, loop, frames in ANIMS:
        out_frames = []
        for (code, bank, dur) in frames:
            local = frame_tile_base(code, bank)
            out_frames.append((base + local, dur))     # ABSOLUTE tile index
            n_frames += 1
        anim_table.append((name, loop, out_frames))

    return tiles, anim_table, n_frames, len(dedup)


def emit(tiles, anim_table, base, out_c, out_h, n_distinct):
    n_tiles = len(tiles) // 8
    n_sprites = n_distinct
    anim_names = [a[0] for a in anim_table]

    with open(out_h, "w", newline="\n") as f:
        f.write("""/* ss_anim.h -- GENERATED by tools/gen_anim.py.  DO NOT EDIT.
 *
 * SonSon (Capcom, 1984) PLAYER + ENEMY animation frames, baked to native Genesis
 * 4bpp sprite tiles (PAL1) + an animation table. Frame sequences captured from
 * the live arcade attract-demo spriteram (see gen_anim.py header for provenance
 * + the verified bake path). Each frame is a 16x16 sprite = SPRITE_SIZE(2,2) =
 * 4 column-major 8x8 tiles; an anim is an ordered list of {tile, duration}.
 *
 * The engine loads ss_anim_tiles[] at VRAM tile index SS_ANIM_TILE_BASE; the
 * per-frame `tile` fields below are ABSOLUTE (already include the base).
 */
#ifndef SS_ANIM_H
#define SS_ANIM_H

#include <genesis.h>

""")
        f.write("/* VRAM tile index the engine must load ss_anim_tiles[] at. */\n")
        f.write("#define SS_ANIM_TILE_BASE   %d\n\n" % base)
        f.write("/* Total baked 8x8 tiles (deduped 16x16 frames * 4). */\n")
        f.write("#define SS_ANIM_TILE_COUNT  %d\n" % n_tiles)
        f.write("#define SS_ANIM_FRAME_COUNT %d   /* distinct 16x16 frames (deduped) */\n\n"
                % n_sprites)

        f.write("/* Baked 4bpp Genesis tiles: SS_ANIM_TILE_COUNT tiles, 8 u32 each\n"
                " * (one 8px row/long, hi nibble = leftmost px); sprite frames in\n"
                " * groups of 4 (column-major TL,BL,TR,BR). */\n")
        f.write("extern const u32 ss_anim_tiles[SS_ANIM_TILE_COUNT * 8];\n\n")

        f.write("/* One animation frame: the 16x16 sprite's absolute base tile +\n"
                " * how many 60fps frames to hold it. */\n")
        f.write("typedef struct { u16 tile; u8 dur; } SSAnimFrame;\n\n")

        f.write("/* One animation: a pointer to its frame list, the frame count,\n"
                " * and whether it loops (1) or holds the last frame (0 = one-shot,\n"
                " * e.g. death). */\n")
        f.write("typedef struct {\n")
        f.write("    const SSAnimFrame *frames;\n")
        f.write("    u8 count;\n")
        f.write("    u8 loop;\n")
        f.write("} SSAnim;\n\n")

        f.write("/* Animation IDs -- index into ss_anims[]. */\n")
        f.write("typedef enum {\n")
        for i, name in enumerate(anim_names):
            f.write("    SS_ANIM_%s = %d,\n" % (name, i))
        f.write("    SS_ANIM_COUNT = %d\n" % len(anim_names))
        f.write("} SSAnimId;\n\n")

        f.write("extern const SSAnim ss_anims[SS_ANIM_COUNT];\n\n")
        f.write("#endif /* SS_ANIM_H */\n")

    with open(out_c, "w", newline="\n") as f:
        f.write("/* ss_anim.c -- GENERATED by tools/gen_anim.py.  DO NOT EDIT.\n")
        f.write(" *\n")
        f.write(" * PROVENANCE: SonSon arcade animation frames, captured from the live\n")
        f.write(" * attract-demo spriteram (MAME sonson, NTSC) + cross-checked vs the\n")
        f.write(" * ROM anim scripts + sub_9617 stepper. Sprite tiles baked via the\n")
        f.write(" * verified gen_assets.bake_sprite_tiles (byte-identical to the port's\n")
        f.write(" * build_sprite -- see tools/verify_bake.py).\n")
        f.write(" *\n")
        f.write(" * %d distinct 16x16 frames -> %d baked 8x8 tiles (%d bytes VRAM),\n"
                % (n_sprites, n_tiles, n_tiles * 32))
        f.write(" * loaded at VRAM tile base %d.\n" % base)
        f.write(" */\n")
        f.write('#include "ss_anim.h"\n\n')

        # tiles
        f.write("const u32 ss_anim_tiles[SS_ANIM_TILE_COUNT * 8] = {\n")
        for ti in range(n_tiles):
            longs = tiles[ti * 8: ti * 8 + 8]
            f.write("    /* tile %3d */ %s,\n"
                    % (ti, ",".join("0x%08X" % w for w in longs)))
        f.write("};\n\n")

        # per-anim frame arrays
        for name, loop, frames in anim_table:
            f.write("static const SSAnimFrame ss_af_%s[%d] = {\n" % (name, len(frames)))
            f.write("    " + ", ".join("{%d,%d}" % (t, d) for (t, d) in frames) + "\n")
            f.write("};\n")
        f.write("\n")

        # the anim table
        f.write("const SSAnim ss_anims[SS_ANIM_COUNT] = {\n")
        for name, loop, frames in anim_table:
            f.write("    { ss_af_%s, %d, %d },   /* SS_ANIM_%s */\n"
                    % (name, len(frames), 1 if loop else 0, name))
        f.write("};\n")


def main(argv):
    arcade_dir = os.path.join(G.REPO, "arcade")
    out_c = os.path.join(G.REPO, "src", "ss_anim.c")
    out_h = os.path.join(G.REPO, "inc", "ss_anim.h")
    base = 346      # right after ss_gfx scene tiles (135) + ss_level tiles (211)

    args = list(argv); i = 0
    while i < len(args):
        a = args[i]
        if a == "--arcade": arcade_dir = args[i + 1]; i += 2
        elif a == "--out-c": out_c = args[i + 1]; i += 2
        elif a == "--out-h": out_h = args[i + 1]; i += 2
        elif a == "--base": base = int(args[i + 1]); i += 2
        elif a.startswith("--"): sys.exit("unknown option %s" % a)
        else: i += 1

    roms = G.Roms(arcade_dir)
    tiles, anim_table, n_frames, n_distinct = bake(roms, base)

    os.makedirs(os.path.dirname(out_c), exist_ok=True)
    os.makedirs(os.path.dirname(out_h), exist_ok=True)
    emit(tiles, anim_table, base, out_c, out_h, n_distinct)

    n_tiles = len(tiles) // 8
    print("gen_anim: %d anims, %d frame-refs, %d distinct 16x16 frames"
          % (len(anim_table), n_frames, n_distinct))
    print("  baked 8x8 tiles : %d  (%d bytes VRAM, %.1f%% of 2048)"
          % (n_tiles, n_tiles * 32, 100.0 * n_tiles / 2048))
    print("  tile base index : %d  (VRAM tiles %d..%d)"
          % (base, base, base + n_tiles - 1))
    for name, loop, frames in anim_table:
        print("    %-13s %s %d frames: %s"
              % (name, "loop" if loop else "once", len(frames),
                 " ".join("t%d/%df" % (t, d) for (t, d) in frames)))
    print("  wrote", out_c)
    print("  wrote", out_h)


if __name__ == "__main__":
    main(sys.argv[1:])

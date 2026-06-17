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
    DEATH : the demo hero does not die on-camera (TODO: re-extract the exact
            death tumble script). Approximated as a held distinct pose (83/80)
            with a final hold -- see PLAYER_DEATH below + the TODO.

  ENEMY (the bank-2 ground enemy, the most common chaser in the demo):
    WALK  : 304 -> 305 -> 306 -> 305   (0x130..0x132; ~4-7 frames/cell)
    DEATH : 307 -> 308 -> 309 -> 310   (0x133..0x136; 2 frames each then 310 the
            corpse/explosion held ~22 frames) -- observed terminal burst before
            the slot recycled.

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

ANIMS = [
    # name,            loop, [ (code, bank, dur), ... ]
    ("PLAYER_RUN",   True,  [(64, PLAYER_BANK, 5), (65, PLAYER_BANK, 5),
                             (80, PLAYER_BANK, 5), (65, PLAYER_BANK, 5)]),
    ("PLAYER_CLIMB", True,  [(66, PLAYER_BANK, 4), (67, PLAYER_BANK, 4),
                             (68, PLAYER_BANK, 4), (69, PLAYER_BANK, 4)]),
    ("PLAYER_FIRE",  False, [(83, PLAYER_BANK, 6), (65, PLAYER_BANK, 4)]),
    # DEATH: approximated (see header TODO) -- a brief stagger on the action pose
    # then the up/passing frame held as a fall. Faithful death-script re-extract
    # is a TODO; this reads as a distinct death beat and never loops.
    ("PLAYER_DEATH", False, [(83, PLAYER_BANK, 8), (80, PLAYER_BANK, 8),
                             (80, PLAYER_BANK, 16)]),

    ("ENEMY_WALK",   True,  [(304, ENEMY_BANK, 5), (305, ENEMY_BANK, 5),
                             (306, ENEMY_BANK, 7), (305, ENEMY_BANK, 5)]),
    # DEATH: 307,308,309 quick then 310 held (corpse). Non-looping; the engine
    # despawns at the end of the last (held) frame.
    ("ENEMY_DEATH",  False, [(307, ENEMY_BANK, 3), (308, ENEMY_BANK, 3),
                             (309, ENEMY_BANK, 3), (310, ENEMY_BANK, 9)]),
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

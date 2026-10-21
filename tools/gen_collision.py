#!/usr/bin/env python3
"""gen_collision.py -- bake a PER-COLUMN TERRAIN COLLISION descriptor for each
native level, from the already-baked visual map (src/ss_level.c).

WHY (RE, cited):
  SonSon's terrain collision (arcade sub_A847 @ $A847) tests a per-column bitmap
  the column streamer builds at RAM $0100..$017F: 32 columns x 4 bytes, where the
  4 bytes encode a 32-bit "is this 8px row solid?" mask for that column. The mask
  is streamed (sub_A82F @ $A82F: copies 4 bytes/col from the level terrain source
  pointer $8E into $0100+col*4) from a per-stage terrain table in program ROM
  (record @ $6485 + stage*7, +5 = the $8E source; 128 bytes = 32 cols x 4 / stage).
  Decoding that ROM data (ss.02e/ss.03e at $825B, contiguous per stage) shows the
  playfield is exactly 6 SOLID TIER bands at Y>>3 rows {4,8,12,16,20,24} (Y =
  {32,64,96,128,160,192}) -- UNIVERSAL across all 26 stages, with NO solidity on
  any other row -- plus sparse per-column GAPS (holes to fall/climb through), 0-2
  gap columns per tier per 32-col stage.

NATIVE MAPPING (cited):
  The 6 arcade tiers map 1:1 to the 6 VISUAL vine floors the native already
  renders. The baked visual map (gen_level.py) places those decorated floor tiles
  at BG rows {7,11,15,19,23,27} (Y = {56,88,120,152,184,216}); a HOLE in a floor
  is a blank cell (local tile 0 == absolute SS_LEVEL_TILE_BASE 135, all-zero
  transparent tile) on that floor row -- exactly the broken-vine break the player
  falls through on screen. The native enemy grid k_spawn_row_y =
  {40,72,104,136,168,200} (entity.c) is sprite TOP-LEFT, i.e. floorY-16, so a
  16px actor's feet rest on the floor; the player uses the same grid.

  So we derive the faithful per-column collision DIRECTLY from what is rendered:
  for each level column, a 6-bit solidity mask, bit t (t=0 top tier .. 5 bottom)
  = 1 if that tier's floor cell at this column is a real floor tile (solid), 0 if
  it is the blank hole tile. This guarantees the player stands on exactly the
  floors he sees and falls through exactly the holes he sees -- the most faithful
  result for the actually-rendered native levels.

EMITS  inc/ss_collision.h + src/ss_collision.c:
  - SS_COLLISION_TIERS (6), SS_TIER_Y[6] (sprite top-left standing Y per tier,
    bottom..top order to match the lane index), SS_TIER_FLOOR_ROW[6] (BG row).
  - const u8 *ss_collision[SS_LEVEL_COUNT]  -- per level, cols bytes; byte c =
    6-bit tier-solidity mask for column c (bit (1<<tier), tier 0 = BOTTOM floor).
  - ss_collision_cols[SS_LEVEL_COUNT] (== ss_levels[i].cols).

The engine (game.c) reads ss_collision[level][col] and tests bit (1<<tier) to seat
the player / detect gaps -- the native sub_A847.
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)

# The 6 visual vine-floor BG rows (Y = row*8). Floor f (0..5) top..bottom on screen.
FLOOR_ROWS_TOP_TO_BOTTOM = [7, 11, 15, 19, 23, 27]      # Y 56,88,120,152,184,216
# The "blank hole" tile is local index 0 of each level = that level's VRAM base.
# In the SEAMLESS-RIBBON build every level has its OWN base (all co-resident), so we
# auto-detect each level's base as the MINIMUM tile index in its map (local 0 is the
# all-zero transparent tile and always the lowest absolute index). This is robust to
# whatever base gen_level.py assigned -- no hard-coded SS_LEVEL_TILE_BASE here.

# Tier index in the ENGINE is bottom(0)..top(5) to match the lane index
# (lane 0 = bottom). So tier t uses floor row FLOOR_ROWS_TOP_TO_BOTTOM[5-t].
def tier_floor_row(tier):
    return FLOOR_ROWS_TOP_TO_BOTTOM[5 - tier]

# Sprite TOP-LEFT standing Y per tier (bottom..top) = floorY - 16 (feet on floor).
def tier_top_y(tier):
    return tier_floor_row(tier) * 8 - 16


def parse_levels(c_path):
    txt = open(c_path).read()
    # Per-level VRAM tile base (the 5th field of each ss_levels[] initializer).
    # In the seamless-ribbon build each level has its OWN base; the blank HOLE tile
    # on a floor row is exactly that base (local 0 = the all-zero transparent tile),
    # while the top HUD band is a literal tile 0 -- so we MUST key the hole test on
    # the level's base, not min(map). Parse the bases from the initializer block.
    bases = []
    mb = re.search(r'ss_levels\[SS_LEVEL_COUNT\]\s*=\s*\{(.*?)\};', txt, re.S)
    if mb:
        for line in mb.group(1).split('\n'):
            mm = re.search(r'\{\s*ss_level\d+_tiles\s*,\s*\d+\s*,\s*ss_level\d+_map\s*,\s*\d+\s*,\s*(\d+)\s*\}', line)
            if mm:
                bases.append(int(mm.group(1)))
    levels = []
    i = 1
    while True:
        m = re.search(r'ss_level%d_map\[(\d+) \* SS_LEVEL_ROWS\] = \{(.*?)\};' % i, txt, re.S)
        if not m:
            break
        cols = int(m.group(1))
        body = m.group(2)
        rows = []
        for line in body.strip().split('\n'):
            nums = re.findall(r'0x[0-9A-Fa-f]{4}', line)
            if nums:
                rows.append([int(x, 16) & 0x07FF for x in nums])   # low 11 bits = tile index
        assert len(rows) == 32, "level %d: expected 32 rows, got %d" % (i, len(rows))
        base = bases[i - 1] if (i - 1) < len(bases) else 0
        levels.append((cols, rows, base))
        i += 1
    return levels


def build_collision(cols, rows, base):
    """Per column: 6-bit mask, bit t (t=0 BOTTOM..5 TOP) = tier t solid here.
    A floor cell == `base` (local 0 = the all-zero transparent tile) is a HOLE."""
    out = []
    for c in range(cols):
        mask = 0
        for t in range(6):
            fr = tier_floor_row(t)
            if rows[fr][c] != base:                 # real floor tile -> solid
                mask |= (1 << t)
        out.append(mask)
    return out


def main():
    c_in = os.path.join(REPO, "src", "ss_level.c")
    out_h = os.path.join(REPO, "inc", "ss_collision.h")
    out_c = os.path.join(REPO, "src", "ss_collision.c")
    levels = parse_levels(c_in)
    n = len(levels)
    assert n >= 1, "no levels parsed from %s" % c_in

    masks = [build_collision(cols, rows, base) for (cols, rows, base) in levels]

    with open(out_h, "w", newline="\n") as f:
        f.write("/* ss_collision.h -- GENERATED by tools/gen_collision.py.  DO NOT EDIT.\n")
        f.write(" *\n")
        f.write(" * Per-column TERRAIN COLLISION for each native level -- the native\n")
        f.write(" * equivalent of SonSon's arcade $0100 collision bitmap (built by the\n")
        f.write(" * column streamer sub_A82F @ $A82F from the per-stage terrain table at\n")
        f.write(" * program-ROM record $6485+stage*7,+5; tested by sub_A847 @ $A847).\n")
        f.write(" * Decoding that ROM (ss.02e/03e @ $825B) shows 6 solid tier bands at\n")
        f.write(" * Y>>3 rows {4,8,12,16,20,24} (universal, every stage) + sparse per-column\n")
        f.write(" * gaps. Here we derive the SAME 6-tier-per-column model from the rendered\n")
        f.write(" * visual map (a blank floor cell == a hole), so the player stands on /\n")
        f.write(" * falls through exactly what is on screen. See gen_collision.py header.\n")
        f.write(" */\n")
        f.write("#ifndef SS_COLLISION_H\n#define SS_COLLISION_H\n\n#include <genesis.h>\n")
        f.write('#include "ss_level.h"\n\n')
        f.write("#define SS_COLLISION_TIERS  6\n\n")
        f.write("/* Sprite TOP-LEFT standing Y per tier, BOTTOM(0)..TOP(5). Feet rest on the\n")
        f.write(" * visual vine floor (floorY-16). Matches entity.c k_spawn_row_y reversed. */\n")
        f.write("extern const s16 SS_TIER_Y[SS_COLLISION_TIERS];\n")
        f.write("/* BG floor row per tier, BOTTOM(0)..TOP(5) (Y = row*8). */\n")
        f.write("extern const u8  SS_TIER_FLOOR_ROW[SS_COLLISION_TIERS];\n\n")
        f.write("/* Per level: `cols` bytes; byte c = 6-bit tier-solidity mask for column c.\n")
        f.write(" * Bit (1<<t) set => tier t (0=bottom..5=top) is SOLID at column c; clear =>\n")
        f.write(" * a hole (fall/climb through). Index ss_collision[level][col]. */\n")
        f.write("extern const u8 *const ss_collision[SS_LEVEL_COUNT];\n")
        f.write("extern const u16 ss_collision_cols[SS_LEVEL_COUNT];\n")
        f.write("\n#endif /* SS_COLLISION_H */\n")

    with open(out_c, "w", newline="\n") as f:
        f.write("/* ss_collision.c -- GENERATED by tools/gen_collision.py.  DO NOT EDIT.\n")
        f.write(" * Per-column 6-tier terrain collision, derived from the rendered level\n")
        f.write(" * floors (see ss_collision.h / gen_collision.py). %d levels.\n */\n" % n)
        f.write('#include "ss_collision.h"\n\n')

        ty = [tier_top_y(t) for t in range(6)]
        fr = [tier_floor_row(t) for t in range(6)]
        f.write("const s16 SS_TIER_Y[SS_COLLISION_TIERS] = { %s };\n"
                % ", ".join(str(v) for v in ty))
        f.write("const u8  SS_TIER_FLOOR_ROW[SS_COLLISION_TIERS] = { %s };\n\n"
                % ", ".join(str(v) for v in fr))

        for i, m in enumerate(masks):
            f.write("static const u8 ss_collision%d[%d] = {\n" % (i + 1, len(m)))
            for j in range(0, len(m), 16):
                f.write("    " + ",".join("0x%02X" % v for v in m[j:j + 16]) + ",\n")
            f.write("};\n")
        f.write("\nconst u8 *const ss_collision[SS_LEVEL_COUNT] = {\n")
        f.write("    " + ", ".join("ss_collision%d" % (i + 1) for i in range(n)) + "\n};\n")
        f.write("const u16 ss_collision_cols[SS_LEVEL_COUNT] = {\n")
        f.write("    " + ", ".join(str(len(m)) for m in masks) + "\n};\n")

    print("gen_collision: %d levels" % n)
    for i, m in enumerate(masks):
        holes = sum(bin(0x3F ^ v).count("1") for v in m)
        print("  level%d: %d cols, %d tier-holes total" % (i + 1, len(m), holes))
    print("  SS_TIER_Y           =", [tier_top_y(t) for t in range(6)], "(bottom..top)")
    print("  SS_TIER_FLOOR_ROW   =", [tier_floor_row(t) for t in range(6)])
    print("  wrote", out_h)
    print("  wrote", out_c)


if __name__ == "__main__":
    main()

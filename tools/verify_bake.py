#!/usr/bin/env python3
"""verify_bake.py -- prove gen_assets' offline 4bpp bake is BYTE-IDENTICAL to the
verified runtime (../../sonson/port/src/video.c build_char / build_sprite).

This is the Phase-1 exit gate (PIPELINE_SPEC.md 5.6): for a handful of (code,bank)
char and sprite pairs from the captured scene, assert gen_assets' baked Genesis
4bpp tile bytes equal what the port's build_char/build_sprite produce.

TWO INDEPENDENT PATHS are compared (so this isn't just "the same code twice"):

  Path A (gen_assets, the thing under test): decodes pens straight from the RAW
      arcade ROMs (ss_7/ss_8, ss_9..ss_14) with the LEFT/RIGHT sprite split, then
      bakes with the char/sprite LUTs.  -> gen_assets.bake_char_tile /
      bake_sprite_tiles.

  Path B (runtime transcription, the ORACLE): a faithful line-by-line Python
      transcription of video.c's build_char / build_sprite, reading the PORT's
      OWN pre-decoded pixel tables (ss_char_px, ss_sprite_px) + LUTs
      (ss_char_lut, ss_sprite_lut) parsed verbatim from the committed
      gfx_assets.c.  This is exactly what runs on the Genesis (same source pens,
      same mirror transform `sx = (sx0&8)|(7-(sx0&7))`, same pen-0/LUT-0->15
      transparency, same column-major tile order).

Different input data (raw ROMs vs port's decoded tables) AND different code paths
converging on identical output bytes = the offline bake faithfully reproduces the
runtime.  Also asserts the port's LUTs/palette regen match gen_assets' (provenance).

Run:  python verify_bake.py
Exit code 0 = all pairs byte-equal; nonzero = mismatch (prints the diff).
"""

import os
import re
import sys
import json

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_assets as G

PORT_GFX_C = r"D:\genesis ports\_capcom\son son\sonson\port\src\gfx_assets.c"


# ---------------------------------------------------------------------------
# Parse the port's committed tables from gfx_assets.c (the runtime's input).
# ---------------------------------------------------------------------------
def _parse_array(text, decl):
    """Extract the integer initializer list for a `... name[...] = { ... };`."""
    m = re.search(re.escape(decl) + r"\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        sys.exit("verify: could not find array %r in port gfx_assets.c" % decl)
    body = m.group(1)
    return [int(tok, 0) for tok in re.findall(r"0x[0-9A-Fa-f]+|\d+", body)]


def load_port_tables():
    with open(PORT_GFX_C, "r") as f:
        text = f.read()
    return {
        "ss_char_px":   _parse_array(text, "const uint8_t ss_char_px[SS_N_CHAR * SS_CHAR_BYTES]"),
        "ss_sprite_px": _parse_array(text, "const uint8_t ss_sprite_px[SS_N_SPRITE * SS_SPRITE_BYTES]"),
        "ss_char_lut":  _parse_array(text, "const uint8_t ss_char_lut[256]"),
        "ss_sprite_lut": _parse_array(text, "const uint8_t ss_sprite_lut[256]"),
        "ss_pal_char":  _parse_array(text, "const uint16_t ss_pal_char[16]"),
        "ss_pal_sprite": _parse_array(text, "const uint16_t ss_pal_sprite[16]"),
    }


# ---------------------------------------------------------------------------
# Path B: faithful transcription of video.c build_char / build_sprite, reading
# the PORT's pre-decoded tables (NOT the raw ROMs).
# ---------------------------------------------------------------------------
SS_CHAR_BYTES = 16
SS_SPRITE_BYTES = 128


def runtime_build_char(port, code, bank):
    """Transcription of video.c build_char(). Returns 8 u32 longwords.
    (The synthetic-comma code 0x9F branch is front-end-only and never appears
    in a BG scene; we don't special-case it here -- the captured scene uses
    real arcade codes.)"""
    src = port["ss_char_px"][code * SS_CHAR_BYTES: code * SS_CHAR_BYTES + 16]
    lut = port["ss_char_lut"][bank * 4: bank * 4 + 4]
    out = []
    for row in range(8):
        b0 = src[row * 2 + 0]
        b1 = src[row * 2 + 1]
        w = 0
        for q in range(4):                 # px 0..3 from b0
            pen = (b0 >> (6 - q * 2)) & 0x3
            w = ((w << 4) | (lut[pen] & 0xF)) & 0xFFFFFFFF
        for q in range(4):                 # px 4..7 from b1
            pen = (b1 >> (6 - q * 2)) & 0x3
            w = ((w << 4) | (lut[pen] & 0xF)) & 0xFFFFFFFF
        out.append(w)
    return tuple(out)


def runtime_build_sprite(port, code, bank):
    """Transcription of video.c build_sprite(). Returns 32 u32 longwords
    (4 tiles x 8 rows, column-major: gtile = tcol*2 + trow).
    Uses the port's contiguous ss_sprite_px + the mirror transform
    sx = (sx0 & 8) | (7 - (sx0 & 7)), identity pens, pen-0/LUT-0->15."""
    src = port["ss_sprite_px"][code * SS_SPRITE_BYTES: code * SS_SPRITE_BYTES + 128]
    lut = port["ss_sprite_lut"][bank * 8: bank * 8 + 8]
    out = []
    for tcol in range(2):
        for trow in range(2):
            for ry in range(8):
                sy = trow * 8 + ry
                w = 0
                for rx in range(8):
                    sx0 = tcol * 8 + rx
                    sx = (sx0 & 8) | (7 - (sx0 & 7))
                    byte = src[sy * 8 + (sx >> 1)]
                    pen = (byte & 0xF) if (sx & 1) else (byte >> 4)
                    if pen == 0:
                        gp = 0
                    else:
                        gp = lut[pen] & 0xF
                        if gp == 0:
                            gp = 15
                    w = ((w << 4) | gp) & 0xFFFFFFFF
                out.append(w)
    return tuple(out)


def fmt(tile):
    return " ".join("%08X" % w for w in tile)


def main():
    roms = G.Roms(os.path.join(G.REPO, "arcade"))
    port = load_port_tables()

    # --- provenance: gen_assets LUTs/palette must match the port's committed ---
    ok = True
    if roms.char_lut != port["ss_char_lut"]:
        print("FAIL: char_lut differs from port"); ok = False
    if roms.sprite_lut != port["ss_sprite_lut"]:
        print("FAIL: sprite_lut differs from port"); ok = False
    if G.SS_PAL_CHAR != port["ss_pal_char"]:
        print("FAIL: ss_pal_char differs from port"); ok = False
    if G.SS_PAL_SPRITE != port["ss_pal_sprite"]:
        print("FAIL: ss_pal_sprite differs from port"); ok = False
    if ok:
        print("provenance: gen_assets char_lut/sprite_lut + both palettes == port (verbatim) OK")

    # --- pick pairs from the captured scene ---
    scene_path = os.path.join(HERE, "scene_capture.json")
    with open(scene_path) as f:
        scene = json.load(f)

    # ~5+ char pairs: take a spread of distinct (code,bank) from the scene,
    # preferring non-blank codes (exercise real LUT remaps).
    char_pairs = []
    seen = set()
    for c, b in scene["char_used"]:
        if (c, b) in seen:
            continue
        seen.add((c, b))
        # is the char non-blank? (any pen != 0 in the decoded tile)
        nonblank = any(any(px for px in row) for row in roms.char_pen[c])
        if nonblank:
            char_pairs.append((c, b))
        if len(char_pairs) >= 8:
            break
    if len(char_pairs) < 5:                       # top up with whatever exists
        for c, b in scene["char_used"]:
            if (c, b) not in char_pairs:
                char_pairs.append((c, b))
            if len(char_pairs) >= 5:
                break

    # ~3+ sprite pairs: distinct (code,bank), prefer non-blank.
    spr_pairs = []
    seen = set()
    for entry in scene["sprite_slots"]:
        c, b = entry[1], entry[2]
        if (c, b) in seen:
            continue
        seen.add((c, b))
        nonblank = any(any(px for px in row) for row in roms.spr_pen[c])
        if nonblank:
            spr_pairs.append((c, b))
        if len(spr_pairs) >= 5:
            break
    if len(spr_pairs) < 3:
        for entry in scene["sprite_slots"]:
            c, b = entry[1], entry[2]
            if (c, b) not in spr_pairs:
                spr_pairs.append((c, b))
            if len(spr_pairs) >= 3:
                break

    print("\n=== CHAR byte-equality (gen_assets raw-ROM bake vs runtime "
          "build_char on port tables) ===")
    cfail = 0
    for c, b in char_pairs:
        a = G.bake_char_tile(roms, c, b)
        rbt = runtime_build_char(port, c, b)
        same = (a == rbt)
        print("  code=%4d bank=%2d : %s" % (c, b, "EQUAL" if same else "**DIFF**"))
        if not same:
            cfail += 1
            print("    genA:", fmt(a))
            print("    rtB :", fmt(rbt))

    print("\n=== SPRITE byte-equality (gen_assets raw-ROM bake vs runtime "
          "build_sprite on port tables) ===")
    sfail = 0
    for c, b in spr_pairs:
        a = G.bake_sprite_tiles(roms, c, b)        # 32 longs
        rbt = runtime_build_sprite(port, c, b)
        same = (a == rbt)
        print("  code=%4d bank=%2d : %s (4 tiles, column-major)"
              % (c, b, "EQUAL" if same else "**DIFF**"))
        if not same:
            sfail += 1
            for t in range(4):
                ta = a[t * 8:t * 8 + 8]
                tb = rbt[t * 8:t * 8 + 8]
                tag = "ok" if ta == tb else "DIFF"
                print("    tile%d %s genA:" % (t, tag), fmt(ta))
                print("    tile%d %s rtB :" % (t, tag), fmt(tb))

    print("\n--- RESULT ---")
    print("char pairs checked  : %d (%d mismatch)" % (len(char_pairs), cfail))
    print("sprite pairs checked: %d (%d mismatch)" % (len(spr_pairs), sfail))
    if not ok or cfail or sfail:
        print("VERIFY FAILED")
        return 1
    print("VERIFY PASSED: offline bake is byte-identical to the verified runtime.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

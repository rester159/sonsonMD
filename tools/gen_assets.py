#!/usr/bin/env python3
"""gen_assets.py -- SonSon (Capcom, 1984) arcade GFX/PROM -> NATIVE Genesis
4bpp tiles, baked ONCE at build time for a captured scene's working set.

Phase-1 asset pipeline for the native port (see tools/PIPELINE_SPEC.md, which is
authoritative). This is the BUILD-TIME equivalent of the verified runtime
converter in ../../sonson/port/src/video.c (`build_char` / `build_sprite`): it
applies the arcade INDIRECT palette (pen + colorbank -> PROM LUT -> one of 16
fixed colors) offline, so the native game writes the VDP nametable / SAT
directly with ZERO per-frame conversion.

DETERMINISTIC, Python 3 stdlib only. Every output byte traces to an exact arcade
ROM/PROM byte (provenance in the emitted file header).

KEY SPEC POINTS implemented here (verbatim from PIPELINE_SPEC.md):
  - Chars: 8x8, 2 bitplanes (pen 0..3), from ss_7.b6 (LSB) + ss_8.b5 (MSB);
    bit7 = leftmost pixel.  (ss_6.c11 is NOT consumed -- see spec note.)
  - Sprites: 16x16, 3 bitplanes (pen 0..7), planes = ss_9++ss_10 (LSB),
    ss_11++ss_12, ss_13++ss_14 (MSB). 32 bytes/plane/tile. The split-column
    quirk: bytes[t*32 + r] = RIGHT 8px (cols 8..15, MSB=col8);
            bytes[t*32 + 16 + r] = LEFT 8px (cols 0..7, MSB=col0).
    Pen order is IDENTITY (no bit-reversal). We decode straight from the raw
    ROMs with this LEFT/RIGHT rule -> correctly oriented pixels, no mirror pass
    (spec 2.2: identical to the runtime's contiguous-buffer + mirror path).
  - Indirect palette step A (the LUT): char_lut[bank*4+pen] = ssb2.c4 & 0x0F
    (PAL0 index 0..15); sprite_lut[bank*8+pen] = ssb3.h7 & 0x0F (PAL1 index).
  - Palettes (step B, the RGB) are taken VERBATIM from spec 5.4 -- NOT
    regenerated from the DAC formula (6 of 32 entries are hand-tuned vs arcade
    screenshots; regenerating would drift). We DO recompute the DAC values and
    assert they match the committed words except those known 6 (warn-only).
  - Final 4bpp bake (spec 2.4):
      CHAR  pixel = char_lut[bank*4+pen] & 0xF   (no pen-0 special case)
      SPRITE pixel: pen==0 -> 0 (hw-transparent); else gp=sprite_lut[bank*8+pen]
                    & 0xF, and if gp==0 -> 15 (opaque duplicate-black slot 15).
    Pack hi-nibble = leftmost pixel. 16x16 sprite -> 4 Genesis 8x8 tiles in
    COLUMN-MAJOR order (gtile = tcol*2 + trow: TL, BL, TR, BR).

USAGE:
    python gen_assets.py [scene_capture.json] [--arcade DIR] [--out-c PATH]
                         [--out-h PATH] [--base N]
  Defaults: scene_capture.json next to this script; arcade/ in the repo;
  res/ss_gfx.c + inc/ss_gfx.h; tile base = 0 (documented; the render loads the
  baked tiles starting at this VRAM tile index).

This module is ALSO importable: `import gen_assets` then use load_roms(),
bake_char_tile(), bake_sprite_tiles() -- the verify harness (verify_bake.py)
uses these to cross-check byte-equality vs the port's build_char/build_sprite.
"""

import os
import sys
import json

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)                       # .../son son/sonson_native

# ---------------------------------------------------------------------------
# Palette arrays -- VERBATIM from PIPELINE_SPEC.md 5.4 (= the verified
# ../../sonson/port/src/gfx_assets.c, lines 5173/5175). Do NOT regenerate.
# ---------------------------------------------------------------------------
SS_PAL_CHAR = [
    0x0000, 0x0888, 0x0080, 0x0226, 0x0228, 0x004A, 0x0AAA, 0x0666,
    0x0420, 0x0842, 0x0424, 0x0848, 0x00AA, 0x0A8A, 0x0C66, 0x000C,
]
SS_PAL_SPRITE = [
    0x0000, 0x0CCC, 0x0888, 0x068C, 0x000A, 0x0006, 0x006A, 0x00AC,
    0x000E, 0x0808, 0x0A04, 0x0C66, 0x006E, 0x0080, 0x00A0, 0x0000,
]

# DAC weights (MAME "Commando" resistor approximation) -- used ONLY for the
# provenance assert in 5.5 step 4 (warn, don't fail, on the 6 hand-tuned).
_DAC_W = (0x0E, 0x1F, 0x43, 0x8F)
# hw colors hand-deviated from round(nibble*7/15) per spec 5.4.
_HANDTUNED_HW = {5, 8, 9, 10, 11, 26}


def _dac(nib):
    return (_DAC_W[0] * (nib & 1) + _DAC_W[1] * ((nib >> 1) & 1) +
            _DAC_W[2] * ((nib >> 2) & 1) + _DAC_W[3] * ((nib >> 3) & 1))


def _vdp_from_dac(r, g, b):
    return (((b >> 5) & 7) << 9) | (((g >> 5) & 7) << 5) | (((r >> 5) & 7) << 1)


# ---------------------------------------------------------------------------
# ROM container
# ---------------------------------------------------------------------------
class Roms:
    """Decoded pen buffers + LUTs from the raw arcade files."""
    def __init__(self, arcade_dir):
        self.dir = arcade_dir
        self._load()
        self._decode_chars()
        self._decode_sprites()
        self._build_luts()
        self._provenance_palette_check()

    def _load_file(self, name, expect):
        p = os.path.join(self.dir, name)
        with open(p, "rb") as f:
            data = f.read()
        if len(data) != expect:
            sys.exit("FATAL: %s is %d bytes, expected %d" % (name, len(data), expect))
        return data

    def _load(self):
        self.ss7 = self._load_file("ss_7.b6", 0x2000)     # char plane0 (LSB)
        self.ss8 = self._load_file("ss_8.b5", 0x2000)     # char plane1 (MSB)
        # sprite planes: two 8KB ROMs concatenated each (16KB/plane)
        self.spr_p0 = self._load_file("ss_9.m5", 0x2000) + self._load_file("ss_10.m6", 0x2000)
        self.spr_p1 = self._load_file("ss_11.m3", 0x2000) + self._load_file("ss_12.m4", 0x2000)
        self.spr_p2 = self._load_file("ss_13.m1", 0x2000) + self._load_file("ss_14.m2", 0x2000)
        self.prom_gb = self._load_file("ssb4.b2", 0x20)   # G hi / B lo
        self.prom_r = self._load_file("ssb5.b1", 0x20)    # R lo
        self.prom_clut = self._load_file("ssb2.c4", 0x100)  # char LUT
        self.prom_slut = self._load_file("ssb3.h7", 0x100)  # sprite LUT
        # ssb1.k11 (timing) and ss_6.c11 intentionally NOT loaded -- unused.

    # --- 1024 char tiles: char_pen[code] = list of 8 rows x 8 cols (pen 0..3) -
    def _decode_chars(self):
        self.N_CHAR = 1024
        self.char_pen = []
        for t in range(self.N_CHAR):
            base = t * 8
            rows = []
            for r in range(8):
                p0 = self.ss7[base + r]
                p1 = self.ss8[base + r]
                row = []
                for c in range(8):
                    bit = 7 - c                         # bit7 = leftmost
                    pen = (((p1 >> bit) & 1) << 1) | ((p0 >> bit) & 1)
                    row.append(pen)
                rows.append(row)
            self.char_pen.append(rows)

    # --- 512 sprite tiles: spr_pen[code] = 16 rows x 16 cols (pen 0..7) -------
    #     Decoded straight from raw ROMs with the LEFT/RIGHT split rule.
    def _decode_sprites(self):
        self.N_SPR = 512
        self.spr_pen = []
        for t in range(self.N_SPR):
            base = t * 32
            rows = []
            for r in range(16):
                # RIGHT 8px = byte at [base+r]; LEFT 8px = byte at [base+16+r]
                rp0, rp1, rp2 = (self.spr_p0[base + r],
                                 self.spr_p1[base + r],
                                 self.spr_p2[base + r])
                lp0, lp1, lp2 = (self.spr_p0[base + 16 + r],
                                 self.spr_p1[base + 16 + r],
                                 self.spr_p2[base + 16 + r])
                row = []
                for c in range(16):
                    if c < 8:                           # LEFT half, MSB=col0
                        bit = 7 - c
                        p0, p1, p2 = lp0, lp1, lp2
                    else:                               # RIGHT half, MSB=col8
                        bit = 7 - (c - 8)
                        p0, p1, p2 = rp0, rp1, rp2
                    pen = (((p2 >> bit) & 1) << 2) | (((p1 >> bit) & 1) << 1) | ((p0 >> bit) & 1)
                    row.append(pen)                     # IDENTITY pen order
                rows.append(row)
            self.spr_pen.append(rows)

    def _build_luts(self):
        # char_lut[bank*4 + pen] -> PAL0 index 0..15 ; 64 banks
        self.char_lut = [self.prom_clut[i] & 0x0F for i in range(256)]
        # sprite_lut[bank*8 + pen] -> PAL1 index 0..15 ; 32 banks
        self.sprite_lut = [self.prom_slut[i] & 0x0F for i in range(256)]

    def _provenance_palette_check(self):
        """Recompute DAC palette and assert it matches the committed verbatim
        arrays except the 6 known hand-tuned hw colors (spec 5.5 step 4)."""
        self.pal_warnings = []
        for i in range(0x20):
            gb = self.prom_gb[i]
            rr = self.prom_r[i]
            r = _dac(rr & 0x0F)
            g = _dac((gb >> 4) & 0x0F)
            b = _dac(gb & 0x0F)
            word = _vdp_from_dac(r, g, b)
            committed = (SS_PAL_CHAR + SS_PAL_SPRITE)[i]
            if word != committed and i not in _HANDTUNED_HW:
                self.pal_warnings.append(
                    "hw[%2d]: DAC=0x%04X committed=0x%04X (UNEXPECTED mismatch)"
                    % (i, word, committed))


# ---------------------------------------------------------------------------
# Final 4bpp tile bake -- mirrors video.c build_char / build_sprite EXACTLY.
# Returns a tuple of 8 u32 longwords (one 8px row each, hi nibble = leftmost px).
# ---------------------------------------------------------------------------
def bake_char_tile(roms, code, bank):
    """Build one Genesis 4bpp 8x8 char tile. CHAR transparency = LUT output
    direct (no pen-0 special case) -- spec 2.4. Returns 8 u32 longwords."""
    pen_rows = roms.char_pen[code]
    lut = roms.char_lut[bank * 4: bank * 4 + 4]
    out = []
    for r in range(8):
        w = 0
        for c in range(8):
            pen = pen_rows[r][c]                # 0..3
            w = ((w << 4) | (lut[pen] & 0xF)) & 0xFFFFFFFF
        out.append(w)
    return tuple(out)


def bake_sprite_tiles(roms, code, bank):
    """Build a 16x16 sprite as 4 Genesis 8x8 tiles in COLUMN-MAJOR order
    (gtile = tcol*2 + trow). Sprite transparency rule (spec 2.4):
        pen == 0 -> Genesis pixel 0 (hw-transparent), even if LUT maps it
                    to a non-black color;
        pen != 0 -> gp = sprite_lut[bank*8+pen] & 0xF; if gp==0 -> 15.
    Returns 32 u32 longwords (4 tiles x 8 rows), tiles concatenated in
    column-major order (matches build_sprite's tile_build layout)."""
    pen_rows = roms.spr_pen[code]              # 16x16 pens 0..7, natural order
    lut = roms.sprite_lut[bank * 8: bank * 8 + 8]
    out = []
    for tcol in range(2):
        for trow in range(2):
            # gtile = tcol*2 + trow -> we emit tiles in this exact iteration
            # order, which is column-major (TL, BL, TR, BR).
            for ry in range(8):
                sy = trow * 8 + ry             # source row 0..15
                w = 0
                for rx in range(8):
                    sx0 = tcol * 8 + rx        # output col 0..15
                    # MIRROR-WITHIN-EACH-8px-HALF, identical to video.c
                    # build_sprite's  sx = (sx0 & 8) | (7 - (sx0 & 7)).
                    # NOTE (deviation from PIPELINE_SPEC.md 2.2): the spec claims
                    # decoding straight from raw ROMs with the LEFT/RIGHT rule
                    # needs NO mirror. That is NOT true relative to the VERIFIED
                    # runtime: our raw-ROM decode (spr_pen) stores pens in natural
                    # screen-col order (byte-identical to the port's ss_sprite_px,
                    # confirmed), and build_sprite RE-MIRRORS that order when it
                    # bakes the Genesis tile. To satisfy the spec's own 5.6
                    # byte-equality gate against build_sprite (the on-screen-
                    # correct ground truth), we apply the SAME mirror here.
                    # verify_bake.py proves byte-identity for all scene sprites.
                    sx = (sx0 & 8) | (7 - (sx0 & 7))
                    pen = pen_rows[sy][sx]     # 0..7
                    if pen == 0:
                        gp = 0
                    else:
                        gp = lut[pen] & 0xF
                        if gp == 0:
                            gp = 15
                    w = ((w << 4) | gp) & 0xFFFFFFFF
                out.append(w)
    return tuple(out)


# ---------------------------------------------------------------------------
# Scene pack: bake only the used (code,bank) set, dedupe identical tiles.
# ---------------------------------------------------------------------------
class ScenePack:
    """Baked working set for one scene.

    Layout in ss_scene_tiles[]:  CHAR tiles first (1 each), then SPRITE tiles
    (4 each, column-major). Deduping is by exact baked-byte equality:
      - chars dedupe per 8x8 tile;
      - sprites dedupe per 16x16 (4-tile) group as a unit, so a sprite's 4
        tiles are always contiguous and its base index addresses tile 0 (TL).
    """
    def __init__(self, roms):
        self.roms = roms
        self.tiles = []                 # flat list of u32 (8 per 8x8 tile)
        self._char_dedup = {}           # baked-bytes -> local 8x8 tile index
        self._spr_dedup = {}            # baked-bytes (32 longs) -> base index
        self.char_map = {}              # (code,bank) -> local 8x8 tile index
        self.spr_map = {}               # (code,bank) -> base 8x8 tile index
        self.n_char_tiles = 0           # count of 8x8 tiles used by chars
        self.n_spr_tiles = 0            # count of 8x8 tiles used by sprites

    def add_char(self, code, bank):
        key = (code, bank)
        if key in self.char_map:
            return self.char_map[key]
        baked = bake_char_tile(self.roms, code, bank)
        if baked in self._char_dedup:
            idx = self._char_dedup[baked]
        else:
            idx = len(self.tiles) // 8
            self.tiles.extend(baked)
            self._char_dedup[baked] = idx
            self.n_char_tiles += 1
        self.char_map[key] = idx
        return idx

    def add_sprite(self, code, bank):
        key = (code, bank)
        if key in self.spr_map:
            return self.spr_map[key]
        baked = bake_sprite_tiles(self.roms, code, bank)   # 32 longs
        if baked in self._spr_dedup:
            base = self._spr_dedup[baked]
        else:
            base = len(self.tiles) // 8
            self.tiles.extend(baked)
            self._spr_dedup[baked] = base
            self.n_spr_tiles += 4
        self.spr_map[key] = base
        return base

    @property
    def total_tiles(self):
        return len(self.tiles) // 8


# ---------------------------------------------------------------------------
# Scene assembly from a capture JSON
# ---------------------------------------------------------------------------
def assemble(roms, scene, base):
    """Returns (pack, nametable[1024], sprite_list, n_char_only_tiles) for the
    captured scene.

    - Chars are baked FIRST (so char local indices are the low part of the tile
      array) by walking the 32x32 BG in row order and adding each cell's
      (code,bank). The nametable word for a cell is TILE_ATTR_FULL(PAL0,0,0,0,
      base + localidx).
    - Then all 24 hardware sprites are baked (their distinct (code,bank) sets),
      and a sprite list emitted: {x, y, tile_base(absolute = base+local),
      flipH, flipV} per the spec 2.5 decode + the port's coord mapping.
    """
    pack = ScenePack(roms)
    vram = scene["videoram"]
    cram = scene["colorram"]
    spr = scene["spriteram"]

    # Pass 1: chars (every BG cell). Char tiles occupy the low part of the
    # baked tile array.
    nametable = [0] * 1024
    for i in range(1024):
        v = vram[i]
        a = cram[i]
        code = v + 256 * (a & 0x03)
        bank = a >> 2
        local = pack.add_char(code, bank)
        nametable[i] = tile_attr_full(0, 0, 0, 0, base + local)

    n_char_only_tiles = pack.total_tiles      # boundary: sprite tiles follow

    # Pass 2: sprites. Decode per spec 2.5; emit all 24 hardware slots in the
    # order offs 0..0x5C (front-to-back = the port's link order; the render can
    # reverse if it prefers, but we preserve arcade slot order + index).
    sprite_list = []
    for offs in range(0, 0x5D, 4):
        sy_b = spr[offs + 0]
        attr = spr[offs + 1]
        code_lo = spr[offs + 2]
        sx_b = spr[offs + 3]
        code = code_lo + ((attr & 0x20) << 3)     # 0..511
        bank = attr & 0x1F                        # 0..31
        flipx = 0 if (attr & 0x40) else 1         # INVERTED
        flipy = 0 if (attr & 0x80) else 1         # INVERTED
        base_local = pack.add_sprite(code, bank)  # base 8x8 index (TL tile)
        # Port coordinate mapping (video.c present_sprites): screen positions
        # are computed as (with g_flip == 0 in this captured scene):
        #   sx_screen = sx_b
        #   sy_screen = sy_b - 24
        # and the SAT later adds +0x80 to each. We emit the SCREEN coords
        # (pre-+0x80), matching what the render feeds VDP_setSpriteFull / the
        # raw SAT word builder. (g_flip handling is left to the render; this
        # captured frame has flip_screen off.)
        sx_screen = sx_b
        sy_screen = (sy_b - 24) & 0xFFFF
        sprite_list.append({
            "offs": offs,
            "x": sx_screen,
            "y": sy_screen,
            "tile": base + base_local,            # absolute VRAM tile index
            "flipH": flipx,
            "flipV": flipy,
            "code": code,
            "bank": bank,
            "raw_x": sx_b,
            "raw_y": sy_b,
        })

    return pack, nametable, sprite_list, n_char_only_tiles


# ---------------------------------------------------------------------------
# TILE_ATTR_FULL replica (SGDK macro), for emitting nametable words offline
# so we can sanity-check; the emitted .c uses the real macro.
#   TILE_ATTR_FULL(pal, prio, flipV, flipH, index) =
#     (index) | (flipH<<11) | (flipV<<12) | (pal<<13) | (prio<<15)
# ---------------------------------------------------------------------------
def tile_attr_full(pal, prio, flipV, flipH, index):
    return ((index & 0x7FF) | ((flipH & 1) << 11) | ((flipV & 1) << 12) |
            ((pal & 3) << 13) | ((prio & 1) << 15))


# ---------------------------------------------------------------------------
# Emit res/ss_gfx.c + inc/ss_gfx.h
# ---------------------------------------------------------------------------
def emit_c_h(roms, pack, nametable, sprite_list, n_char_only_tiles,
             base, scene, out_c, out_h):
    n = pack.total_tiles

    # --- header ---
    with open(out_h, "w", newline="\n") as f:
        f.write("""/* ss_gfx.h -- GENERATED by tools/gen_assets.py.  DO NOT EDIT.
 *
 * SonSon (Capcom, 1984) ONE captured arcade gameplay scene, baked to native
 * Genesis 4bpp tiles + the two fixed arcade palettes. See tools/PIPELINE_SPEC.md
 * (authoritative) and tools/scene_capture.json (the raw dump this was built
 * from). Palettes are VERBATIM from the verified port (spec 5.4).
 */
#ifndef SS_GFX_H
#define SS_GFX_H

#include <genesis.h>

""")
        f.write("/* Captured scene: arcade frame %d, md=%d (01=attract demo gameplay). */\n"
                % (scene["frame"], scene["md"]))
        f.write("#define SS_SCENE_FRAME      %d\n" % scene["frame"])
        f.write("\n")
        f.write("/* VRAM tile index the render must load ss_scene_tiles[] at. The\n"
                " * nametable words + sprite tile indices below are ABSOLUTE (they\n"
                " * already include this base). */\n")
        f.write("#define SS_SCENE_TILE_BASE  %d\n" % base)
        f.write("\n")
        f.write("/* Total baked 8x8 tiles (chars first, then sprite tiles). */\n")
        f.write("#define SS_SCENE_TILE_COUNT %d\n" % n)
        f.write("#define SS_SCENE_CHAR_TILE_COUNT %d   /* [0..this) are char tiles */\n"
                % n_char_only_tiles)
        f.write("#define SS_SCENE_SPRITE_COUNT %d\n" % len(sprite_list))
        f.write("\n")
        f.write("/* Arcade palettes (16 colors each, VDP words). PAL0 = chars,\n"
                " * PAL1 = sprites (slot 15 == slot 0 == black, by design). */\n")
        f.write("extern const u16 ss_pal_char[16];\n")
        f.write("extern const u16 ss_pal_sprite[16];\n")
        f.write("\n")
        f.write("/* Baked 4bpp Genesis tiles: SS_SCENE_TILE_COUNT tiles, 8 u32 each\n"
                " * (one 8px row per long, high nibble = leftmost pixel). Char tiles\n"
                " * occupy indices [0..SS_SCENE_CHAR_TILE_COUNT); sprite tiles follow\n"
                " * in groups of 4 (column-major: TL,BL,TR,BR). */\n")
        f.write("extern const u32 ss_scene_tiles[SS_SCENE_TILE_COUNT * 8];\n")
        f.write("\n")
        f.write("/* One VDP nametable word per 32x32 BG cell (row-major):\n"
                " * TILE_ATTR_FULL(PAL0,0,0,0, SS_SCENE_TILE_BASE+localidx).\n"
                " * Blank/space cells resolve to their baked tile (often tile 0). */\n")
        f.write("extern const u16 ss_scene_nametable[32 * 32];\n")
        f.write("\n")
        f.write("/* One entry per active hardware sprite (24), arcade slot order\n"
                " * offs 0,4,..,0x5C (front-to-back). Coordinates are SCREEN coords\n"
                " * PRE the SAT +0x80 bias: x = arcade raster x; y = arcade y - 24\n"
                " * (the port's playfield mapping). Parked sprites have raw y=248.\n"
                " * `tile` is the ABSOLUTE base (top-left) VRAM tile index; the four\n"
                " * 8x8 tiles are column-major from there. flip_screen is OFF in this\n"
                " * captured frame, so flipH/flipV are the decoded (inverted) bits\n"
                " * directly. */\n")
        f.write("typedef struct {\n")
        f.write("    s16 x;       /* screen x (add 0x80 for the SAT word)          */\n")
        f.write("    s16 y;       /* screen y = arcade y - 24 (add 0x80 for SAT)   */\n")
        f.write("    u16 tile;    /* absolute base (TL) tile index; SPRITE_SIZE(2,2)*/\n")
        f.write("    u8  flipH;\n")
        f.write("    u8  flipV;\n")
        f.write("} SSSprite;\n")
        f.write("extern const SSSprite ss_scene_sprites[SS_SCENE_SPRITE_COUNT];\n")
        f.write("\n#endif /* SS_GFX_H */\n")

    # --- source ---
    with open(out_c, "w", newline="\n") as f:
        f.write("/* ss_gfx.c -- GENERATED by tools/gen_assets.py.  DO NOT EDIT.\n")
        f.write(" *\n")
        f.write(" * PROVENANCE: native bake of ONE captured SonSon arcade gameplay\n")
        f.write(" * scene (tools/scene_capture.json: arcade frame %d, md=%d).\n"
                % (scene["frame"], scene["md"]))
        f.write(" *   char tiles  <- ss_7.b6 (LSB) + ss_8.b5 (MSB), 2bpp, via\n")
        f.write(" *                  ssb2.c4 char LUT (pen+bank -> PAL0 index)\n")
        f.write(" *   sprite tiles<- ss_9..ss_14 (3bpp, LEFT/RIGHT split, identity)\n")
        f.write(" *                  via ssb3.h7 sprite LUT (pen0 transparent;\n")
        f.write(" *                  LUT-0 opaque -> slot 15)\n")
        f.write(" *   palettes    <- VERBATIM from the verified port (spec 5.4)\n")
        f.write(" *\n")
        f.write(" * Baked working set: %d char tiles + %d sprite tiles = %d total\n"
                % (n_char_only_tiles, n - n_char_only_tiles, n))
        f.write(" * (%d bytes VRAM). Tile base = %d.\n" % (n * 32, base))
        f.write(" */\n")
        f.write('#include "ss_gfx.h"\n\n')

        # palettes (verbatim)
        f.write("const u16 ss_pal_char[16] = {\n    ")
        f.write(",".join("0x%04X" % w for w in SS_PAL_CHAR))
        f.write("\n};\n\n")
        f.write("const u16 ss_pal_sprite[16] = {\n    ")
        f.write(",".join("0x%04X" % w for w in SS_PAL_SPRITE))
        f.write("\n};\n\n")

        # baked tiles
        f.write("const u32 ss_scene_tiles[SS_SCENE_TILE_COUNT * 8] = {\n")
        for ti in range(n):
            longs = pack.tiles[ti * 8: ti * 8 + 8]
            kind = "char" if ti < n_char_only_tiles else "spr"
            f.write("    /* tile %4d (%s) */ %s,\n"
                    % (ti, kind, ",".join("0x%08X" % w for w in longs)))
        f.write("};\n\n")

        # nametable
        f.write("const u16 ss_scene_nametable[32 * 32] = {\n")
        for row in range(32):
            chunk = nametable[row * 32: row * 32 + 32]
            f.write("    " + ",".join("0x%04X" % w for w in chunk) +
                    ",  /* row %2d */\n" % row)
        f.write("};\n\n")

        # sprite list
        f.write("const SSSprite ss_scene_sprites[SS_SCENE_SPRITE_COUNT] = {\n")
        for s in sprite_list:
            # x/y emitted as signed screen coords; cast to s16 range
            sx = s["x"] if s["x"] < 32768 else s["x"] - 65536
            sy = s["y"] if s["y"] < 32768 else s["y"] - 65536
            f.write("    { %4d, %4d, %4d, %d, %d },"
                    " /* offs %2d code %3d bank %2d raw(x=%d,y=%d) */\n"
                    % (sx, sy, s["tile"], s["flipH"], s["flipV"],
                       s["offs"], s["code"], s["bank"], s["raw_x"], s["raw_y"]))
        f.write("};\n")


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def main(argv):
    scene_path = os.path.join(HERE, "scene_capture.json")
    arcade_dir = os.path.join(REPO, "arcade")
    out_c = os.path.join(REPO, "res", "ss_gfx.c")
    out_h = os.path.join(REPO, "inc", "ss_gfx.h")
    base = 0

    # tiny arg parse (stdlib only)
    args = list(argv)
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--arcade":
            arcade_dir = args[i + 1]; i += 2
        elif a == "--out-c":
            out_c = args[i + 1]; i += 2
        elif a == "--out-h":
            out_h = args[i + 1]; i += 2
        elif a == "--base":
            base = int(args[i + 1]); i += 2
        elif a.startswith("--"):
            sys.exit("unknown option %s" % a)
        else:
            scene_path = a; i += 1

    roms = Roms(arcade_dir)
    for w in roms.pal_warnings:
        sys.stderr.write("WARN palette provenance: %s\n" % w)

    with open(scene_path) as f:
        scene = json.load(f)

    pack, nametable, sprite_list, n_char_only = assemble(roms, scene, base)

    os.makedirs(os.path.dirname(out_c), exist_ok=True)
    os.makedirs(os.path.dirname(out_h), exist_ok=True)
    emit_c_h(roms, pack, nametable, sprite_list, n_char_only, base, scene,
             out_c, out_h)

    n = pack.total_tiles
    print("gen_assets: scene frame %d (md=%d)" % (scene["frame"], scene["md"]))
    print("  char (code,bank) used : %d -> %d distinct 8x8 tiles (deduped)"
          % (len(pack.char_map), pack.n_char_tiles))
    print("  sprite (code,bank) used: %d -> %d 16x16 (= %d 8x8 tiles, deduped)"
          % (len(pack.spr_map),
             sum(1 for _ in pack._spr_dedup), pack.n_spr_tiles))
    print("  TOTAL baked 8x8 tiles : %d  (%d bytes VRAM, %.0f%% of 2048)"
          % (n, n * 32, 100.0 * n / 2048))
    print("  tile base index       : %d" % base)
    print("  sprite list entries   : %d (24 hardware slots)" % len(sprite_list))
    print("  wrote", out_c)
    print("  wrote", out_h)
    if roms.pal_warnings:
        print("  palette provenance WARNINGS:", len(roms.pal_warnings))
    else:
        print("  palette provenance: OK (matches verbatim except 6 hand-tuned)")


if __name__ == "__main__":
    main(sys.argv[1:])

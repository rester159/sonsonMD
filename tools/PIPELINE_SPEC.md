# SonSon Native — Build-Time Asset Pipeline Spec (Phase 1)

**Status:** SPEC ONLY (do not implement yet). Authoritative for the Phase-1 asset tool.
**Goal:** convert Capcom SonSon (1984) arcade graphics + indirect-palette PROMs into
native Sega Genesis 4bpp tiles + fixed CRAM palettes, **once at build time**, so the
native game writes the VDP nametable / sprite-attribute-table directly with zero
per-frame conversion (the "translation tax" the sibling transliteration port pays).

**Authoritative sources used to write this spec (all read + cross-checked):**
- `../../sonson/port/src/video.c` — the VERIFIED-CORRECT runtime converter
  (`build_char`, `build_sprite`, `cache_get`, `video_init`). Renders the game right.
- `../../sonson/port/tools/convert_assets.py` — the existing offline *intermediate*
  decoder (arcade ROM/PROM → pen-packed C tables). Documents the bit layouts verbatim
  from MAME `sonson.cpp`. **NOTE its palette formula is superseded — see §5.4.**
- `../../sonson/port/src/gfx_assets.{c,h}` — the generated tables the runtime consumes
  (`ss_char_px`, `ss_sprite_px`, `ss_char_lut`, `ss_sprite_lut`, `ss_pal_char`,
  `ss_pal_sprite`). The palette arrays here are the eyeball-verified ground truth.
- MAME `sonson` driver: only the **binary** is present (`D:/portForge/tools/mame/mame.exe`,
  no C++ source tree). The layouts are transcribed in `convert_assets.py`'s header and
  re-verified against the port's on-screen-correct decode → **the port is authoritative**;
  MAME is used only as a live pixel/color oracle (see §6).
- Live evidence: two headless MAME `sonson` arcade traces (`tileuse_ss.lua`,
  `tileuse_ss2.lua`, both in `D:/portForge/tools/mame/`) measuring real per-frame and
  global tile/bank usage. Results in §4.

---

## 1. Where the arcade bytes physically live

The arcade graphics are **NOT** in any port `romdata.c` blob. `romdata.c` holds only the
6809 *program* image (`rom_image[0xC000]`, the 0x4000–0xFFFF code area). The gfx/PROM
bytes exist only as:
1. the **raw arcade files** (the offline tool's input), present in two copies:
   - `D:/genesis ports/_capcom/son son/sonson_native/arcade/`
   - `D:/genesis ports/_capcom/son son/rom/`  (what `convert_assets.py` reads today)
2. the **decoded** form baked into `../../sonson/port/src/gfx_assets.c`.

The native tool reads the **raw arcade files directly** (sizes asserted as a provenance
check). Recommended input dir: `sonson_native/arcade/` (self-contained for this project).

### File roster (sizes verified)
| File | Size | Role |
|------|------|------|
| `ss_6.c11` | 8 KB | **UNUSED for video.** (Char ROM slot; see note.) |
| `ss_7.b6` | 8 KB | Char plane 0 (LSB) |
| `ss_8.b5` | 8 KB | Char plane 1 (MSB) |
| `ss_9.m5` + `ss_10.m6` | 8 KB ea | Sprite plane 0 (LSB), concatenated → 16 KB |
| `ss_11.m3` + `ss_12.m4` | 8 KB ea | Sprite plane 1, concatenated → 16 KB |
| `ss_13.m1` + `ss_14.m2` | 8 KB ea | Sprite plane 2 (MSB), concatenated → 16 KB |
| `ssb4.b2` | 32 B | Color PROM: **G** (hi nibble) + **B** (lo nibble) of each of 32 hw colors |
| `ssb5.b1` | 32 B | Color PROM: **R** (lo nibble) of each of 32 hw colors |
| `ssb2.c4` | 256 B | **CHAR color LUT** (64 banks × 4 pens) — the indirect palette |
| `ssb3.h7` | 256 B | **SPRITE color LUT** (32 banks × 8 pens) — the indirect palette |
| `ssb1.k11` | 256 B | Timing PROM — **UNUSED for video** (do not load) |
| `ss.01e/02e/03e` | 16 KB ea | 6809 program — not graphics |

> **`ss_6.c11` note:** The header comments in `gfx_assets.h`/ROADMAP loosely say "chars =
> `ss_6..ss_9`", but the verified decoder (`convert_assets.py` + `build_char`) uses **only
> `ss_7.b6` (LSB) and `ss_8.b5` (MSB)** for the 1024 char tiles. `ss_6.c11` is not consumed.
> Treat chars as a **2-plane (2bpp)** decode from `ss_7`+`ss_8`. (If a future audit shows
> `ss_6` is a third char plane in some region, revisit — but the port renders correctly
> without it, so it is out of scope for parity.)

> **PROM concatenation order** (MAME `proms` region, for reference): `ssb4.b2`(0x00) +
> `ssb5.b1`(0x20) + `ssb2.c4`(0x40) + `ssb3.h7`(0x140) + `ssb1.k11`(0x240). The tool reads
> the individual files, so this only matters if cross-checking a MAME memory dump.

---

## 2. The conversion algorithm (exact)

### 2.1 CHAR tiles — geometry & pixel decode
- **Geometry:** 8×8 pixels, **2 bitplanes** → pen value **0..3**. 1024 tiles.
- **ROM layout** (`gfx_8x8x2_planar`): 8 bytes per plane per tile. For tile `t`, row `r`
  (0..7): `p0 = ss7[t*8 + r]`, `p1 = ss8[t*8 + r]`. **Bit 7 = leftmost pixel.**
- **Pen:** for column `c` (0..7), `bit = 7-c`,
  `pen = ((p1>>bit)&1)<<1 | ((p0>>bit)&1)`  (ss_8 = MSB, ss_7 = LSB).

### 2.2 SPRITE tiles — geometry & pixel decode
- **Geometry:** 16×16 pixels, **3 bitplanes** → pen value **0..7**. 512 tiles.
- **ROM layout** (`spritelayout`, RGN_FRAC(1,3), planes {2/3,1/3,0/3}): 32 bytes per plane
  per tile. Each plane is two 8 KB ROMs concatenated:
  `spr_p0 = ss_9 ++ ss_10` (LSB), `spr_p1 = ss_11 ++ ss_12`, `spr_p2 = ss_13 ++ ss_14` (MSB).
- **The split-column quirk (critical, from MAME x-offsets `{8*16+7..8*16+0, 7..0}`):**
  for tile `t`, row `r` (0..15):
  - `bytes[t*32 + r]`      = the **RIGHT** 8px (screen cols 8..15), MSB = col 8
  - `bytes[t*32 + 16 + r]` = the **LEFT**  8px (screen cols 0..7),  MSB = col 0
- **Pen:** for col `c` (0..15): if `c<8` use the LEFT triple at `bit=7-c`, else the RIGHT
  triple at `bit=7-(c-8)`; `pen = ((p2>>bit)&1)<<2 | ((p1>>bit)&1)<<1 | ((p0>>bit)&1)`.
- **Pen order is IDENTITY** — no bit-reversal. (A historical rev3 bit-reversal was a bug;
  ground-truthed against a live arcade screenshot, identity matches 615/615 opaque pixels.
  Do not reintroduce any pen permutation.)

> The mirror-within-each-8px-half is the same transform `build_sprite` applies as
> `sx = (sx0 & 8) | (7 - (sx0 & 7))` when reading from the *contiguous-packed* `ss_sprite_px`.
> If the native tool decodes **directly from the raw ROMs** using the LEFT/RIGHT byte rule
> above (§2.2), it gets correctly-oriented pixels with no extra mirror step. Either path
> yields identical output — prefer decoding straight from raw ROMs (simpler, matches MAME).

### 2.3 The INDIRECT PALETTE (the heart of the conversion)
The arcade does **two** lookups to get a final color; the Genesis can do neither in HW,
so we bake both offline:

**Step A — pen → palette index (the LUT / "colorbank"):**
- Each on-screen cell/sprite carries a **colorbank** (chars: 0..63; sprites: 0..31).
- `char_lut[bank*4 + pen]  = ssb2.c4[bank*4 + pen] & 0x0F`  → a 0..15 index into PAL0.
- `sprite_lut[bank*8 + pen] = ssb3.h7[bank*8 + pen] & 0x0F` → a 0..15 index into PAL1.
- (Upstream MAME OR's `0x10` onto sprite indices to address hw colors 16..31; we keep the
  **PAL1-relative 0..15** index and put the sprite palette in PAL1.)

**Step B — palette index → RGB (the color PROMs):** 32 hardware colors total.
For hw color `i` (0..31):
- `R = DAC(ssb5.b1[i] & 0x0F)`            (R is the low nibble of ssb5)
- `G = DAC((ssb4.b2[i] >> 4) & 0x0F)`     (G is the high nibble of ssb4)
- `B = DAC(ssb4.b2[i] & 0x0F)`            (B is the low nibble of ssb4)
- `DAC(nibble) = 0x0E*b0 + 0x1F*b1 + 0x43*b2 + 0x8F*b3` → an 8-bit-ish value 0..223
  (the MAME "Commando" resistor-DAC approximation).

So **hw colors 0..15 are the char palette, hw colors 16..31 are the sprite palette.**
The colorbank only ever *remaps which of those 16 colors a pen shows* — it never
introduces a color outside the fixed 16. (Proven in §4 — this is what makes it fit.)

### 2.4 Final Genesis 4bpp tile build (what the runtime does, baked offline)
For a CHAR cell `(code, bank)` (`build_char`):
- For each of 8 rows, each of 8 pixels: `genesis_nibble = char_lut[bank*4 + pen] & 0xF`.
- Pack a Genesis 4bpp tile: 8 longwords, one row each, **high nibble = leftmost pixel**
  (`w = (w<<4) | nibble`).

For a SPRITE `(code, bank)` (`build_sprite`), with **transparency rule**:
- 16×16 → **4 Genesis 8×8 tiles** in **column-major** order:
  `gtile = tcol*2 + trow` (tile0 TL, tile1 BL, tile2 TR, tile3 BR).
- Per pixel: `pen` from §2.2, then
  - `if pen == 0:` Genesis pixel = **0** (hardware-transparent), *even if* the LUT maps
    pen 0 to a non-black color. (Arcade sprite transparency is PRE-PROM: only pen 0.)
  - `else:` `gp = sprite_lut[bank*8 + pen] & 0xF; if gp == 0: gp = 15;` — i.e. an opaque
    pen whose LUT output is color 0 (black) must NOT become Genesis pixel 0 (transparent);
    it is remapped to **slot 15**, which holds a duplicate black (`ss_pal_sprite[15]==[0]`).
    This is what makes opaque text boxes (READY/GAME OVER) not see-through.
- Pack each row hi-nibble-first as for chars.

> **CHAR transparency:** chars use the LUT output directly (no pen-0 special case). Pen 0
> of a char typically maps to color 0 (black backdrop) via its bank; the BG plane shows
> it as background. The native BG present sets `VDP_setBackgroundColor(PAL0*16+0)`.

### 2.5 The on-screen attribute decode (for the game code, not the asset tool, but the
tool's tile *indexing* must match it)
From `video.c` / MAME `sonson.cpp`:
- **BG cell** `i`: `v = videoram[i]`, `a = colorram[i]`;
  `code = v + 256*(a & 0x03)` (0..1023), `bank = a >> 2` (0..63).
- **Sprite** at `spriteram` offs (0x00..0x5C step 4):
  `code = byte2 + ((byte1 & 0x20) << 3)` (0..511), `bank = byte1 & 0x1F` (0..31),
  `flipx = (byte1 & 0x40) ? 0 : 1` (**inverted**), `flipy = (byte1 & 0x80) ? 0 : 1`,
  `sx = byte3`, `sy = byte0`. Drawn back-to-front offs 0x5C→0x00.
- These are the keys the asset tool must be able to satisfy: the baked tile for
  `(code,bank)` must be addressable when the game writes that cell/sprite.

---

## 3. Asset counts (measured)

### 3.1 Raw tile inventory (from the ROMs)
- **CHAR:** 1024 tiles total; **964 non-blank**, 60 all-blank (unreferenced slots — one,
  code 0x9F, is repurposed by the front-end as a synthetic comma glyph).
- **SPRITE:** 512 tiles (16×16) total; **483 non-blank**, 29 all-blank.

### 3.2 Color banks
- **CHAR banks:** 64 (`ssb2.c4`, 4 pens each). Across all 64 banks the LUT references the
  full set of PAL0 indices **{0..15}** — i.e. nothing outside the 16-color char palette.
- **SPRITE banks:** 32 (`ssb3.h7`, 8 pens each). Across all 32 banks the LUT references the
  full set of PAL1 indices **{0..15}** — nothing outside the 16-color sprite palette.

### 3.3 Distinct colors (arcade → Genesis 9-bit)
- 32 hw colors decoded. After Genesis 3-bit-per-channel quantization there are **27
  distinct** colors among the 32 (5 collisions from the down-quantization).
  - Char half (hw 0..15): **16 distinct** after quant.
  - Sprite half (hw 16..31): **15 distinct** (hw16 `(0,0,0)` and hw31 `(14,14,14)` both
    quantize to `0x0000`; and slot 15 already duplicates black by design).
- **Color loss:** arcade PROM is 4 bits/channel (16 levels via the resistor DAC); Genesis
  CRAM is 3 bits/channel (8 levels). The mapping is ≈ `round(nibble*7/15)`. This is a real
  but minor banding loss; the committed palette additionally **hand-tunes 6 of 32 entries**
  away from the pure formula (see §5.4) — those hand values are eyeball-verified vs arcade.

### 3.4 The cross-product explosion (why naïve pre-bake is impossible)
Because the colorbank changes a tile's pixel→color mapping, the *same* arcade tile code
needs a *different* Genesis tile per bank. The full cross-product, after de-duping
visually-identical results, is:
- CHAR: 1024×64 = 65,536 combos → **42,440 distinct** resolved 8×8 Genesis tiles.
- SPRITE: 512×32 = 16,384 combos → **11,843 distinct** 16×16 (= **47,372** 8×8 tiles).

**~90,000 tiles ≫ the Genesis's 2048-tile VRAM.** You CANNOT statically bake every
combination. This is exactly why the transliteration port uses a runtime cache. **The
native port must instead bake only the combinations actually used** (§4 + §5).

---

## 4. THE KEY ANALYSIS — palette fitting

### 4.1 Verdict: **YES, it fits 4×16 trivially — with room to spare.**
The arcade's "indirect palette" is **not** a free-form 256-color system. Every LUT entry
(char and sprite) is an index **0..15 into one of two fixed 16-color hardware palettes**:
- **hw colors 0..15 = the char palette** (every one of the 64 char banks indexes only here)
- **hw colors 16..31 = the sprite palette** (every one of the 32 sprite banks indexes only here)

The colorbank is a *remap within those 16 colors*, never a new color set. Therefore:

| Genesis palette | Contents | Covers |
|---|---|---|
| **PAL0** | arcade hw colors 0..15 (`ss_pal_char`) | **ALL 64 char banks** |
| **PAL1** | arcade hw colors 16..31 (`ss_pal_sprite`) | **ALL 32 sprite banks** |
| **PAL2** | free | HUD / front-end / future |
| **PAL3** | free | free |

This is exactly what the verified runtime does (`video_init`:
`PAL_setColors(PAL0*16, ss_pal_char, 16); PAL_setColors(PAL1*16, ss_pal_sprite, 16);`).
**No per-level palette swap is needed. No per-line palette tricks. No color approximation
beyond the unavoidable 4-bit→3-bit channel quantization.** The "hard problem" the ROADMAP
flagged turns out to be benign *because the indirection is bounded to 16 colors per class.*

> The colorbank's flexibility is in **tile shape**, not color count: it lets one tile ROM
> code be re-colored many ways, all drawn from the same 16-color pool. On Genesis we pay for
> that flexibility in **VRAM tiles** (one baked tile per used (code,bank)), not in palettes.

### 4.2 The real constraint is VRAM tiles — and that ALSO fits (measured)
Two headless MAME arcade traces measured live usage (attract + the built-in gameplay demo,
~50,000 frames; scripts: `D:/portForge/tools/mame/tileuse_ss{,2}.lua`):

| Metric | Value |
|---|---|
| **Peak SIMULTANEOUS** char `(code,bank)` in one frame | **~203** distinct tiles |
| Peak simultaneous char *banks* in one frame | 14 |
| Peak SIMULTANEOUS sprite `(code,bank)` in one frame | ~16 (→ 64 8×8 tiles) |
| Peak simultaneous sprite *banks* in one frame | 6 |
| **GLOBAL** distinct char `(code,bank)` over whole run | **~322–325** |
| Global distinct char *codes* | 226 |
| Global distinct char banks | 25 (of 64) |
| **GLOBAL** distinct sprite `(code,bank)` over whole run | **~66–68** |
| Global distinct sprite codes | 66 |
| Global distinct sprite banks | 12 (of 32) |

The global counts **saturated** (flat from ~frame 10,000 to 50,000). VRAM budget if we
statically bake the global working set:
- char: ~325 tiles × 32 B = ~10 KB
- sprite: ~68 × 4 tiles = 272 tiles × 32 B = ~8.5 KB
- **Total ≈ 597 tiles ≈ 19 KB ≈ 29% of the 64 KB / 2048-tile VRAM.** Comfortable.

> **Caveat / one open data point:** the coin-insert in `tileuse_ss2.lua` did not flip the
> game to `md=02` (the demo coinage path differs), so these numbers come from the **attract
> demo's gameplay**, which exercises the early stage(s) but may not visit *every* level. The
> true all-levels totals will be **somewhat higher** than 325/68. Even so, the cross-product
> is 65,536/16,384, the *full distinct* resolved set is 42k/47k, and *simultaneous* peak is
> ~203 — there is ~3–10× headroom before any VRAM pressure. **Recommendation in §5.3.**

### 4.3 Recommended Genesis palette assignment (concrete)
- `PAL0` ← `ss_pal_char` (16 words, copy verbatim from `gfx_assets.c` — see §5.4).
- `PAL1` ← `ss_pal_sprite` (16 words, verbatim).
- Background color = `PAL0` index 0 (`VDP_setBackgroundColor(PAL0*16+0)`).
- BG nametable entries: `TILE_ATTR(PAL0, prio=0, flip=0, tileindex)`.
- Sprites: `TILE_ATTR(PAL1, prio, flipy, flipx, tileindex)`; side-mask strips use PAL1
  slot 15 (opaque black).
- `PAL2`/`PAL3`: reserve for the native HUD / front-end (the home-mode splash, score font)
  so the game art never competes for them. Not required for arcade parity.

---

## 5. Recommended pipeline implementation

### 5.1 Language & form
**Host Python 3 script** (`sonson_native/tools/gen_assets.py`), deterministic, stdlib-only —
mirroring the proven `convert_assets.py`. Rationale: the bit-twiddling is identical to a
script that already works; Python keeps it readable and the run is a few seconds. A host C
program is unnecessary (no perf need at build time).

### 5.2 Output format — **two-tier, recommendation: raw C blobs (Tier B) as primary**
The colorbank-per-tile model does **not** fit SGDK `rescomp`'s clean "one image → one
palette" abstraction (rescomp can't express the indirect remap), so:

- **Tier A (optional, for eyeballing only):** emit PNGs of the decoded tilesheets +
  per-bank recolors, for visual diff vs arcade (Phase-1 verify step). Not consumed by the
  build.
- **Tier B (primary, consumed by the build):** emit a C source/header pair, e.g.
  `res/ss_gfx.c` / `inc/ss_gfx.h`, containing:
  - `ss_pal_char[16]`, `ss_pal_sprite[16]` (VDP words, **copied verbatim** from the
    verified `gfx_assets.c` — see §5.4).
  - A **baked tile array** of final Genesis 4bpp tiles for the chosen working set
    (§5.3), `u32 ss_tiles[N*8]`, 4bpp, hi-nibble-left, sprite tiles in column-major order.
  - An **index map**: given `(code,bank)` → base tile index. Two reasonable encodings:
    1. **Dense static tables** if we bake the full *used* set: `u16 char_tileidx[code][bank]`
       is too big (1024×64); instead emit a **sorted key list** `(packed_key → tileidx)`
       the game binary-searches, OR
    2. **Per-level packs** (§5.3): each level/scene gets its own small contiguous tile
       blob + a small `(code,bank)→localindex` table, uploaded on level load.

  Recommendation: **per-scene packs** (§5.3) — smallest VRAM, simplest indexing, and it
  matches how a native game naturally streams a level's tileset.

### 5.3 Working-set strategy (the important design choice)
Do **not** bake the 90k cross-product. Bake only used `(code,bank)` combos, partitioned by
scene. Concretely:

1. **Capture the per-level working set** with a MAME trace (extend `tileuse_ss2.lua`):
   for each level/region, record the set of char `(code,bank)` and sprite `(code,bank)`
   actually drawn. Drive real gameplay (fix the coin path — see §6) so **every** level is
   visited; dump one set per level.
2. **Pipeline bakes one tile pack per level** (deduped within the pack): the union of that
   level's used combos. From §4.2, a single scene needs only ~200 char + ~64 sprite tiles,
   so a level pack is ~300–400 tiles (~10–12 KB) — fits VRAM many times over, leaving room
   for double-buffering / streaming.
3. **Shared pack:** a small always-resident pack for the HUD/score/common glyphs +
   ubiquitous sprites (player), so level transitions only swap the level-specific remainder.
4. **Indexing:** within a pack, emit `(code,bank)→local tile index` as a compact sorted
   table (the game does a tiny lookup when it writes a cell/sprite — far cheaper than the
   port's per-frame *re-resolve*, because the **pixels are already baked**; this is just an
   index translation, O(log n) or a perfect-hash, no PROM math, no DMA of fresh tiles).

> If a future measurement shows the **whole game** fits in 2048 tiles (plausible: full
> distinct *used* set across all levels is likely ≲ 1000–1500 given the demo saturated at
> 325/68 on the early stages), prefer **one global pack** baked once into VRAM at boot — no
> streaming at all. Decide after the all-levels capture (§6). Start with per-level packs;
> collapse to global if it fits.

### 5.4 Palette: REUSE the committed arrays, do NOT regenerate from the DAC formula
**Important gotcha found while writing this spec.** The DAC formula documented in
`convert_assets.py` (§2.3) does **not** reproduce the palette arrays actually shipped in
`gfx_assets.c`. The committed `ss_pal_char`/`ss_pal_sprite`:
- match `round(nibble * 7 / 15)` for **26 of 32** entries, and
- are **hand-deviated for 6 entries** (hw 5, 8, 9, 10, 11, 26 — e.g. hw8 raw G/B `(4,6)`
  ships as 3-bit `(1,2)` not the formula's `(2,3)`).

These deviations were evidently tuned against arcade screenshots and are part of why the
port "looks right." **Therefore the native tool must copy the committed VDP words verbatim**
rather than recompute them. The exact arrays (paste these):

```c
/* PAL0 — char palette (arcade hw colors 0..15), VDP words, verified on-screen */
const u16 ss_pal_char[16] = {
    0x0000,0x0888,0x0080,0x0226,0x0228,0x004A,0x0AAA,0x0666,
    0x0420,0x0842,0x0424,0x0848,0x00AA,0x0A8A,0x0C66,0x000C
};
/* PAL1 — sprite palette (arcade hw colors 16..31) */
const u16 ss_pal_sprite[16] = {
    0x0000,0x0CCC,0x0888,0x068C,0x000A,0x0006,0x006A,0x00AC,
    0x000E,0x0808,0x0A04,0x0C66,0x006E,0x0080,0x00A0,0x0000
};
```

(Source: `../../sonson/port/src/gfx_assets.c` lines 5173/5175. Note slot 15 of PAL1 ==
slot 0 == black, by design — the opaque-black duplicate.)

The LUTs (`ss_char_lut[256]`, `ss_sprite_lut[256]`, each `&0x0F`) CAN be regenerated from
`ssb2.c4`/`ssb3.h7` (deterministic, verified). They are needed by the **tool** (to bake
tiles) but need NOT ship in the ROM (the baked tiles already have final indices).

### 5.5 Tool steps (implement in this order)
1. Load + size-assert the raw arcade files from `sonson_native/arcade/` (§1 roster).
2. Decode raw pen buffers: `char_pen[1024][8][8]` (0..3), `spr_pen[512][16][16]` (0..7),
   using §2.1 / §2.2 exactly (decode sprites straight from raw ROMs with the LEFT/RIGHT
   byte rule — no separate mirror pass).
3. Build LUTs: `char_lut[256] = ssb2.c4 & 0x0F`, `sprite_lut[256] = ssb3.h7 & 0x0F`.
4. Take the palette arrays **verbatim** (§5.4). (Optionally also compute the DAC values and
   *assert* they match the committed arrays except the 6 known hand-tuned entries, as a
   provenance/regression check — warn, don't fail, on the known 6.)
5. Read the per-level used-combo sets (from the MAME capture, §6). For each pack:
   - For each used char `(code,bank)`: build the final 4bpp 8×8 tile (§2.4 char). Dedupe
     identical tiles within the pack. Assign a local index.
   - For each used sprite `(code,bank)`: build the 4 column-major 4bpp tiles with the
     transparency rule (§2.4 sprite). Dedupe 16×16 results. Assign a local base index.
   - Emit the pack's tile blob + `(code,bank)→local index` table.
6. Emit `res/ss_gfx.c` + `inc/ss_gfx.h`: palettes, the pack blobs, the index tables, and
   `#define`s for counts. Provide a tiny runtime helper signature, e.g.
   `u16 ss_char_tile(u16 code, u8 bank)` / `u16 ss_sprite_tile(u16 code, u8 bank)` returning
   the VRAM-relative tile index for the currently-loaded pack.
7. Print a report: per-pack tile counts, total VRAM bytes, and assert ≤ the VRAM tile
   budget reserved for art.

### 5.6 Verification (Phase-1 exit criteria)
- **Counts:** every pack ≤ its VRAM allotment; palettes are exactly 2×16 (§4.1). (Done in
  the tool; this spec already proves the working set fits — §4.2.)
- **Pixels:** dump baked tiles to PNG (Tier A) and diff against a MAME arcade screenshot of
  the same scene. The runtime port already proves the math; this guards the *offline* rebake.
- **Cross-check vs the port:** for a handful of `(code,bank)` pairs, assert the tool's baked
  4bpp tile bytes equal what `build_char`/`build_sprite` produce (lift those two functions
  into a tiny host harness, or compare against a one-off dump from the port). Byte-equality
  here = the offline bake is faithful to the verified runtime.

---

## 6. MAME oracle / capture notes (for the per-level working-set step)
- MAME here is **binary-only** (`D:/portForge/tools/mame/mame.exe`); no C++ source. Use it
  as a live oracle, not a source reference. The driver name is `sonson` (arcade), distinct
  from the port's `genesis -cart` runs. **Arcade `sonson` is NTSC-native here** (unlike the
  `megadriv`/`genesis` PAL/NTSC gotcha noted in project memory — that applies to the
  Genesis port driver, not this arcade driver).
- Headless invocation pattern (proven in this analysis):
  `mame.exe sonson -nothrottle -video none -sound none -seconds_to_run N -autoboot_script X.lua -window -nomaximize`
- Arcade RAM addresses for tracing (maincpu program space): videoram `0x1000–0x13FF`,
  colorram `0x1400–0x17FF`, spriteram `0x2020–0x207F`, mode byte `md` at `0x0002`
  (01 = attract, 02 = in-game).
- Input port field names (verified via enumeration) for driving full gameplay:
  - `:SYSTEM` → `Coin 1`, `1 Player Start`, `Coin 2`, `2 Players Start`
  - `:P1` → `P1 Up/Down/Left/Right`, `P1 Button 1`
  - `:DSW1`/`:DSW2` → coinage/lives/difficulty (set coinage so `Coin 1` registers a credit).
- **Known issue to fix for all-levels capture:** in `tileuse_ss2.lua` the coin pulse did not
  advance to `md=02` (likely needs the DSW coinage set, or more/earlier coin frames, or the
  game consumes the coin only on a specific edge). Resolve this before the per-level capture
  so every level's tileset is recorded; the attract-demo numbers in §4.2 are a *lower bound*.
- Existing reusable tracers (this analysis): `D:/portForge/tools/mame/tileuse_ss.lua`
  (attract, peak + global usage) and `tileuse_ss2.lua` (coin-drive scaffold + summary).

---

## 7. Summary of decisions
- **Algorithm:** bake the indirect palette offline exactly as `build_char`/`build_sprite` do
  (§2). Chars = 2bpp from `ss_7`+`ss_8`; sprites = 3bpp from `ss_9..14` with the LEFT/RIGHT
  split, identity pens, pen-0 transparency + LUT-0→slot-15 opaque-black remap.
- **Palette fitting: SOLVED — fits in 2 of the 4 Genesis palettes** (PAL0 chars, PAL1
  sprites), because the colorbank only remaps within 16 fixed colors per class. No per-level
  palette swaps, no approximation beyond 4→3-bit channel quant. Copy the committed
  (hand-tuned, verified) palette arrays verbatim — do **not** regenerate from the DAC formula.
- **VRAM: SOLVED — bake only the used `(code,bank)` working set** (~300–600 tiles per scene,
  ~29% VRAM for the measured demo set), not the 90k cross-product. Per-level packs now;
  collapse to one global pack if the all-levels capture shows ≤ the art VRAM budget.
- **Tool:** deterministic Python, stdlib only, emitting C tile blobs + palettes + a small
  `(code,bank)→tileindex` map (plus optional PNGs for eyeball verification).

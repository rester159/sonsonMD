# SonSon Native — Roadmap

**Mission:** a from-scratch *native* Sega Genesis re-implementation of SonSon (Capcom, 1984) that runs at **full 60fps**, by removing the per-frame "translation tax" that caps the sibling transliteration port (`../sonson/port`) at ~87%.

**The core idea:** the arcade used an *indirect palette* (a PROM that maps tile-pixel + color-bank → color) and its own video memory formats. The Genesis can't do that lookup in hardware, so the transliteration port redoes it in software **every frame** (~7 of ~55 frame-budget units). Here we convert the graphics to native Genesis 4bpp tiles + fixed palettes **once at build time**, and the game writes the Genesis nametable / sprite-attribute-table **directly** — no per-frame scan, palette lookup, or reformatting.

---

## Phases (each ends with a concrete, verifiable deliverable)

### Phase 0 — Skeleton ✅ (in progress)
Standard SGDK project that builds + boots (shows a title text). Proves toolchain. → `out/rom.bin` boots in MAME `genesis`.

### Phase 1 — Asset pipeline (the keystone)
A build-time tool (`tools/`) that:
- decodes the arcade **char ROMs** (`arcade/ss_6..ss_9`) and **sprite ROMs** (`arcade/ss_10..ss_14`);
- applies the **indirect-palette PROMs** (`arcade/ssb1..ssb3` = RGB color PROMs; `ssb4/ssb5` = char/sprite color-lookup PROMs) to resolve real colors — the same math `build_char`/`build_sprite` do at runtime in the port, done ONCE here;
- **fits the result into the Genesis's 4×16-color palette budget** (the hard problem — analyze which color-banks are actually used together per scene; assign fixed Genesis palettes);
- emits SGDK resources (PNG + `.res`, or raw tile/palette blobs) into `res/`.
- **Verify:** dump the converted tiles to PNG and eyeball vs arcade; counts fit VRAM (≤2048 tiles) + palette (≤4×16) budgets.

### Phase 2 — Static render proof
Load a converted level + sprites and display them natively (hardware scroll for BG, direct SAT for sprites). No game logic yet — just prove a SonSon screen renders correctly at GF ~55.
- **Verify:** snapshot vs arcade screenshot; GF meter ~55 (full speed) with a full screen of tiles+sprites.

### Phase 3 — Core game loop & player
Native re-expression (from the RE: `census_semantic.toml`, `regions.jsonl`, the port's `sub_*.c` as behavioral reference) of: player movement, the SonSon climb/lane mechanic, gravity, the camera/scroll. Native structs, direct rendering.
- **Verify:** play the player around a static level at 60fps.

### Phase 4 — Entities, collision, scoring
Enemies + their AI (entity census: ~55 real archetypes), collision, fruit/items, score + popups. Native entity pool.
- **Verify:** an enemy-populated round plays correctly at 60fps.

### Phase 5 — Game flow
Attract/demo, round structure + progression, lives, extends, game-over, level data (`levels.json` / `levelcap.txt`).
- **Verify:** full round-to-round progression.

### Phase 6 — Audio
Music + SFX on the Z80 (XGM2 driver, or convert the arcade audio). Decouple from frame rate (lesson from the port).
- **Verify:** music/SFX play, correct tempo, no frame coupling.

### Phase 7 — Parity & perf
Side-by-side vs arcade (look + feel), tuning, and the headline: sustained **GF ~55 (60fps)** in busy gameplay.
- **Verify:** deterministic GF meter ~55 in the heaviest scenes; visual parity pass.

---

## The hard problem to watch: palette fitting
The arcade's indirect palette allows far more simultaneous color combinations than the Genesis's 4 palettes × 16 colors. The transliteration port hides this with a runtime tile cache. A native game must **pre-assign** palettes. Phase 1 must analyze real per-scene color-bank usage (the port's caches + MAME palette traces are evidence) and either fit within 4×16 or swap palettes per region/line. If a scene genuinely needs >64 colors, we approximate to the nearest faithful set. This is the main risk to "looks identical."

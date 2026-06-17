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

### Phase 8 — Presentation & front-end wrapper (owner directive, 2026-06-17)
The native build must ship the **full presentation**, ported from the existing verified front-end (already mostly native SGDK C — this is reuse + re-wiring, not invention). Boot flow:

**SEGA splash → CAPCOM logo intro → Arcade/Home selector ("PORTED BY RESTER159 2026") → [ Arcade: DIP-switch config → game | Home: title menu + options + credits → game ]**

- **SEGA opening** — reuse `../sonson/port/src/sega_logo.{c,h}` + `sega_splash.c` (Sonic-1 SEGA screen; `sega_logo_shinobi.*` = the Shinobi-logo splash v7 variant). Refs: sonic1-sega-splash-extraction, shinobi-sega-intro-extraction.
- **CAPCOM opening** — reuse `../sonson/port/src/capcom_logo.{c,h}` + `capcom_splash.c` (source kit `../../_capcom logo intro/`, register-exact FM jingle; gotcha: 68k→YM needs the Z80 bus held). Ref: capcom-intro-extraction.
- **Arcade/Home selector + RESTER credit** — reuse `../sonson/port/src/home.c` + `home_title_data.c` + `../sonson/port/FRONTEND_SPEC.md`. Ref: sonson-home-frontend.
- **Arcade mode** — expose the arcade DIP switches (lives / difficulty / bonus-life / coinage…) as a config screen; the native engine reads them as game parameters.
- **Home mode** — title menu + options + credits (the home front-end).
- **Wiring** — the native engine exposes a config struct (lives, difficulty, …) the DIP/options screens set; `main.c` runs the boot-flow state machine before entering the game loop. Port the *presentation* verbatim; re-point any "start the game" hook from the old transliteration loop to the native engine.
- **Verify:** boot → SEGA → CAPCOM → selector → both modes reach the native game; RESTER credit present; DIPs/options change params; all at 60fps.

---

## Palette fitting — SOLVED (Phase 1; see tools/PIPELINE_SPEC.md)
Resolved benignly: SonSon's indirect palette is a *remap within two fixed 16-color sets* — chars use hw colors 0-15 (→ PAL0), sprites 16-31 (→ PAL1). Two Genesis palettes cover all 96 banks; **PAL2/PAL3 stay free for the HUD + front-end**. No per-level swaps, no approximation beyond the unavoidable 4→3-bit channel quant. The indirection's real cost is VRAM tiles (one baked tile per used (code,bank)) — and the working set fits (~29% of VRAM).

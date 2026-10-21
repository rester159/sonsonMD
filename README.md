# SonSon MD

Ported by rester159, 2026.

This repository contains the native game source. **No ROMs or SEGA/CAPCOM intro modules, images or audio are included.** The source build starts at the mode selector; the local full build retains both boot intros. Generated native game graphics and sound tables are included so compiling the game does not require an arcade ROM. Regenerating those tables requires your own source data.

The owner’s local boot sequence includes about nine seconds of intro screens. Controller-driven emulator fixtures currently use those timings; adapt their input frame offsets for this intro-free source build. The host save tests and gameplay implementation are independent of the boot intros.

A from-scratch **native** Genesis re-implementation of Capcom's SonSon (1984), targeting full 60fps. See [ROADMAP.md](ROADMAP.md) for the plan and the *why* (it removes the per-frame graphics-translation tax that caps the sibling transliteration port at ~87%).

## Build
```bash
bash build.sh        # standard SGDK build -> out/rom.bin   (fast; not the LTO port build)
```
On macOS/Linux this uses native `m68k-elf-gcc` and SGDK 2.11. Set
`SONSON_GDK` to the SDK directory if it is not in the existing portForge location.
The compiler, SDK library, source, ROM and symbol identities are recorded in
`out/build-manifest.json`. `bash build.sh --clean` recompiles all native sources.
Windows retains the standard SGDK make workflow.

Run the native `out/rom.bin` in a **window** with
`mame genesis -window -cart out/rom.bin` (NTSC). The sibling `../sonson/` is the
older slow translation and remains a research reference.

Performance checks are headless and disable host keyboard/mouse/gamepad input:

```bash
python3 tools/perf_run.py --players 1 --frames 42000 --tag final-1p
python3 tools/perf_run.py --players 2 --frames 42000 --tag final-2p
python3 tools/verify_ring.py
python3 tools/verify_second_core.py
```

The second-core check uses the installed Genesis Plus GX libretro core; set
`GENPLUS_CORE` to override its location. It creates no emulator window. Per-refresh
traces, screenshots and results are saved under `out/`. The main soak replenishes
lives to cover sustained combat and full scenery loops; this is a performance
fixture, not a claim of an unassisted playthrough.

## Controls and credits

- **Arcade:** C on either controller inserts a coin. Start starts or rejoins that
  controller's player, spending a credit. Start never pauses. A/B fire; C is
  reserved for coins. The coinage DIP is respected, including free play.
- **Home:** choose 1 Player or 2 Players on the title menu. Either controller's
  Start pauses/resumes gameplay; held menu Start does not pause the new game.
  A/B/C fire. Music continues during pause while the world and game timers freeze.
- **Home Options → CREDITS:** choose 0, 3, 5 or 9 shared continues (default 3),
  separately from lives. At all-player game over, Start spends one credit to
  restore the selected players with their configured lives. With no credits left,
  Start cannot continue. A new game reloads the configured pool.

Control regressions run without emulator windows:

```bash
python3 tools/verify_modes.py
python3 tools/verify_modes.py --p2-only
python3 tools/verify_home_ui.py
```

## Instadeath survival (Home)

Choose **INSTADEATH 1P** or **INSTADEATH 2P** from Home's title menu. Each
player has one life: contact kills immediately, with no energy meter, bonus
lives or continues. Survive as long as possible. In co-op the shared timer stops
when the last player dies; Home Start pauses the world and the timer.

Pressure rises every 20 seconds: the scheduled-enemy limit grows from 4 to 16,
spawns become more frequent, and enemies get faster. The maximum retains the
Genesis sprite and CPU budget. The HUD shows elapsed time and the best survival time. Survival records are separate for 1P/2P
and separate from regular point scores.

## SRAM saves

The cartridge automatically loads high scores, survival records, Home options
and Arcade DIP switches on boot. Home options and Arcade DIP banks are saved
independently. Options save when confirmed; scores/time records save when the
run ends. Attract demonstrations are excluded. A reset or power-off during a
run does not finalize that run's record.

Regular score boards match the mode, player count and gameplay settings. The
32-entry store rotates old regular configurations when full while preserving
the survival boards. **RESET OPTIONS** keeps scores; **CLEAR SCORES** keeps
settings. Select either action in Home Options and press A/B/C twice to confirm.

The save format uses two 512-byte slots in conventional odd-byte cartridge
SRAM (header range 0x200001–0x20FFFF). Version, generation, CRC-32 and a final
commit marker protect against corrupt and incomplete writes. Readback failures
keep the old slot; absent/read-only SRAM leaves the game playable with session
settings. Unknown newer save versions are preserved. This is a battery-save
feature, not a mid-game save-state feature. Physical console/flashcart persistence
still requires testing on the particular device.

Additional checks:

```bash
python3 tools/verify_survival.py --players 1
python3 tools/verify_survival.py --players 2
python3 tools/verify_survival_core.py
python3 tools/verify_sram.py
```

All emulator checks are headless. Each save test uses its own disposable battery
save directory below `out/`, so it does not modify the player's ordinary save.

## Layout
```
src/        native game code (C, SGDK)
res/        SGDK resources (.res) -> compiled tiles/palettes/sprites
inc/        headers
tools/      build-time asset pipeline (arcade gfx -> Genesis format)
arcade/     the arcade ROMs (graphics + PROMs + program), source for conversion
out/        build output (gitignored)
```

## What we reuse (we are NOT starting from zero)
All under `../` (the sibling `sonson/` project + RE):
- **Arcade graphics** — `arcade/ss_6..ss_9` (BG char ROMs, 8KB ea), `arcade/ss_10..ss_14` (sprite ROMs), `arcade/ssb1..ssb3` (RGB color PROMs, 256B), `arcade/ssb4/ssb5` (color-lookup PROMs, 32B = the indirect palette).
- **The palette-resolution algorithm** — already implemented as `build_char` / `build_sprite` + the cache in `../sonson/port/src/video.c`. Phase 1 replays this math *offline*. (MAME's `sonson` driver is the canonical cross-check.)
- **Game-logic RE** — `../sonson/census_semantic.toml` (entity archetypes + behavior), `../sonson/regions.jsonl` + `../sonson/understand.json` (decoded code), `../sonson/levels.json` + `levelcap.txt` (level layouts). The port's `sub_*.c` are a working behavioral reference for every routine.
- **Tooling** — MAME at `D:/portForge/tools/mame` (GF meter `gf.sh`, snapshot luas) for verification; SGDK at `D:/portForge/tools/sgdk`.

## Status
Native one- and two-player simultaneous co-op are implemented. The October 2026
performance pass caches unchanged HUD fields and preserves the background ring
phase at world wrap, removing the full-plane transfer that stalled both modes.
See `tools/performance_results.json` for measured coverage and artifact identity.
Physical console testing and the inherited arcade-faithfulness gaps recorded in
`../session_resume_docs/NEXT_WORK.md` remain separate from performance validation.

### Arcade presentation

All interactive menus use the original arcade letter shapes at 256-pixel width. The opening selector uses the extracted SonSon logo; the port credit appears only there. Options and DIP settings use compact rows without instruction paragraphs. Gameplay keeps the score header, character life icons, credit count and survival clock; development FPS/round and pressure labels are removed. `tools/gen_arcade_font.py` regenerates the ASCII-compatible font from the baked arcade glyphs, including the small punctuation additions needed by menus and the timer. Font tiles load once per scene; dirty HUD updates remain in place.

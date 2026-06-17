# SonSon Native (Sega Genesis)

A from-scratch **native** Genesis re-implementation of Capcom's SonSon (1984), targeting full 60fps. See [ROADMAP.md](ROADMAP.md) for the plan and the *why* (it removes the per-frame graphics-translation tax that caps the sibling transliteration port at ~87%).

## Build
```bash
bash build.sh        # standard SGDK build -> out/rom.bin   (fast; not the LTO port build)
```
Run `out/rom.bin` in MAME: `mame genesis -cart out/rom.bin` (NTSC).

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
Phase 0 (skeleton) — builds + boots. Next: the asset pipeline (Phase 1).

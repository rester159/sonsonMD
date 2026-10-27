# Son Son II performance pass — 2026-10-26

Optimized two measured CPU hotspots without reducing actors, collision checks,
AI updates, animation detail, or altering the existing catch-up policy. This is
an incremental throughput improvement, not a claim of hitch-free 60 Hz.

## Changes

- `ss2_probe_point_native`: compact 68000 terrain lookup replaces duplicated
  inline C. Word indexing and known aligned accesses avoid widened arithmetic
  and caller register spills. It still reads the current map, bank and metatile
  base on every call, and preserves every source scratch/flag write. The old C
  implementation remains the host implementation and target diagnostic oracle.
- `ss2_obj_motion_native`: exact signed 16-bit velocity addition into strided
  24-bit coordinates using ADD/ADDX, preserving source carries and scratch byte.
- Explicit RAM alignment, retained lookup tables and ABI-preserved registers.
- `SS2_PERF_INPUT` is diagnostic only: target simulation-indexed input, life
  refill and double-buffered completed-tick evidence. Shipping builds omit it.

Stage-2 PC sampling ranked `dropper_edge` first (44,637 samples), ahead of the
frame-loop function (40,250), with `ss2_obj_common` also hot (22,080). The
instrumented object section averaged about 136 scanlines. H-int sampling changes
timing and replaces the normal raster callback, so it locates hotspots only;
throughput comparisons below use builds without that sampler.

## Matched-work results

Genesis Plus GX, core SHA-256
`88314cd8554a3cc5eebf00a4139e6f0dee9ea579dcb2321a0959579f1902fa09`.
Reported core refresh rate: **59.922743404312 Hz**. Both builds use the same
GCC 16.1/SGDK settings, `SS2_DBG_STAGE=1` or `6`, and `SS2_PERF_INPUT`.
Both include the preceding boot/audio fixes. Baseline source is saved under
`out/ss2/perf-20261026/baseline`; the diagnostic input/snapshot code was added
identically to both sides.

7,800-refresh runs; matched window is completed ticks 1000 through 6000. Enemy
counts, camera X/Y, player X/Y and RNG match on all **6,446** shared completed
stage-2 ticks and **6,912** stage-7 ticks. Comparing raw work RAM at refresh
boundaries was rejected: the faster build is often at a different point inside
a tick. Double-buffered publication avoids that ambiguity.

| Metric | Stage 2 before | Stage 2 after | Stage 7 before | Stage 7 after |
|---|---:|---:|---:|---:|
| Refreshes for 5,000 updates | 5,846 | 5,754 | 5,139 | 5,098 |
| Prepared pictures in that work | 3,273 | 3,347 | 4,442 | 4,550 |
| Prepared pictures/second | 33.55 | 34.86 | 51.80 | 53.48 |
| Simulation updates/second | 51.25 | 52.07 | 58.30 | 58.77 |
| Lowest prepared count in rolling 60 refreshes | 25 | 26 | 23 | 25 |
| Longest preparation-counter gap, refreshes | 5 | 5 | 5 | 6 |
| Peak live objects | 11 | 11 | 15 | 15 |

Presentation throughput improves about **3.9%** and **3.3%**. `ss2_present_n`
counts pictures prepared for queued DMA, not independently measured fresh
scanouts; no claim of unique-screen FPS is made. The rolling low improves,
but the isolated longest gap does **not** improve: stage 7's sampled gap grows
by one refresh as scheduling phase changes. This tradeoff is retained for the
higher sustained throughput and rolling low, with the limitation explicit.
Further render/scheduling work is still needed to eliminate the worst hitches.

The earlier exploratory host-driven runs are retained in the evidence directory
but are not the matched-work basis for these figures.

## Validation and resource impact

`python3 tools/ss2/perf/verify_native.py` runs the real 68000 routines in a ROM:

- 65,536 terrain cases against the original C reference; zero differences in
  return values, scratch coordinates, vine/solid/hazard flags.
- 65,536 signed-velocity pairs (all 16-bit velocity bit patterns) against an
  independent 24-bit arithmetic oracle, across all 32 slots; zero differences.
- Periodic whole-RAM guards and all callee-saved register canaries pass.
- Cases vary camera wrapping, signed coordinate inputs, bank 6/7, metatile
  pointer wrapping and synthetic attributes. No pointer/map cache is assumed.

MAME controller-driven release smoke enters both games, resets, and enters
again. Screenshots were inspected; gameplay and HUD render normally. This is
not a whole-game playthrough or physical console test. PAL was not profiled.

Native build and existing code-generation hazard guard pass (known SDK
Z80_upload warnings remain). Release linked text shrinks **20,480 bytes**;
data is unchanged and BSS increases **one byte** for alignment. Padded ROM size
remains 2 MiB. Earlier boot and audio fixes remain included.

## Build and evidence

Shipping ROMs: `out/rom.bin`, `dist/SonSonCollection-v1.0.bin`, and
`dist/SonSonCollection-v1.0-perf-candidate.bin`.
SHA-256: `8064697a5b5022bcab3bd4550ed063bb5d9df2924975735c97effc2695c0262e`.
No public push/release was performed in this pass.

Evidence: `out/ss2/perf-20261026/` contains source baseline, instrumented ROMs,
per-refresh CSVs, matched-work JSON reports, PC samples, release build log and
MAME screenshots. `out/ss2/perf-native-check/results.json` records the target
oracle result and core/fixture hashes.

Reproduction from repository root:

```
python3 tools/ss2/perf/verify_native.py
cc -O2 tools/ss2/gpgx_shot.c -o out/ss2/gpgx_shot
python3 tools/build_native.py -D SS2_DBG_STAGE=1 -D SS2_PERF_INPUT --out out/ss2/bench-s2
python3 tools/ss2/perf/benchmark.py out/ss2/bench-s2 out/ss2/bench-s2/run
python3 tools/ss2/perf/compare.py BEFORE_RUN AFTER_RUN 59.922743404312
```

Use stage index 6 for stage 7. Build the baseline with identical diagnostic
instrumentation. The target input and life refill are explicit fixtures,
not proof of natural survival or a complete route through every level.

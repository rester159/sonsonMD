# Stage-one upward scrolling — 2026-10-26

This pass adds headroom for vertical scrolling and removes a world-wrap redraw
spike. It does not establish that every hitch reported on stage one is fixed.

## Changes

`draw_row` now looks up each shared 4-tile metatile row once instead of decoding
all 33 cells independently. It still reads the live map and uses the existing
synchronous VDP transfers and shared scratch buffer. No queued source lifetime,
DMA budget, simulation, collision, sprite, or audio behavior changes.

`stream_view` rebases the previous camera origin across the 256-tile world wrap
before testing its displacement. A one-row upward wrap now writes 33 cells,
rather than redrawing all 759 cells. Ordinary large jumps still redraw. Since
256 is divisible by both dimensions of the 64x32 VDP ring, rebasing preserves
all resident cell addresses. The tested stage-one vine does not cross that
world boundary; the wrap fix is a separate verified corner case.

## Evidence

Baseline includes the earlier native terrain/motion optimization and boot/audio
fixes. Both builds have the same diagnostic input and work-state publication.
Genesis Plus GX reports 59.922743404312 Hz. GCC 16.1 and SGDK 2.11 are unchanged.

A diagnostic warp aligns the body probe with the first long vine at world X1080,
using a player origin 16 pixels to its left. Initial jump then alternating
40-tick Up/Down inputs produces actual vertical scrolling. Life refill and the
warp are test fixtures, not a natural whole-stage playthrough.

Across 7,529 shared completed ticks, camera/player coordinates, RNG and live
object counts match. In ticks 1000–6000, camera Y spans 1601–1661. Both builds
prepare 5,000 pictures in 5,000 refreshes: this light repeatable route already
keeps pace, so it does not reproduce the user's severe jitter.

For 1,000 sampled passes that stream a row, profiling section 10 falls from
37 scanlines to 23: **37.8% less map-streaming time**, about 0.89 ms of additional
headroom on those passes. This is not a 38% whole-game FPS gain. The timing
profiler remains enabled identically on both builds; its NTSC V-counter
approximation is not a cycle-exact measurement.

Captured frames at refreshes 2000, 5000 and 7799 are byte-identical before/after.
The release ROM also enters normal stage-one gameplay through the collection
menu and title sequence; its screenshot was inspected. No physical-console or
PAL validation was performed in this pass.

`tools/ss2/perf/check_streamer.py` compiles the actual streamer with a recording
VDP and an independent visible-map oracle. Address/undefined-behavior sanitizers
pass 99,864,666 cell comparisons covering all tile phases, bidirectional diagonal
movement, world/plane wrap and large jumps. Baseline and candidate produce the
same pixels; the upward-wrap transfer counts are 759 and 33 respectively.

Release build/code-generation checks pass, with only the existing SDK
Z80_upload warnings. Relative to the preceding perf candidate, linked text grows
256 bytes, data/BSS are unchanged, and padded ROM size remains 2 MiB.

## Reproduction and artifacts

Evidence directory: `out/ss2/perf-upward-20261026/`. `baseline/` contains the
pre-change sources with the same climb fixture; `vine-base/` and `vine-cand/`
contain traces and captures. `results.json` records the matched-work results.
Earlier `base-run` and `climb-*` attempts did not sustain camera movement and
are not used for the result above.

```
python3 tools/ss2/perf/check_streamer.py
python3 tools/build_native.py -D SS2_DBG_STAGE=0 -D SS2_DBG_WX=1080 \
  -D SS2_DBG_WY=1820 -D SS2_PERF_INPUT -D SS2_PERF_UP -D SS2_TPROF \
  --out out/ss2/upward-check
python3 tools/ss2/perf/benchmark.py out/ss2/upward-check out/ss2/upward-check/run
```

Release: `dist/SonSonCollection-v1.0-upward-candidate.bin`, also copied to the
usual `dist/SonSonCollection-v1.0.bin`. Prior candidate files are retained.
SHA-256: `dbb8af93c52dd05cb2983ee7cb0d5ac02db7e38aa141d69c5b18852bbd2d5c52`.
Changes remain uncommitted in the main checkout; nothing was pushed or deployed.

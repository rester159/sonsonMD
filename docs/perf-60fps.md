# 60 fps: what was actually wrong (2026-10-23)

Baseline ROM `03abbe52…`, 55,000 source ticks, Genesis Plus GX, NTSC 59.92 Hz.

| Workload | Missed refreshes | Worst 60-refresh window |
| --- | --- | --- |
| 1P controller script, before → after | 75 → 6 | 40 → 58 |
| 2P controller script, before → after | 768 → 57 | 32 → 52 |
| 1P exact input (`-DREPLAY_EXACT_INPUT`), before → after | 152 → 3 | 37 → 59 |
| 2P exact input, before → after | 488 → 75 | 31 → 35 |

Exact-input render equivalence vs baseline (`tools/frame_profile.py compare`):
Plane A 100% of matched ticks (1P and 2P), linked SAT 100% / 99.998%,
tile VRAM 100% / 99.9% (the remainder are the intentional one-refresh
deferrals below). Camera matches on every tick, and simulation rate is unchanged.

## Root causes (measured with the HInt PC sampler, `-DPROF_HINT`)

Average load was never the problem: 2P single-tick refreshes average ~60% of
the frame. Misses came from timing structure, plus a tail of heavy frames.

1. **Catch-up cascade.** The loop ran up to two 55.4 Hz source ticks per
   refresh. Two ticks never fit in one refresh (1009 of 1011 such refreshes
   overran). One overrun left ~1.85 ticks due, the next refresh took two, overran
   again, and so on: about a second at 30 Hz. Now one tick of debt is carried and
   repaid by the ~7.5% of NTSC refreshes that have no tick due. Two ticks run
   only above that (`CATCHUP_CARRY`, default 2). PAL keeps the old rule.
2. **Frames lost inside VBlank.** `SYS_doVBlankProcess()` waits for the *next*
   VBlank whenever it is entered during VBlank, so a frame finishing a few lines
   late cost a full refresh. `game_present()` now flushes immediately while the
   remaining blank lines fit the queued DMA (NTSC).
3. **Synchronized barrier flashes.** Every barrier requests a new pose on the same
   tick, which was the most common overrun in 2P. Cosmetic background
   publication (barriers, HUD score text) yields to the next refresh once the
   frame is late (`frame_budget.h`). Barriers rotate so none starve, and the HUD
   defers at most 8 refreshes.
4. **One-word DMAs.** Food/barrier/release/finish/bonus map cells went through
   the SGDK DMA queue (~400 CPU cycles per one- or two-word transfer; release
   queued 24 of them). They now use `vram_words.c`, plain CPU writes right after
   the frame's DMA flush in the same VBlank, in the same order.
5. **Codegen.** LTO inlined `arcade_native_admission_refresh` and
   scalar-replaced its 112-byte input: 2213 instructions with two moves per byte.
   It is now `noinline`, giving 318 instructions. The large-actor SAT copies use
   `memcpy`.

Also fixed: the camera H-scroll was written to VRAM mid-display (tear line,
sprites a frame out of step with Plane A). It is now `VDP_setHorizontalScrollVSync`.

## Remaining

One dense 2P G/H stretch (exact input, camera ~15400) runs single-tick frames
at 95–100% load and still drops to ~35 fps for about a second. Policy cannot fix
that, because carrying more debt only slows the game. It needs about 10% less
work per tick there. The candidates are render (`entity_render_mode`, about
1000 cycles per sprite including admission) and G/H stepping/contact. PAL 50 Hz
was already overloaded (sim needs 1.11 ticks per refresh) and is unchanged.

## Tools

```bash
python3 tools/frame_profile.py bench out --players 1 2
python3 tools/frame_profile.py profile --players 2 --camera 15370 15445
python3 tools/build_native.py -D REPLAY_EXACT_INPUT --out out/rx
python3 tools/frame_profile.py compare BASE/out/rx out/rx --players 1 2
```

`build_native.py` gained `-D`, `--out` and `--cflag`. `--out` builds still
need `out/rom_head.bin` from a normal build, because `sega.s` includes it by path.

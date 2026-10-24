# Historical release checkpoints

These are historical observations, not current release status.


1. Preserve the current baseline and prioritize M3: structural actor/collision costs are the immediate blocker. Defer cosmetic work and further title polish until representative crowded 2P gameplay meets its frame budget.
2. Validate structural changes against the same consumed inputs, then finish M2 across full courses. The existing direct-native-view experiment improves the short diagnostic 2P workload from 41.30 to 45.44 presentations/s (1P 57.31 to 57.89), and is now integrated after matching 18,019 per-tick records, native simulation pools, and 84,509 cached/direct sprite-view comparisons. Full-course coverage remains open. It does not close M3. Do not mistake diagnostic results for production results.
3. After M2/M3 pass, freeze the candidate and execute M4/M5. Finish M6 last so screenshots and documentation match the shipped source.

Each checkpoint reports the build identity, passed/failed checks, remaining blockers and next action. No credible release date or completion percentage is available until arcade discrepancies and the performance gap are bounded.

Use windowless harnesses where possible. If an interactive emulator is needed,
MAME/RetroArch must run in a small window, never fullscreen, and close when done.

Latest retained optimization: native C link-time optimization, with unchanged
SDK/boot objects. Fixed-input 18,000-tick comparison improved 54.01/41.29 to
59.40/57.90 presentations/s. All 18,011 compared per-tick gameplay records match;
281 pool snapshots differ only once in the E render cache. Repeated title idle
and P2 demo-start smoke checks pass. Maximum-pressure Survival passes 1P but
fails 2P (1,199 missed intervals out of 5,999). Full-course and hardware coverage
remain open. Evidence: out/parity-audit/native-lto/.

Retained -O3/LTO checkpoint: all 18,011 compared tick records and all 281
actor-pool snapshots match the -O2/LTO baseline. Fixed-input rates improved
59.40/57.90 to 59.73/59.16 presentations/s. Production controller rates are
59.89/59.35 with zero final debt, but worst one-second windows remain 52/32
presentations. Repeated title lifecycle passes. Survival stress still fails 2P
(745 missed intervals out of 5,999); 1P has none. Evidence:
out/parity-audit/native-lto-o3/. The paired background-DMA experiment was not
adopted; production graphics transfers remain unchanged.

Mode-specialization checkpoint: the renderer, actor update and collision body
are compiled separately for the session mode from shared C implementations.
All 18,011 Arcade tick records / 281 pool snapshots and 7,011 Survival tick
records / 109 pool snapshots match their respective baselines. A separate
target diagnostic compared 11,149 old/new Survival SAT emissions with no
differences. Repeated production title cycles pass. The same production ROM
passes both maximum-pressure Survival workloads (5,999/5,999 measured intervals
present in each mode, peak CPU 62.89%/84.08%). This is an injected pressure/grace
fixture, not proof of normal Instadeath rules.

Tradeoff retained explicitly: the fixed-input Arcade benchmark moved from
59.73/59.16 to 59.57/58.84 presentations/s; the Survival improvement does not
close Arcade performance. Current controller results above remain separate
from fixed-input results. Evidence: out/parity-audit/collision-mode-specialization/.

Individual score rollover now matches the source's eight decimal digits.
Pinned source bytes, 200,002 host decimal-carry cases, 243 actual 68000 cases,
and 15,360 score-gate cases pass. The updated production ROM still passes both
maximum-pressure Survival pacing checks. Current Arcade rates are 59.77/59.14
presentations/s, with zero final debt; sustained 60 FPS remains open. Combined
score arithmetic and per-award event ordering remain separate parity work.
Evidence: out/parity-audit/score-rollover/ and score-rollover-target/.

Paired column DMA retained after full-course fixed-input gains: 1P58.94→59.27,
2P57.75→58.15 presentations/s. Immutable generated map pairs halve normal
column queue operations; row-zero wrapping retains separate transfers.
1,277,952 host address/retry cases, target VRAM readback and80,000 food-world
updates pass. Existing food pixel/map artifacts regenerate unchanged.
Evidence: out/parity-audit/late-column-dma/, late-column-dma-2p/, column-dma/.
Next measured opportunity: barrier flashing still requeues constant tilemap
references while changing pattern data; skipping these requires proving all
background-stream and scene-reset invalidation paths first.

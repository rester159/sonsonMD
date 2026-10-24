# Public v1.0 release plan

Assessment: 2026-10-23. **Not release-ready.**

Current ROM SHA-256: `03abbe523a46c5900bc6b5f17aad19a925133c896536ba2db27e1d1690c9709c`.

Latest correction replaces legacy Start join/continue placement with source
state6/AE99 reentry, without consuming a granted reserve. Per-controller Start
is explicitly a console extension, not an original midgame arcade behavior.
Both joining orders, held Start, natural exhaustion/continue and skipped-source-
update admission are verified in five controller-only scenarios. Both opening
6000-refresh runs pass5999/5999 intervals. Full-course performance remains open.
Evidence: `out/parity-audit/join-reentry/decision.json`.

Previous33fba18b HUD correction:

Latest correction restores source reserve-life icons at columns2/23, rows2/3,
capped at four and excluding the active life. All512 owner/reserve values,
512 source icon pixels and32 target map words pass; screenshot inspected.
Both6,000-refresh opening workloads pass all5,999 intervals. Survival retains
its existing display. Full death/reentry scheduling and late-course timing
remain unqualified. Evidence: `out/parity-audit/reserve-hud/`.

Previous9265b12b correction:

Latest retained correction restores the source two-player TOTAL SCORE label
(4877,106B) and eight-digit combined score (4D5A,108D), including decimal
carry wrap. Fifteen existing lowercase font slots hold the source-colored
labels; no additional VRAM is allocated. Credits move to row27 to avoid the
combined-score field: this is an explicit224-line viewport adaptation, not
the source row30 position. Credit styling remains open; reserve icons are corrected above.
200,000 formatter cases, all32 score-field words,27 label words,960 glyph
pixels and palette readback pass. Windowless visual capture inspected.
Both6,000-refresh opening runs pass all5,999 intervals with zero simulation
debt. This is not full-course qualification; the new ROM's late-course
performance remains unmeasured. Evidence: `out/parity-audit/total-score-hud/`.

Previous ce37808e full-course performance baseline:
Latest retained optimization skips shot/player contact scans for completely
empty main/C/D/E/F pools. Presence is gathered in the existing actor updates;
occupied pools retain all source-order contact checks. Full fixed-input replay:
55,011 per-tick records and859 pool snapshots match;2,097,152 active-state
transition cases and five shot-adapter fixtures pass.
Production55,000-update1P/2P workloads:59.851/59.269 presentations/s,
worst60-refresh windows40/32, final simulation debt0/0. Both sustained60FPS
gates remain open. Evidence:`out/parity-audit/pool-contact-presence/decision.json`.

Gameplay score fields now use source eight-digit, leading-blank formatting at
columns2/13/23. Labels use source blue/orange glyphs and source positions.
Twenty-three otherwise-unused lowercase font slots hold labels and reserve icons; no extra VRAM
is allocated.200,000 formatter cases, actual2P tilemap with eight-digit score
fixtures,448 source glyph pixels and score palette readback pass.
Evidence:`out/parity-audit/hud-format/results.json`.
The latest build disables strict-aliasing optimization at compile and LTO link:
SGDK CPU copy reads u16 palette buffers as u32, and GCC16 removed their stores.
Actual PAL3 was garbage; all16 colors now match the intended ramp. This also
corrects selector/options palette initialization. Home title score width and
white glyph bank fixes remain included.

Preceding cdd340c6 full-course controller workloads (55,000 updates):59.799/59.092 Hz
1P/2P, worst60-refresh windows36/31, final simulation debt0/0. Sustained60FPS
still fails. Exact-input diagnostic compares33,011 ticks and515 native-pool
snapshots with zero differences. Ten fresh-process SRAM checks pass.
Ten preview screenshots from preceding HUD ROM c8136460: `out/screenshots/c8136460/selected/index.html`.
Evidence:`out/parity-audit/aliasing-build/decision.json` and
`out/parity-audit/aliasing-state/report.json`. The state recorder does not cover
every game global or establish full arcade parity. Windows flags are updated;
that toolchain has not been tested here.

Latest change caches ordered background contact candidates; bonus-life labels
also fit the 13-column value area. Current full-course and save results appear
in the latest checkpoint below.
The preceding ROM (766392a3) passed the runtime checks below.
The bonus-life integration boot crash is fixed: GCC 16 O3/LTO widened a
configuration byte load into an unaligned longword read. An explicit volatile
byte read prevents it; the linked code now uses MOVE.B. The normal ROM passes
controller-only P2 attract/start and 6,000-refresh opening workloads in both
player modes: all 5,999 presentation intervals pass, with zero simulation debt.
These opening checks do not close the full-course performance gate.

Current complete-course evidence (ROM `3acfda0b`): 55,000 source updates,
one course and restart in each player mode. Average presentation rates are
59.804 Hz (1P) and 59.104 Hz (2P), but the worst 60-refresh windows contain
only 36 and 31 presentations. Both sustained-performance gates remain open.
Lives are refilled in these workloads; normal-play qualification remains open.
Ten fresh-process SRAM checks pass; name-entry and Survival persistence still
need final acceptance.

## Closure priorities

Barrier batch-layout experiment rejected: exact-input 2P average
59.4032→59.4611 Hz, worst 60-refresh window33→34. Standalone upload
readback passes, but this does not close the frame-budget gate. Production
remains ce37808e. Evidence: `out/parity-audit/barrier-batch-layout/decision.json`.


Latest windowless diagnosis: camera 20380–20460, current-source 2P.
Food/barrier publication is the largest measured background component
(mean 125.4 / peak 425 subticks); total render mean 432.5 / peak 900,
sprite construction mean 208.4 / peak 300. These inclusive diagnostic timings
include interrupts and perturb cadence; maxima need not share a frame.
Next target is barrier publication and transfer setup, preserving every pose.
Evidence: `out/parity-audit/current-background-profile/interpretation.json`.


The release is blocked by frame stalls and unclosed integrated behavior/feature
checks. Stop average-FPS micro-optimization experiments that leave the failing
window unchanged. The popup-render specialization experiment is rejected;
production remains ce37808e.

1. **Performance:** reproduce the current production 2P worst window at source
   ticks 43293–43349 / camera 20406–20437 (32 presentations per 60 refreshes).
   Account for the expensive frame across simulation, contacts, render, DMA and
   audio before choosing another change. Boundary PC samples alone are not
   exclusive CPU costs. Accept a fix only when the failing window closes with
   unchanged gameplay, then run the complete 1P/2P workload.
2. **Behavior and features:** resolve confirmed arcade mismatches and run the
   existing controls, Survival, saves and UI checklist on the candidate. Keep
   reproduced defects distinct from missing evidence; do not expand scope.
3. **Release candidate:** freeze the ROM, complete normal 1P/2P playthroughs and
   visual/audio/persistence checks, and state hardware coverage accurately.
4. **Publication:** capture ten final screenshots, verify a clean reproducible
   source export, and publish to rester159/sonsonMD without ROMs or intros.

No new features or speculative engine rewrites during closure. Use the bounded
busy scene for iterations; run the full workload after a material improvement.
No emulator frontend is needed for the current performance reproduction.

## Milestones

| Milestone | Work remaining | Pass condition |
|---|---|---|
| M1 — Freeze an auditable baseline | Preserve the working source, tools, asset inputs, ROM, symbols and reports; consolidate known discrepancies into one defect register. | Rebuild from the snapshot; record matching artifact identities and reproducible 1P/2P workloads. Every blocker has a reproduction, expected behavior and regression check. |
| M2 — Complete arcade behavior | Verify the connected six-state title/attract flow and prelude against original traces; finish clock/RNG ownership across repeated cycles; resolve post-game timing, death/reentry, continue, course ending and restart differences. Compare enemy, collision, scoring and progression behavior throughout complete courses. | Integrated source/native state and event comparisons pass in 1P and 2P, including natural game-over. No unexplained gameplay differences. Explicitly document console adaptations and Home-only features. Component tests alone do not close this gate. |
| M3 — Meet performance target | Profile crowded 2P actor/collision work, rendering, DMA and audio; optimize measured costs with identical consumed-input regression workloads. | Present every active-gameplay NTSC refresh (approximately 59.92 Hz) in full courses and worst-case scenes, with full sound, no growing simulation backlog and original approximately 55.4 Hz gameplay cadence. Record fresh motion/art separately from display commits. Do not reduce enemies or change gameplay to pass. |
| M4 — Accept requested features | Run one final-build checklist for controls, Home, Instadeath, persistence, menus and branding below. | Every checklist item passes on the same candidate ROM, including reset, new-process save restoration and damaged-save recovery. |
| M5 — Qualify a release candidate | Normal complete 1P and 2P playthroughs without injected state or refilled lives; hour-long sessions in each mode; matched graphics/audio review; region and hardware validation. | No release-blocking defects. Release-sensitive checks pass on two materially different accurate emulator implementations. Record exact console/cart/firmware coverage; untested hardware remains explicitly unverified. Changes reopen affected checks. |
| M6 — Publish | Capture ten new screenshots from the accepted build; finish controls/build/asset instructions and known limitations; prepare a clean public export. | Clean-checkout build using documented local asset prerequisites; audit exported files and Git history for ROMs and intro assets; publish source to rester159/sonsonMD and tag v1.0 only after M1–M5 pass. Keep the playable ROM local. |

## M4 acceptance checklist

Current03abbe52 ROM:32 controller-only mode/option checks pass. Arcade checks
cover unpaid Start, held C on both controllers, C not firing, paid start/join,
and live-player held Start without pause or credit consumption. Home checks
navigate options3→5→9→0 and select5/0; verify held menu Start, pause freezing
simulation/camera, P2 resume, C not adding credits, and natural-game-over paid
shared continue versus zero-credit rejection. No injected mode, lives or RAM.
Both joining orders are independently covered by the same-ROM join-reentry
report. Evidence: `out/parity-audit/mode-controls-core/report.json`.
Other feature, source-parity and performance gates remain open.


- [x] Arcade controls: C inserts coins, per-controller Start starts/joins, Start never pauses; both joining orders, held Start and unpaid title Start tested on03abbe52. Full coinage-DIP matrix is separate.
- [x] Home controls: Start pauses/resumes; selected credits apply; paid shared continue and zero-credit rejection tested on03abbe52.
- [ ] Instadeath: one life, no energy, increasing enemy pressure, correct survival timer and end result.
- [ ] SRAM: high scores and names, options and DIPs persist across reset and a fresh process; old/corrupt/absent saves behave correctly.
- [ ] Arcade-style UI throughout; inspect every menu, gameplay HUD, name entry and ranking screen.
- [ ] SEGA and Capcom intros work in the local build and transition correctly.
- [ ] “ported by rester159, 2026” appears only on the opening selector; `v1.0` appears at its bottom right.

## Closure order

Work through these steps in order. Keep the requested feature scope fixed.

1. **Close M1:** bind the current source snapshot, local asset inputs, toolchain,
   ROM and symbols; reproduce the baseline. Existing ROM archives alone do not
   prove a clean source rebuild.
2. **Close M3:** fix the measured late-course rendering/DMA stalls. The barrier-map cache experiment is rejected: corrected full-course 2P
   diagnostic gain was only 58.145 to 58.270 Hz, with the worst window unchanged
   at 26. Narrow the late-section actor/collision profile before choosing the
   next change; broad instrumentation itself induces catch-up. Use a bounded failing scene for
   iteration, then rerun the complete 1P/2P workload once a candidate passes.
   Reject changes that alter consumed-input gameplay or merely improve average
   FPS while leaving the acceptance workload below its frame budget.
3. **Close M2:** produce one finite discrepancy register from connected
   source/native comparisons: title/start, player movement and combat, enemy
   waves, death/reentry/join, scoring, game-over/ranking, ending and restart.
   Classify each entry as a reproduced defect, missing evidence or documented
   console adaptation. Fix reproduced defects; verify missing evidence without
   speculative rewrites. Include complete-course coverage in both player modes.
4. **Close M4/M5:** freeze the candidate and execute the feature checklist,
   normal playthroughs, long sessions and graphics/audio checks. Record physical
   hardware coverage separately and explicitly identify untested configurations.
5. **Close M6:** capture ten screenshots from that candidate, validate a clean
   public source build with documented private asset prerequisites, audit files
   and Git history, then publish source and tag v1.0. No ROM or intro assets in
   the public repository.

## Current blocker register

Current-ROM normal-flow check: controller coin/start followed by neutral input,
no RAM writes and no refills, passes in1P/2P. Each owner traverses alive, dying,
respawning and final game-over with3→2→1→0 lives; reserve decrements occur at
source state5→6, final out at state4. Next attract cycle is excluded. Source
component replay also matches24,944 death and14,488 reentry observations.
This closes reachability for these neutral sessions, not complete gameplay
parity or frame-exact lifecycle timing.
Evidence: `out/parity-audit/natural-life-flow/summary.json`.


| Blocker | Evidence / next closure check |
|---|---|
| Gameplay presentation stalls | Current 55,000-update traces: 59.851/59.269 Hz averages, worst 60-refresh windows 40/32. Late barrier scenes remain the failing target; caching their map writes did not materially fix stalls. |
| Bonus-life integration qualification | Source-derived routine, raw DIP labels and reserve rollover are integrated; host oracle and target bridge tests pass. Full-game startup regression is fixed. Connected award/death/results scheduling still needs final acceptance. |
| Remaining gameplay HUD parity | Score widths/positions and player/HIGH labels are corrected and target-verified. Reserve-life icons/counts are now source-matched; credit styling, other overlays and lifecycle timing still need comparison. |
| Join/continue feature acceptance | Legacy placement is replaced by source state6/AE99 reentry. Arcade controller-only tests pass both joining orders and continues. Home credit/pause and paid/zero-credit continuation now pass controller-only checks. Other final-build feature acceptance remains open; Start admission is a console extension. |
| Integrated arcade parity unproven | Component contracts exist; connected title/post-game timing, per-award event ordering and complete-course original/native comparisons remain open. This is missing evidence, not a claim that every listed behavior is broken. |
| Final feature acceptance incomplete | Execute the M4 checklist on one frozen candidate. Survival pressure tests do not prove normal Instadeath rules. |
| Release qualification incomplete | Refilled-life benchmark runs do not replace normal playthroughs, audio/visual review, long sessions or named hardware coverage. |
| Public package incomplete | Clean rebuild/export audit, ten final screenshots and source publication remain outstanding. |

## Reporting and work limits

Report a milestone closed only with its exit evidence. Each checkpoint names the
ROM hash, blocker addressed, result and next action. Do not report a release
percentage or date unsupported by the remaining work. Do not add features or
frameworks during closure. New test infrastructure must address an identified
acceptance gap; passing component-test counts are not release progress by themselves.

Use windowless harnesses where possible. If an interactive emulator is needed,
MAME/RetroArch must run in a small window, never fullscreen, and close when done.

Historical optimization checkpoints are preserved in
[the checkpoint archive](docs/release-checkpoint-history-2026-10-23.md). They do
not override the current status at the top of this document.

Latest bounded experiment: `out/parity-audit/barrier-map-cache/decision.json`.
That experiment left production unchanged. The replay tool now anchors controller
patterns to the first active gameplay update rather than an absolute frame
counter; historical diagnostic comparisons must retain that limitation.
Late-section profile: `out/parity-audit/late-background-profile/analysis.json`;
its instrumentation induces catch-up and is not production timing evidence.

Retained native-loop split: fixed slot ranges now execute as separate ordered
loops, preserving same-tick allocations and K-before-I ordering. Full-course
fixed-input diagnostics improve 59.266→59.652 Hz (1P) and 58.145→59.067 Hz (2P).
All 55,011 per-tick fields match; 859 pool snapshots match outside two renderer-only
E readiness-cache values. The normal ROM rates are reported above. Both Survival
stress tests pass all 5,999 measured intervals; P2 attract-start smoke passes.
Evidence: `out/parity-audit/native-loop-split/`. Arcade sustained performance,
normal playthroughs and full source parity remain open.

Audio wait batching was rejected: full-course fixed-input 2P changed only
59.06656→59.12901 Hz, with the worst window unchanged at 30. Production audio
is unchanged. Evidence: `out/parity-audit/audio-wait-batch/decision.json`.

Single-pass barrier shots were also rejected: 2P fixed-input full-course timing
regressed 59.06656→58.89633 Hz and worst window 30→29. Both candidate and current
production wrappers match 1,680 saved original-CPU EB11 observations; candidate
also matches 20,000 wrapper equivalence cases. Production remains unchanged.
Evidence: `out/parity-audit/barrier-shot-single-pass/validation.json`.

NTSC one-update catch-up cap rejected: its average improved 59.067→59.557 Hz
but pending simulation reached 47 ticks, with 753 consecutive refreshes carrying
debt. Worst window worsened 30→29. Final zero debt and correct whole-course
average simulation rate conceal this temporary slowdown. Retain production's
two-update cap; solve frame cost without deferring gameplay. Evidence:
`out/parity-audit/ntsc-catchup-limit/decision.json`.

Latest performance diagnosis: the coarse late-course profile (camera18432–21503)
reduces measurement distortion: 9348 simulation calls versus9161 renders,
mean567 subticks per simulation and299 per render. Entity work averages361
(including133 collision); sprite construction averages219. These include probe
overhead and do not certify release timing. Evidence:
`out/parity-audit/late-coarse-profile/analysis.json`.

Precomputed sprite-tile lookups were rejected:327,680 exact mapping cases
passed, but full-course2P average improved only59.133→59.227 with the worst
60-refresh window unchanged at30. The tables add35KB and expand the padded ROM
by128KB. Production keeps the original routines. Evidence:
`out/parity-audit/render-tile-lookup/decision.json`.

H/G/I contact-range loop rewrite rejected:59.133→59.200 presentations/s,
worst60-refresh window unchanged at30.200,000 host cases and55,011 per-tick
fields match; one renderer-only E-ready byte differs in859 pool snapshots.
Production traversal remains unchanged. Evidence:
`out/parity-audit/contact-range-loops/decision.json`.
The fixed-input worst window begins at source tick31712, camera15395, with
12 scheduled legacy-slot actors plus4 D actors;2 E actors join during the
window. It advances55 source ticks but prepares only30 displays over60
refreshes. This is a sustained busy actor scene, not merely course reload.

Ordered background-object candidate lists adopted after positive paired
55,000-update results:1P59.720→59.742 (worst37→39),2P59.133→59.362
(worst30 unchanged), final simulation debt zero.20,000 randomized shot cases,
50,000 world steps (including same-tick player pushes) and1,680 original-CPU
shot observations pass. Membership invalidates on object loading and eligibility
changes; every contact still checks current flags and geometry in source order.
Production rebuild, paid P2 start and all ten fresh-process SRAM checks pass on
ROM3acfda0b. Production55,000-update course/restart workloads finish at59.804/59.104 presentations/s
(1P/2P), worst60-refresh windows36/31, final debt0/0. Both sustained60FPS gates
still fail. Refilled-life workloads do not establish normal-play qualification. Evidence:
`out/parity-audit/world-contact-candidates/decision.json`.

SRAM acceptance evidence on ROM7c9db16d: ten windowless fresh-process checks
pass for Arcade DIP writes, Home options/credits, mode independence, high-score
load/write/reload, damaged newest-slot fallback and both-slots default recovery.
This uses explicit RAM/save fixtures and does not prove ranking-name UI, Survival
time persistence or physical flashcart behavior. Tool: `tools/verify_sram_core.py`;
archived report: `out/parity-audit/world-contact-candidates/production-before/sram-report.json`.

Direct G/H sprite renderer rejected: paired33,000-update2P diagnostic
regressed59.42245→59.31500 presentations/s, worst60-refresh window unchanged
at30.500,000 isolated sprite comparisons passed; no production gameplay change
was retained. Evidence:`out/parity-audit/gh-render-direct/decision.json`.

Gameplay HUD production validation: ROMc8136460 completes55,000 source updates in each mode. 1P 59.786 presentations/s, worst60-refresh window 36, 2P 59.080 presentations/s, worst60-refresh window 31. Final simulation debt zero in both modes; sustained60FPS still fails. Evidence:`out/parity-audit/gameplay-hud-production/genplus-results.json`.

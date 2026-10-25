# SonSon Native — Roadmap

Current consolidated release checklist: [RELEASE_PLAN.md](RELEASE_PLAN.md). Historical checkpoints below retain their original evidence.

## Release assessment — 2026-10-23

Public release is blocked by arcade lifecycle parity and sustained performance,
not just packaging. Current build: `9c0518802e10d72b5e4994501fe8e847f5e380bc75ce0d3b658f4211dab27cfd`.
Extended 55,000-update workloads measure 59.71/58.96 presentations/s (1P/2P),
with zero final simulation debt. Both modes traverse one course and restart.
Both sustained 60 FPS checks still fail. Lives are refilled, so this is not a
normal playthrough; timing-dependent controller input also prevents treating
older-build rates as controlled comparisons.

Name entry and both ranking screens are connected to post-game flow. Five
injected-boundary target scenarios pass: 1P entry/results, paid Start
interruption, 2P names/individual/combined ranking, and a fresh process restoring
that save before another ranked run, plus unattended name-entry timeout through
both ranking boards. Serialized names, scores, CRC and next
save generation are checked. Natural game-over reachability and exact original
post-game timing remain open. The six-state title sequencer is now connected: animated prelude, gameplay demo,
individual/combined rankings and credited start screen. Repeated production
cycles and P1/P2/simultaneous purchases pass windowless smoke checks. Exact queue
timing, shared prelude/game retained state and full source presentation parity
remain open.

### Immediate work queue

1. **M1 checkpoint:** preserve a reproducible source snapshot, benchmark the
   current ROM in both modes, and consolidate the remaining parity defects.
2. **M2 first deliverable:** verify the connected six-state title lifecycle;
   verify clock/RNG ownership, credit/Start/Freeze transitions and post-game
   setup/timeout behavior. Cover both players and natural game-over paths.
3. **M3 first deliverable (interleaved with M2):** retain the verified native-loop split
   and profile the remaining late-course stalls with narrower timing probes.
   Full-course production now reaches 59.71/58.96 Hz; sustained 60 remains open.
4. **M4 persistence checkpoint:** verify ranking names, scores, options and DIPs
   survive a new emulator process and reset; include 2P and damaged-save recovery.
5. Then execute M2–M6 exit criteria below. No v1.0 tag until all release gates pass.

Every checkpoint must report the exact build, evidence, failures and next action.
No release date is supported until the performance gap and parity inventory are
bounded. Planning and automated checks do not require MAME/RetroArch frontends.

## Public v1.0 release gates (updated 2026-10-23)

Current status: not release-ready. Earlier phase checkmarks below describe prior
implementation work, not completion of the arcade-parity rebuild. The current
ROM still fails sustained 60 FPS in both player modes.

| Milestone | Required result | Exit evidence |
|---|---|---|
| 1. Reproducible baseline | Bind source, compiler/SDK, assets, ROM and symbols; replace stale performance claims. | Clean build and manifest; current 1P/2P results with scope and hashes. |
| 2. Arcade timing and logic | Unify source23:24 into one clock; establish boot/menu/results/course phase; preserve Home pause. Match arcade cadence and verify every stage, ending, restart, death, join and continue. | Original/native state and event comparisons with every remaining difference documented. Component-only tests do not close this gate. |
| 3. Performance | Sustain NTSC display commits at the target refresh rate in 1P/2P with full sound and unchanged gameplay. | Full-course and worst-case traces, rolling minima, bounded stalls, and separate simulation/presentation/art counts. Timing changes require new baselines. |
| 4. Requested features | Arcade C coins and per-controller Start with no Start pause; Home pause and configured credits; Instadeath; SRAM scores/options/DIPs; arcade UI; intros; opening-only credit and v1.0. | Final-build feature regression, including save/reset/invalid-save cases and mode transitions. |
| 5. Release candidate | Complete normal 1P/2P playthroughs and long sessions; compare graphics/audio; establish supported regions and hardware. | No release-blocking defects; named core/console/cart configurations tested, with untested hardware clearly identified. |
| 6. Public package | Ten new final screenshots, accurate instructions, clean source export and v1.0 tag. | Clean-checkout build; audit upload to rester159/sonsonMD to exclude ROMs and intro assets, with local asset prerequisites documented. |

The source-rate reference is MAME 0.288 SonSon at 55.40 Hz. Default fidelity
policy is original gameplay speed with approximately 60 Hz NTSC presentation;
the scheduler is connected, but pending simulation debt grows under load and
render interpolation is not implemented. One simulation update per nominal 60 Hz
refresh would speed gameplay up by about 8.3%. Repeated/interpolated display
frames must be reported honestly; 60 display commits are not 60 arcade logic
updates or proof of smooth motion. PAL requires its own tested timing policy.

The full clock audit still reports FAIL. The native adapter now advances both
clock bytes together and schedules source-rate work, but the original lifecycle
and sustained processing rate remain incomplete.
Blindly replacing it with wall-clock reads would make Home pause advance phases. Original IRQ masking, initialization, input
edges on non-simulation refreshes, audio cadence, and render interpolation need
explicit ownership before this gate can pass.

## Execution plan and acceptance criteria

### Current blockers and milestone status

| Gate | Status | What is still missing |
|---|---|---|
| M1 — Baseline | Partial | Existing hashes and benchmarks must be tied to a reproducible source snapshot and a consolidated open-defect inventory. |
| M2 — Arcade behavior | Open | Original two-player scripted attract lifecycle; boot/menu clock phase; Freeze DIP front-end coverage; join/continue ownership; RNG lifecycle/call order; integrated stage, ending and restart comparisons. |
| M3 — Performance | Failing | 1P/2P display cadence, worst-case stalls and growing 2P simulation debt. Existing small optimizations do not close this gap. |
| M4 — Features | Implemented in substantial part; final acceptance pending | Verify every requested control, mode, save and presentation requirement on the release candidate. |
| M5 — Release candidate | Pending | Normal complete playthroughs, long sessions, matched graphics/audio review and declared emulator/hardware coverage. |
| M6 — Publication | Pending | Ten final screenshots, clean public source build, exclusions audit and v1.0 publication. |

Original source inspection and a natural boot trace show a two-player script and
clock-dependent timeout. These now replace the synthetic one-player inputs and
fixed 2400-update cap. The sequencer, paid/unpaid demo Start handoff, demo-sound DIP gate and gameplay
Freeze gate have bounded checks. Setup, world state, prelude and full front-end
lifecycle still need integrated verification; those component checks do not
close the arcade behavior gate.

2026-10-23: the native two-stream input sequencer now matches 5,179 natural
original-CPU updates (41,432 field comparisons; two complete demos and a partial
third), with sanitizer checks and 68000 compilation. It is connected to the game
loop for both players. The production-loop test passes 512 phased-timeout cases
and the pad adapter passes all 512 player/input combinations. The ROM builds;
the windowless target harness also matches 5,760 script updates across three
complete demo cycles with both players and no injected game state. Full source
attract lifecycle validation remains an M2 blocker.

Execution order: close M1, then use the attract/lifecycle defect as the next
bounded M2 deliverable. Work on measured M3 CPU bottlenecks between parity fixes;
do not wait for all parity work before addressing the severe 2P slowdown. Re-run
the same 1P/2P benchmark after each retained performance change. Once M2/M3 pass,
freeze features, complete M4/M5, then capture and publish M6. Changes to the
candidate reopen affected acceptance checks.

Each milestone ends with a short report containing the build hash, passed and
failed criteria, evidence paths and remaining blockers. No percentage-complete
or release date is inferred from component-test counts. The next scheduling
checkpoint is a complete defect inventory plus a demonstrated improvement in
the dominant 2P cost.

Latest measured baseline: ROM
`a840997ad353b02ef0b685ea42931a53e732821c120c8600d0dbc315119b27f9`.
The windowless Genesis Plus GX workload completed 18,001 updates in 1P and 18,000 in 2P:
1P averaged 48.0 presentations/s and 55.4 simulation updates/s; 2P averaged
33.2 and 54.8 respectively. Worst 60-refresh windows had 20/20 presentations.
Neither run completed a course; lives were replenished. These are blockers,
not release validation.

1. **Freeze an auditable baseline.** Archive the current source/build identities,
   ROM, symbols, workload, core identity and traces. Inventory remaining provisional
   logic and classify every parity claim as source-proven, component-tested or
   full-game-tested. Deliverable: one reproducible baseline and explicit defect list.
2. **Close arcade behavior and timing.** Compare original/native state and event
   traces for movement, enemies, collisions, scoring, spawning and progression.
   Finish boot/menu/results clock ownership, Freeze DIP behavior, joins, continues,
   endings and restarts. Verify each stage and both player modes. Keep intentional
   Home/Instadeath differences separate. Deliverable: no unexplained gameplay
   divergence in the agreed coverage, with remaining hardware differences documented.
3. **Meet the performance requirement.** Profile elapsed CPU time by subsystem,
   then fix the dominant costs while rerunning deterministic logic checks. Retain
   original 55.4 Hz gameplay speed and target every NTSC display refresh (about
   59.92 Hz on the measured core). Define and validate smooth motion between source
   ticks; duplicated commits alone do not qualify. Test full courses, dense waves,
   two-player combat, large enemies, transitions and full audio. Deliverable:
   sustained display cadence, no growing simulation debt and no unexplained missed
   gameplay refreshes. Record rolling minima and art/motion updates separately.
   Baseline profiling may start before milestone 2 closes; final acceptance follows it.
4. **Lock requested features.** Test Arcade C coins, controller-specific Start and
   no Start pause; Home pause and chosen credits; Instadeath one life/no energy,
   increasing enemies and survival timer; SRAM scores/options/DIPs across restart,
   corrupt saves and interrupted writes. Review arcade-style UI, both intros and
   opening-only author credit plus bottom-right v1.0. Deliverable: final-build
   checklist with every requirement passed and no placeholder presentation.
5. **Qualify the release candidate.** Complete normal 1P/2P playthroughs without
   life-refill fixtures, extended sessions, and graphics/audio comparisons. Check
   save persistence and controllers on the declared target configurations. Test
   physical console/flashcart where available; otherwise label that coverage
   unverified and avoid compatibility claims. Declare NTSC support explicitly;
   claim PAL only after separate validation. Deliverable: fixed RC hash and no
   unresolved release-blocking defects. Code changes reopen affected checks.
6. **Publish v1.0.** Capture ten new screenshots from that RC. Finalize controls,
   supported configurations, build instructions, asset prerequisites and known
   limitations. Audit tracked files and publication history for excluded ROMs and
   intro assets. Verify a clean-checkout build with documented local prerequisites,
   then publish source and the v1.0 tag to `rester159/sonsonMD`. Keep the playable
   ROM local, with its exact path and checksum supplied to the user. Deliverable:
   reproducible public source package, ten screenshots and verified exclusions.

Release decision: all six gates must close. Feature implementation alone is not
completion; arcade behavior and performance are the largest remaining risks.
A reliable date requires the subsystem profile and remaining-parity inventory.
No emulator frontend is needed for the current automated workload. Any manual
MAME/RetroArch session must use a small window and close when finished.

## Next concrete deliverables

Four windowless prelude captures exposed and verified a fix for clipped bottom
UI: copyright and credits now fit the224-line display. This is a documented
layout adaptation; high score/credits/free-play/prompt now accept live owner values; production
lifecycle wiring remains pending.

The prelude display now runs in an isolated68000cartridge for640updates. It
uses the captured title background, so pixel comparison and source-command
composition remain pending. Source firing is disabled during this scene; shots
are not a missing feature. Production lifecycle binding remains the next gate.

Prelude pickup updates, player-priority collection and score-pose rendering now
use the shared source-derived routines. Caller-owned reward state and title
score gating remain explicit; live scene binding and shots are still pending.

The prelude now has the original first-segment terrain callback, verified across
all32collision-ring phases. It deliberately requires caller-owned ring state;
carrying that state through the original lifecycle remains integration work.

Prelude SAT composition is implemented and checked for source order, wrapping,
attributes, hidden actors and capacity. Live VDP ownership and scene integration
remain open; component tests do not prove the title scene matches the arcade.

Prelude input initialization now matches three archived natural arcade cycles
(1834 updates). Main-demo input regression also passes. The remaining title
integration must implement prelude actors/world/rendering and clock/RNG ownership;
input component parity alone does not close the lifecycle gate.
The prelude-specific 0100 horizontal motion entry is implemented and host-tested;
actor setup and fixed decoration placement now have a native owner and bounded
source-contract checks; real terrain, shots/collisions, rendering and live calls
remain open. The rebuilt shared player code passes the stopped-player target regression;
current performance remains below target in both modes.

Native sequencer validation: `verify_title_flow_natural.py` now matches 8021
updates and 18859 fields from the archived original uncredited lifecycle,
covering two complete cycles and a partial third. The module is not connected
to title.c yet; rendering, queue completion, prelude actors and credited runtime
cycles remain required. This does not close M2.

Source audit update: `tools/audit_title_lifecycle.py` pins all six original
front-end handlers. A credited waiting screen expires after 240 handler calls
and resumes the prelude/demo even with credits remaining. Replace the native
900-refresh title loop with the complete sequencer, including RNG reseeding,
source clock ownership and queued presentation transitions. Do not suppress
demos solely because credits are available. Natural full-lifecycle verification
and queue presentation decoding remain required.

1. **M1: auditable snapshot and blocker register.** Preserve the dirty source tree;
   bind an isolated source snapshot to the current ROM, symbols, toolchain and
   reports. List each remaining gameplay discrepancy with reproduction, expected
   source behavior and a regression check. Do not treat historical tests as final
   build acceptance.
2. **M2: lifecycle parity.** The join regression now passes paid starts/joins,
   held Start and new-entrant movement in both controller orders, with coherent
   snapshots and controllable flow states. Initial-player death input and full
   continue lifecycle remain outside this test. Keep requested per-controller joining as an explicit
   console extension. Next close boot/setup/attract/results clock and RNG ownership,
   then compare complete course progression, death/reentry and restart paths.
3. **M3: a measured 2P performance checkpoint.** The profiling insertion
   point around the fallback-contact guard is corrected and checked. The short
   diagnostic identifies collision processing and actor updates as major costs.
   Measure the remaining dominant costs, retain only validated improvements and
   rerun the same 1P/2P workloads. A local speedup is a checkpoint, not the exit:
   final acceptance requires every active-gameplay NTSC refresh presented, no
   growing simulation backlog, original gameplay speed and full audio. Record
   fresh motion/art separately from repeated display commits. Run M2 and M3
   iteratively; neither can be deferred to packaging.
4. **M4: one final-build feature checklist.** Cover both start/join orders,
   coin exhaustion, held Start, Home pause/resume, configured credits, Instadeath
   progression/timer, SRAM cold boot/reset/corruption recovery, all menus, intro
   return paths, and opening-only author credit/version placement.
5. **M5: fixed release candidate.** Complete normal 1P and 2P courses without
   life refills or injected state, including endings and restarts, plus at least
   one hour continuously in each player mode. Compare matched graphics and audio;
   run release-sensitive checks on two different emulator implementations. Record
   physical console/cart/region evidence separately and label untested targets.
6. **M6: source-only publication.** Ten new screenshots from the accepted candidate:
   opening selector, Arcade UI/DIPs, Home options, 1P gameplay, 2P gameplay, crowded
   combat, large enemy, Instadeath timer, results/high scores and ending. Check
   frames for excluded intro material. Clean-checkout build with documented local
   asset inputs; audit both published files and Git history for ROM/intro assets;
   publish to rester159/sonsonMD and tag v1.0 only after the other gates pass.

At each checkpoint report: gate status, exact build identity, evidence, remaining
failures and the next deliverable. Do not substitute a completion percentage or
an unsupported release date. No MAME or RetroArch frontend is required for this
planning work; later manual sessions must be small windows and closed afterward.

## Earlier implementation plan (historical)

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

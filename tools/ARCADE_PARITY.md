# Arcade parity audit — work in progress

The complete game is **not yet verified 1:1**. Prior FPS and screenshot tests did
not establish behavioral fidelity. Home options/credits/pause and Instadeath are
intentional owner-requested extensions; the arcade behavior is the core reference.

## Independent source

Owner-provided `ss.01e`, `ss.02e`, `ss.03e` main MC6809 ROMs, combined SHA-256
`09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1`.
MAME 0.288 `-verifyroms sonson` passes. Source addresses below are main CPU,
linear program space $4000–$FFFF, this exact revision. The older sibling port's
transliterations are navigation aids; original CPU observations decide behavior.
[MAME's driver](https://github.com/mamedev/mame/blob/mame0288/src/mame/capcom/sonson.cpp)
defines the hardware, not the game AI. Its screen refresh is 55.4 Hz.

## Corrected and independently checked

- `src/arcade_player.c`: typed native movement corresponding to $A9A7–$AD0F.
  Right $01AA, left $014D, ascent $0240, descent $02C0. Fresh versus held input,
  the four climb phases, fractional carry, screen bounds, shoulder riding and
  carrying the other hero preserve source behavior. Climbing changes facing but
  does not add ordinary horizontal run velocity. Clamp/separation retain the
  fractional X byte. The $A9C5 idle drift follows clamping and the fire pose.
- One-player source observation: 460 frames, 3,220 position/state/facing checks,
  zero differences. Two-player source observation: 1,280 frames, 20,480 checks,
  including 228 shoulder-riding actor frames, zero differences.
- The same two-player trace is replayed in the actual 68000 build by
  `tools/verify_player_target.py`. Inputs are injected at the joypad-read boundary
  to isolate differing board scan timing. Initial pose is seeded and respawn
  invulnerability suppresses enemy interruption. Checks occur at a completed
  presentation write; a video callback can observe a partially executed tick.
  This verifies integration and ABI, not a natural playthrough or enemy logic.
- `tools/gen_arcade_world.py`: all 26 source segment descriptors at $6485,
  their $653B macro dictionary, $6D3B+ scene streams and $825B+ collision masks.
  The complete course is 87 screens / 2,784 columns / 22,272 pixels. The old
  eight partial screen captures formed an incorrectly shortened 11,344-pixel
  course and accelerated its mapped wave cadence.
- Original-CPU column expansion fixture: all 832 unique segment columns,
  48,256 code/attribute/collision bytes, zero differences. The fixture accelerates
  column processing and selects valid segment pointers; it does not prove
  natural stage transitions. Generated scenes share 575 byte-identical tiles;
  no pixel approximation or missing segment is used to fit them in VRAM.
- $A847 floor probing rounds relative to the camera before adding its column
  ring offset. Direct world-X rounding was wrong on seven of eight scroll phases.

- `src/arcade_waves.c`: $EF7C/$EF98/$EFBB now load one wave and consume
  exactly one of its twelve control bits per simulation tick. Repeated active
  records retain their phase; absent bits clear active/state/phase while retaining
  their other fields. The producer's $9C delay decrements on the original phase.
- $95F4 RNG arithmetic is centralized and exhaustively compared with original
  CPU execution for every one of its 65,536 input seeds. Whole-game seed lifecycle
  and call order are **not** established by this test.
- $F682/$F7F3 G/H controllers preserve their different first wait ticks, wrapped
  zero countdowns, occupied-pool gate, reservation tick, six-member formations,
  entry side and direction. G deliberately indexes overlapping two-byte windows;
  H selects distinct twelve-byte groups. All four stage groups and RNG selectors
  are extracted from the verified ROM. Groups 2/3 are diagnostic coverage and
  are not a claim that normal stage progression reaches them.
- Original-CPU isolated-call comparison: 1,131 producer ticks, 15 delay cases,
  7,680 G/H controller cases, 11,520 member births and the exhaustive RNG cases;
  **406,503 field checks, zero differences**. `tools/arcade_wave_trace.lua` executes
  unmodified source routines through a RAM JSR wrapper with interrupts masked.
  This establishes local contracts, not natural gameplay reachability.
- G/H entry states ($C6DA/$C6F4/$C711 and $CF39/$CF58/$CF75) preserve source
  fractional drift, delayed activation, initial H motion and the entry boundary.
  **80 cases / 3,416 ticks / 30,744 fields, zero differences**, including left/right
  entry, zero/long waits, fractional carry and stopped-scroll cases. These traces
  invoke the complete source class dispatchers, including their phase loading.
- The same producer/controller/entry cases are replayed through actual functions
  in the compiled 68000 ROM: **12,242 cases / 531,956 byte checks**, zero differences.
  Return checks respect the target ABI: an unsigned-byte return defines D0's low
  byte, not its upper byte. This is isolated arithmetic/ABI evidence.
- Gameplay now reserves six slots per G/H family and invokes the controllers after
  actor updates/collision. Pending members remain invisible and non-colliding;
  ordinary combat contact begins on the following tick after entry completes.
  Scenery seams no longer reset producer/controller state. G/H now continue into
  the source-grounded combat module described below; other class controllers
  still use provisional single-body fallbacks.

## Rebuilt G/H combat and straight weapons

`src/arcade_foes.c` replaces the regular-mode G/H approximations after entry:
walking decision cadence, target selection, vertical transitions, terrain seating,
throw windup/recovery, one-weapon change to the unarmed variant, off-screen
cleanup and both normal/special death animation paths. The weapon has four
reserved slots and travels horizontally; it is no longer routed through the
provisional parabolic projectile. G, H, then their weapons update in source order.
The custom Survival spawner still uses its separate provisional actor behaviors.

The native data has a finite animation graph of **168 frames / 38 pose pairs**,
source selectors separate from compact frame IDs, and exact motion vectors.
Generated code contains no original CPU program or interpreter. Animation
countdowns preserve the source's load-versus-decrement timing, including multiple
animation ticks in a single source update. Sprite art adds 24 previously missing
resident poses; the shared background base moves from 562 to 654. Sprite tiles
290..653 and background tiles 654..1228 remain below the font at 1376.

Independent original-CPU dispatch fixtures cover **48 trajectories / 19,200
updates**, including **14,050 active-actor ticks**, slots 0/3/5, live/out/alternate
player configurations, gaps, attack refusal, climbing, dropping, ordinary death
and the alternate group-death animation entry. **1,293,960 field checks, zero
differences.** These are isolated source calls and seeded deaths, not a natural
playthrough or proof of contact/scoring semantics.

The actual compiled 68000 functions replay all 19,200 cases, comparing state,
animation output and four weapons. A RAM callback records the **10,498 floor
queries** observed on the original CPU and supplies the independently decoded
collision result. **1,656,776 checks, zero differences.** This proves arithmetic,
ABI and callback arguments; normal-game floor integration has separate player
and ring checks.

The linked ROM's animation metadata and pose pixels are checked against source
ROM data. All 2,784 flattened collision columns are independently checked against
the original CPU's column-expansion observations, including each segment repeat.
The flat collision array removes the per-actor 26-segment search without changing
terrain. A normal one-player source observation also confirms the unused second
controller retains kind zero; the native bridge now distinguishes never joined
from eliminated players instead of suppressing targeting in every one-player run.

Still missing at the G/H boundary: source contact boxes/phase gates, complete
hit-side-effect and group-clear reward transactions, exact player lifecycle flags,
the source 24-sprite admission budget, shared bonus/pickup state $FF and externally
assigned H state 3 ($87CD). Those are explicit unresolved contracts, not claimed
covered by the combat fixture.

The original `$E2B3` collision dispatcher selects P1 on even global phases and P2
on odd phases. The former native dispatcher processed both every tick. An
independent running-source trace observes 128 selected pages with zero mismatches;
its write tap filters out `$D5`'s unrelated floor-scratch uses. Native collision
pages now follow this source schedule in regular modes. A compiled regression
with both owners' shots overlapping one body fails on the earlier ROM (P1 always
wins), and passes both phases on the corrected ROM. Other collision details remain
pending; Survival retains its custom all-player checks.

## Other corrections awaiting their own complete source trace

- $AD10 / $B018 give each hero two projectile slots, not one. The old $B25A
  citation describes a different object's single slot and was misidentified.
- $B04F gives horizontal player shots +/-6 pixels/tick, and $B091 limits flight
  to 96 pixels from launch. The launch pose is captured before the player's idle
  drift. End-puff animation keeps the slot occupied until released. Exact frame
  and collision event comparison remains pending.
- Starting horizontal placement and separate co-op tiers now follow observed
  arcade poses. Difficulty starts at the DIP-selected stage, not always zero.
- Music transition cues use the source segment event byte instead of sounding
  on every visual seam. Special-event sequencing remains pending.

## Earlier performance checkpoints and transparent CPU changes

The fuller G/H groups exposed a two-player regression: fourteen missed
presentations in 42,000 Genesis Plus GX refreshes. Empty/reserved-slot rejection
now precedes type decoding in the shot collision scan; type metadata uses a
constant table instead of the comparison chain. The provisional tier-seating
helper uses an equivalent closed form over its admitted 40..200 range (all 161
positions, including tie cases); other values keep the original scan. Score
formatting uses the SDK hardware division helper for its six-digit path.

A fixed-input two-ROM comparison captures completed-update state, rather than a
potentially partial refresh snapshot: 4,000 two-player ticks, all 94 player/world
bytes and 2,016 entity bytes per tick, **zero differences**. The inputs are keyed
to simulation index so different missed refreshes cannot change their meaning.
The compiled score formatter is separately compared with SDK `intToStr` on
10,897 values, including all 0..10,000 values and large/signed-boundary fallbacks:
76,291 bytes, zero differences.

On ROM `1d7e5631dc1f9d6aedfbd1bf1e3fc402b0cdba726205bf70617954b2532e17fb`,
42,000-refresh full-ribbon runs each reached a wrap: 1P had no missed presents,
2P had **four** (rolling 60-refresh floor **59**). Peak SDK load was 66.75% / 87.79%;
peak scheduled bodies 14, total entities 23 / 24. These are partial-AI,
controller-driven tests with lives refilled, not the final source-equivalent
heavy workload. **Sustained 60 in both modes is not yet established.**

A ROM read-tap profile of 4,000 completed updates (no instrumentation added to
cartridge code) places the largest remaining cost in the entity/collision/
scheduler section: mean 5.65 ms, peak 10.74 ms in that fixture. These elapsed
sections include interrupts occurring inside them; they are not exclusive CPU
cycle totals. The failed zero-tap experiment is retained separately and excluded.

## Latest measured build

ROM `acdf4bab0ced9f6c1ec426f3509703e679be26363387173e86a61a2335814377`
includes the G/H combat rebuild, flattened terrain lookup, direct slot indices
and source collision-page schedule. In separate 42,000-refresh Genesis Plus GX
runs, **1P has no missed presentations; 2P has one** (rolling floor 59). Both
runs complete a full ribbon wrap. Peak SDK CPU load is 73.19% / 89.45%; maximum
scheduled bodies 19, total entities 27 / 28. This is materially more behavior than
the earlier simplified workload. Sustained 60 in both modes is still pending.

The direct-index optimization was compared before/after over **16,000 completed
two-player updates**, 2,110 player/world/entity bytes each, with zero differences.
The collision-page correction intentionally changes hit timing and has its own
source trace and failing-old-ROM/passing-new-ROM regression.

A final controller check detected that the legacy `entry-oracle.jsonl` had been
partially overwritten by a failed old writer. The corrupted artifact is retained
as `entry-oracle-corrupted-stale-writer.jsonl`; it is not accepted as evidence.
A fresh bounded run writes `entry-oracle-v2.jsonl`, which reproduces the original
verified hash `dd6dac4ce7e6a3efba660dd4d109012acf9f3e916b202111c6b2964790c71fca`.
Target verifiers now reject source fixtures whose hashes differ from their
passing source comparison reports. Snapshot capture output is isolated by ROM
hash. The ten current gameplay captures and their ROM identity are in
`out/parity-audit/foe-preview/manifest.json`.

## Combat difficulty correction

The native port now keeps a separate 16-bit combat accumulator for source
`$96:$97`; the scenery base remains `$5A`. G/H kills add `$0033` / `$0019`.
Wave formation selection and ordinary G/H decisions read the combat high byte.
At scenery seams, the base increments below `$1C` and replaces only the combat
high byte; the fraction survives. Once the base reaches `$1C`, seams leave
combat difficulty untouched. Addition wraps at 16 bits before rejecting a
result at or above `$3F00`, exactly as the original CPU does.

`arcade_difficulty_trace.lua` executes the original `$E407` and `$A73F` routines
without changing program ROM. Across 9,216 cases (all low bytes at eight boundary
high bytes for four increments, plus every base byte with four combat values),
`compare_arcade_difficulty.py` and `verify_difficulty_target.py` each check
18,432 fields with zero differences. The latter executes linked 68000 functions.
ROM: `58fc3f71a7506abd952a593d120dcbef733748bf8ce868eb371af7d59e10a99c`.
This proves the local arithmetic and seam contract, not the complete kill economy:
non-G/H kill contributions, restart/continue lifecycle and natural high-group
reachability remain unverified. Existing approximate hit detection still controls
when G/H increments occur and must be replaced.

The preceding performance figures and ten screenshots belong to the earlier
`acdf4bab` build. A fresh 42,000-refresh run per mode on `58fc3f71` completes one
course wrap each: 1P has zero misses; 2P has two (rolling floor 59). CPU peaks are
73.19% / 89.45%. Both 6,000-refresh smoke runs had zero misses, illustrating why
short tests are insufficient. All 14 cabinet/Home control checks and the two
collision-page regressions pass. Full 1:1 behavior and strict 60 FPS in both modes
remain unfinished. See `out/parity-audit/difficulty-course-results.json`.

## G/H shot-hit geometry

The generic rectangle test for the first twelve native G/H slots is replaced
with source `$E8EA` / `$E993` geometry: `(u8)(enemyY-shotY+11) <= 16` and
`(u16)(enemyX-shotX+8) <= 16`, using upward internal Y. Both bounds are inclusive
and wrapping is intentional. High-bit major states are excluded. Other families
and custom survival retain their existing collision paths pending extraction.

`arcade_hitbox_trace.lua` invokes both original collision leaves without ROM
patches, including edge grids around X/Y wrap, every byte Y, and distant high-byte
X values. `compare_arcade_hitbox.py` checks 7,748 original CPU results, with 1,192
hits and zero differences. `verify_hitbox_target.py` also checks all 7,748
results on linked 68000 code with zero differences (ROM `48c2636b2832dc7d390f1561c1bbeb67723b6d235ebf9887e9fbd1f6b53f8c09`).
This establishes geometry only: family/shot scan order,
entry eligibility, active=2 death flags, contact collisions and complete reward
transactions remain unfinished. Source group-clear leaves also add difficulty
`$004C` (G) / `$0040` (H), besides the ordinary kill increment; these are pending
along with their reward/audio events.

The `48c2636b` build completes one course loop per mode in 42,000-refresh tests.
1P misses one presentation (rolling floor 59), 2P five (rolling floor 58); peak
SDK CPU is 77.44% / 88.18%. The changed hit geometry changes combat outcomes, so
this is not an equal-work optimization comparison with prior builds. Strict
60 FPS remains unmet. The two collision-page regressions still pass. Details:
`out/parity-audit/hitbox-course-results.json`.

## G/H kill and group-clear transactions

The successful-hit path now preserves source active=2 for the enemy and shot,
credits the firing player, decrements the family reservation with byte wrap,
and selects phase 4 on the final group kill. Rendering now includes active=2
through the death/puff sequence. Ordinary G/H hits award 150. Final G/H kills
add 2,000 / 1,000 points and an additional `$004C` / `$0040` combat difficulty
increment after the ordinary increment; the two additions each apply the source
cap separately. Native calls sound `$0D`, then `$0E` for a group clear.

`arcade_reward_trace.lua` executes original hit leaves and the actual `$4CBE`
score handler, checking both owners, all 256 reservation bytes, both families
and eight difficulty boundaries. `compare_arcade_rewards.py` and
`verify_rewards_target.py` each match **8,192 cases / 57,344 fields** with zero
differences. `verify_reward_integration.py` executes the linked entity update
for eight family/player/ordinary/final combinations, checking 80 score, state,
active-flag, reservation and difficulty values; zero differences.

Current ROM: `310ddea2917c4daebf157e772a2e2067fea81d58031a6d4964c32986449adea8`.
In 42,000-refresh course tests per mode, 1P misses one presentation and 2P six;
rolling floor 59 for both, CPU peaks 77.78% / 89.45%, one course wrap each.
See `out/parity-audit/reward-course-results.json`. Strict 60 remains unmet.
These fixtures prove successful-hit transaction behavior, not natural family
scan order, eligibility, full event scheduling, score rollover, or source audio
waveform/timing. Those remain open. Earlier sections retain historical findings
and ROM-specific measurements; this section supersedes their pending G/H
successful-hit/group-clear implementation notes.

## G/H scan order and fixed shot slots

The source runs H then G. Each family scans shot slot 0 then slot 1, then its six
actor records in address order. Only active=1 records with a clear major-state
high bit participate. Native G/H collisions now use this sequence and the source
active/state test directly; the former cached entry flag no longer suppresses a
G/H shot hit. Player shots retain their own stable source slot ID during flight
and the puff, independent of where a free global entity record is allocated.

`arcade_scan_trace.lua` executes the original H/G scanner functions and observes
successful hit leaves. Across 1,024 deterministic mixed-overlap fixtures there
are 755 hits and 2,092 eligible selector calls. `compare_arcade_scans.py` matches
the ordered hit lists; `verify_scans_target.py` checks the same source-derived
selector outcomes in linked 68000 code. `verify_reward_integration.py` now has
88 checks, including reversed entity-versus-shot-slot order: H takes shot slot 0,
while slot 1 and the overlapping G survive. These tests do not prove source
ordering against the other enemy families, whose collision paths remain partial.

The generic shot loop skips dedicated G/H and weapon slots after the new pass;
those slots cannot be generic targets. Custom survival retains the generic loop.

Current scan build `25efb76af86d8235f37ca3e5ac34ff15a2328c1d06f8f4622eb73e67f9a12ecd`
passes 2,092 compiled selector cases, 88 integration checks, the two collision-page
regressions, and all 14 mode-control checks. Its 42,000-refresh tests complete
one course loop per mode: 1P misses one presentation, 2P six; rolling floor 59
for both, peak CPU 75.49% / 88.92%. The pre-trim scan build missed 2 / 29;
refresh-driven inputs make those timing runs different workloads after a stall,
so they are not proof of state equivalence. Full fidelity and strict 60 remain
unmet. See `out/parity-audit/scan-course-results.json`.

## Player-shot lifecycle and puff collisions

Regular-mode player shots now execute the typed native `arcade_shots.c` state
machine corresponding to original `$B03A/$B05E/$B075/$B0A1/$B0B1/$B0C0`.
The original flight and end scripts alternate frames every two ticks. Newborn
shots move on their first update; impact initialization holds the source flight
pose until the next update. End setup sets a four-tick countdown, followed by
four end updates. Offscreen release still finishes movement/animation arithmetic.
The isolated helper also preserves the observed newborn-offscreen availability
edge case: the source animation call changes Y to a ROM script, so the immediate
release cannot restore the parent's counter. This is a semantic condition in
native C, not a CPU/register interpreter.

`arcade_shot_trace.lua` executes 48 trajectories (1,920 updates), covering four
directions, fraction bytes, ordinary/range/hit/offscreen paths and camera wrap.
`compare_arcade_shots.py` matches 26,880 fields; `verify_shots_target.py` matches
30,720 compiled byte/counter checks. `verify_shot_render.py` checks all four
linked poses against source pixels (512 bytes) and eight target SAT cases.
Source attributes `$C2` keep the artwork unflipped for both travel directions;
active=2 impact shots remain visible.

A range puff remains active=1 and is still eligible for shot collisions. An
impact changes active to 2 and restarts state 2, preventing further hits. The
source scan fixture now includes all four shot major states: 1,024 scenarios,
747 hits, 1,990 selector calls, zero differences. `verify_reward_integration.py`
checks 16 flight/puff/player/family/last-kill cases, 192 values, including restart
of an existing range puff and reversed global entity versus stable shot order.

These are isolated contracts plus integration fixtures. Input firing cadence,
parent availability lifecycle, and original spawn-coordinate/fraction handoff
remain pending. The production bridge derives capacity from live slots; it does
not yet use the helper's parent-counter output. Custom survival retains its
existing shot rules. Source 55.4-Hz versus native presentation timing remains open.

Current ROM: `3122be9265605603e653ea6b1ee057422cd84f72f99205c0edb6703069c59bf9`.

The current shot build completes one course loop per mode in 42,000-refresh
tests: 1P misses two presentations; 2P seven; rolling floor 59 for both. Peak
SDK CPU is 77.83% / 92.72%. Strict 60 remains unmet. These controller workloads
still use life refills and partial other-family AI. Details are in
`out/parity-audit/shot-course-results.json`.

## Firing gate, fixed player-shot pools and parent counters

Original `$AD10` checks nonzero parent availability and a rising input bit 0.
It selects shot 0 only if inactive, otherwise shot 1 (without another active
check), writes active=1 and state/phase=0, copies integer X/Y and facing, and
subtracts 8 from internal Y in parent state 5. Nonzero parent kind forces facing
1. Fraction bytes and all unrelated shot fields survive. Availability does not
change until the shot's first update.

`arcade_fire_trace.lua` executes 4,096 original-CPU cases spanning availability,
held/new button bits, both kinds, state 1/5, four directions and active flags.
`compare_arcade_fire.py` matches all 262,144 shot-record bytes (768 births), and
`verify_fire_target.py` checks 20,480 compiled result fields with zero differences.
`verify_fire_integration.py` has 12 target cases / 432 checks: both owners can
fire with all ordinary records occupied; two first updates consume availability;
release restores it; stable slot ownership and fraction bytes survive reuse.

Regular mode now reserves records 16..19 for the four player shots. Ordinary
allocations start at 20; custom survival retains its existing allocator. Parent
availability starts at 2 on native game initialization and is updated by the
source-checked shot routine, replacing the prior derived free-count bridge.
The captured pre-drift launch pose supplies height-corrected Y and source facing.
Native alive-player firing uses source kind 0; full player entry/death/respawn,
continue and attract kind/counter transitions remain to be rebuilt. Fixed source
pools for other families and full global capacity remain pending.

ROM `69c5b20ad4b364199ae621b45ef312f1e95412dc6cb8e0c18d4208c978d7fd34` passes the 16 collision/reward integration
cases, collision-page regression and all 14 mode-control checks. Over 42,000
refreshes per mode it completes one course wrap each: 1P misses two presentations,
2P seven; rolling floor 59 both. Peak SDK CPU 78.66% / 91.06%. Strict 60 and full
arcade parity remain unmet; `out/parity-audit/fire-course-results.json` binds the
workload and ROM. This section supersedes the earlier unbound-counter limitation.

## Required before claiming complete 1:1

1. Rebuild the remaining enemy families, their hit points, targeting, sprites
   and projectile behavior. Finish G/H's external collision/reward/bonus states
   and normal-play integration; their isolated ordinary combat path is checked.
2. Finish non-G/H wave controllers and their formations, original global phase
   initialization, complete pool ownership/update order and exact capacity. G/H
   producer, controller and entry contracts are now independently checked; their
   normal-play integration still depends on the remaining lifecycle/special states.
3. Match the original RNG seed lifecycle and whole-game call order, including
   scenery/food/other controllers. Arithmetic is exhaustively verified; native
   startup currently seeds zero and cannot yet claim natural formation parity.
4. Compare shot flight, slot release, terrain/body collision, score and effect
   events on source and target. Source contact routines have phase gating and
   byte boundary tests beyond the current generic body AABBs.
5. Replace four hand-placed ambient fruit with the original food/pickup system,
   including background changes, progression, score economy and bonuses.
6. Rebuild and trace actual player death/respawn/entry/continue states. Existing
   native life flow and grace timers are approximations; $B1C2 was misidentified
   as the player respawn, whereas the player uses the $AD9B/$AE72 path.
7. Verify special/checkpoint events, all 26 segment transitions, complete course
   wrap, difficulty/bonus settings, attract behavior and both player join orders.
   In particular `$9044/$9046` initially copy base stage $5A into high byte $96.
   `$A73F..$A748` increment base $5A up to $1C and copy that high byte at seams
   while below the cap; at the cap, later seams no longer reset it. `$E407`
   separately adds a 16-bit kill-dependent amount to $96:$97 and rejects results
   with high byte >=$3F. G/H kills add $0033/$0019. Native base and combat state are now separate; fractional carry and cap
   semantics pass original-CPU and 68000 tests. Full reset/continue lifecycle
   and all other kill contributions still need proof. Formation groups 2/3 cannot simply
   be declared unreachable because the base stage caps at $1C.
8. Resolve original 55.4-Hz simulation versus the Genesis presentation clock;
   one source tick per NTSC refresh is faster in wall time. Keep 60-Hz display
   performance separate from source simulation cadence.
9. Verify animation script timing/order and audio commands/tempo with gameplay.
   A matching still sprite or a source frame list is insufficient.
10. Re-run performance on the final equivalent heavy workloads in two emulator
    implementations; test physical hardware separately before claiming it.

## Repeatable evidence

All emulator commands must remain headless or explicitly `-window`.
Never launch MAME or RetroArch fullscreen.

`tools/arcade_player_trace.lua` records original execution (`SONSON_TRACE_PLAYERS`
and `SONSON_TRACE_OUT` choose player count/path). `tools/arcade_world_trace.lua`
records original column expansion. `tools/arcade_wave_trace.lua` and
`tools/arcade_entry_trace.lua` record isolated source routine execution. Run with
the owner's local verified arcade
ROM directory; no arcade ROMs are distributed.

```
python3 tools/compare_arcade_player.py
python3 tools/compare_arcade_players.py out/parity-audit/arcade-2p.csv
python3 tools/compare_arcade_world.py
python3 tools/compare_arcade_waves.py
python3 tools/compare_arcade_entries.py
python3 tools/gen_anim.py --base 290
python3 tools/gen_arcade_foes.py
python3 tools/gen_arcade_world.py
python3 tools/compare_arcade_foes.py
bash build.sh
python3 tools/verify_foe_assets.py
python3 tools/verify_foes_target.py
python3 tools/verify_collision_pages_target.py
python3 tools/verify_waves_target.py
python3 tools/verify_player_target.py
```

Raw traces and reports are under `out/parity-audit/`. Reports apply only to the
ROM/source identities they record; the named `SonSonMD-1.0.bin` remains preserved
while the parity rebuild is being validated.


## Class-I controller and ordinary actor recovery (2026-10-22)

The actual class-I controller is **$F97A**, record $06A8 / producer bit 7,
with four owned actor records $0D98..$0E17. $FA07 instead owns $06B8 / bit 8
and scans background records; it must not be substituted for this controller.
The native controller is `arcade_i_wave_step` in `src/arcade_i.c`.
It waits for zero live class-I actors before its initial countdown. The timer
advances on `(clock+3)&31 == 0`, wraps as a byte, and initializes four reserved
births. Births use camera+248, Y248 and facing3 with twelve decrement ticks
between spawn phases. Major state1 continues even when the live count rises.

Original MC6809 execution covers 8,640 controller cases (576 births), including
zero-timer wrap, inactive controllers, live-count gating and camera wrap.
Host comparison and RAM-loaded native 68000 code both pass 138,240 meaningful
field/byte checks. This is a component test, **not linked gameplay validation**.

`src/arcade_i_actor.c` implements ordinary $D317 states0..4 using typed position,
velocity, animation and lifecycle fields. Horizontal acceleration is unsigned
32-bit +24/-24; vertical acceleration is 24-bit $001210. The source uses a
separate flight animation format at $D50C and ordinary death scripts at $D4CB,
$D4D9, $D4EB and $D4F9. The flattened native frame indices are not ROM pointers.
6,144 original actor updates cover two owned slots, stopped/moving camera,
retained position fractions, both facing attributes, ordinary/group deaths,
repeated hops and offscreen release. Host and RAM-loaded native 68000 comparisons each pass 153,600 bytes.
Shared state $FF and the global $A92E bonus conversion remain outside this
ordinary-actor contract. The first trace accidentally inherited the source
bonus flag $A1; it is retained as `i-actor-oracle-uncontrolled-purge.jsonl`.
The corrected fixture explicitly clears that flag each update.

The initial isolated components have now been connected to native gameplay,
as detailed below. Reproduce the original component comparisons with:

```
python3 tools/compare_arcade_i_wave.py
python3 tools/verify_i_wave_target.py
python3 tools/compare_arcade_i_actor.py
python3 tools/verify_i_actor_target.py
```

The two target scripts compile isolated native functions and load them into
Genesis RAM in windowed/headless MAME; they record separate machine-code hashes
and do not imply those functions are linked into the playable cartridge.


### Class-I gameplay integration and rewards

Cartridge `43ee509f680134b28111e3bc79f9196c2ba14df6b0f589c5d9dadd503f43e8a4`
replaces the old class-I single-thrower fallback with four owned slots20..23.
The general pool starts24; fixed player shots remain16..19. Source actor
fractions survive slot reuse; live count increments at actor initialization and
decrements at release. The controller follows G/H after actor/collision updates.

Original $EA3C hit/reward and $4CBE score execution establish 50 points per hit,
500 extra at reservation zero, combat increment $40 plus another $40 for group
clear, source events $0423/$0413, sound commands $0C/$0E, hit style by owner,
actor active2/state4/phase0 or4, and shot active2/state2. All256 incoming
reservation values, two owners and eight combat boundary values pass4,096
source/host/linked68000 cases (28,672 fields). Original $EA0E scanner tests
pass1,024 two-shot/four-actor cases, including inactive/death/puff flags and
stable slot selection (1,255 native selectors,352 hits). Its wrapped geometry
matches the separately verified G/H bounds. $E2D1 scans I before H and G;
this ordering now holds in the native collision pass.

The integrated `entity_update_all` fixture verifies64 consecutive updates,
births at updates1/14/27/40 and visible initialization one update later, plus
four owner/group-clear collision cases deliberately overlapping H/G:740checks.
Source script pose extraction, original graphics pixels, the linked tile
selector and SAT attributes pass35cases/4,480pixel bytes. Sprite tiles occupy
290..729, backgrounds730..1304, below the font at1376. G/H graph/collision art
regression, shot-pool exhaustion/reuse, G/H reward ordering and14mode-control
checks also pass on this cartridge.

The source bonus state $FF and global $A92E conversion, player contact and
ordering against other unfinished families remain incomplete. Integrating this
group does not establish whole-game parity. Genesis Plus GX course workloads
(42,000refreshes, one wrap each, lives refilled) report zero misses for1P and
two misses for2P; rolling floors60/58 and CPU peaks74.07%/95.36%. Changed enemy
behavior changes the workload, so this is not an equal-work optimization claim.
Strict60both remains unfulfilled. Reports: `i-course-results.json`,
`i-integration/results.json`, `i-render/results.json`, `i-reward-comparison.json`,
`i-scan-comparison.json` and their target reports under `out/parity-audit/`.


### Ordinary G/H/I player-contact correction (2026-10-22)

Cartridge `85401c8c6d790ddf76bcab0c4dc96882fe63e315ff75ce8b07c0b02464678898`
replaces the generic 16-pixel AABB for ordinary G/H/I contacts with the original
$ECE9 scan geometry: unsigned byte `(playerY-enemyY+7)<=14` and unsigned word
`(playerX-enemyX+7)<=14`. Player active must equal1. Enemy active must be nonzero
and have bit1 clear; the original does not exclude enemies by major AI state.
The native ordinary-contact helper separately excludes bit6 bonus objects,
whose collection transaction is not implemented yet. Original G/H/I wrappers
for both owners cover6,060 cases, including X/Y wrap, signed offsets at both
boundaries, eight actor flags, four major states and four player active flags.
Host and linked68000 comparisons have zero differences (2,772 lethal cases).

The native collision loop now checks H, G, I in source order and bypasses their
old generic AABBs. Current native alive/invulnerability/page ownership gates
remain; their full correspondence to original lifecycle flags is still unproven.
Instadeath keeps its existing contact rules. The entity-loop fixture checks300
I contact combinations with900 assertions: +/-7 and +/-8 boundaries, both
owners, active1/2/128, and invulnerability. G/H reward and I spawning/reward
integration regressions also pass on this build. This verifies a contact rule,
not source death/respawn/energy logic or all enemy families.

Reproduction: `tools/arcade_contact_trace.lua` (original CPU),
`tools/compare_arcade_contact.py`, `tools/verify_contact_target.py`, and
`tools/verify_contact_integration.py`. All emulator runs are windowed/headless.


The subsequent `c14d4a5d` cartridge skips empty and bit1/bit6-marked enemy slots
before calling the same ordinary-contact helper. This avoids redundant argument
setup for rejected contacts. The linked 6,060-case contract and 300-case
integration fixture pass again. A two-player workload with inputs keyed to
simulation updates matches all 2,110 captured player/entity bytes for each of
4,000 updates against `85401c8c`, with zero differences. This comparison covers
captured gameplay state; it does not prove whole-game arcade parity or physical
hardware performance. The baseline cartridge and symbols are retained under
`out/parity-audit/contact-before-cull/`.

Latest course measurement (`contact-course-results.json`, cartridge `c14d4a5d`):
42,000 refreshes and one wrap in each mode, with lives refilled. 1P has zero
missed presentations, rolling floor60 and peak CPU76.71%; 2P has six misses,
rolling floor57 and peak CPU97.02%. The cull reduced peak load in these runs but
did not remove two-player misses. Strict60 in both modes remains unmet.


## Player death lifecycle recovery (2026-10-22)

`src/arcade_life.c` implements the verified portion of the original $AD9B
player-kind dispatch: states0..6 through left-edge reentry setup. It is a typed
native component, not yet connected to `game.c`. The existing `player_respawn`
comment referring to $B1C2 is incorrect: player death/entry uses $AD9B/$AE72.
The current playable preview therefore still has approximate life flow.

The original starts active2 and sound command $0A, loads the owner's death
animation, sets timer60 and decrements it immediately. Following the animation
hold, Y decreases two unsigned pixels per tick until the offscreen $F8 clamp.
It then loads the idle pose and checks reserve lives. Zero reserves emits
$020E+owner, sets collision-page gate $88/$89 and clears player kind. Otherwise,
the next state decrements reserves, emits $0C00|owner, then initializes reentry
at camera-8 and internal Y104, facing1, with a sixteen-update input lock. The
source preserves position fractions and applies +141/256 horizontal drift after
the state routine whenever scrolling is not stopped. Source $8F8C..$8F91 copies
configured lives minus one into $58/$59, so reserves exclude the current life.

Original CPU traces cover224 trajectories: both owners, reserves0/1/2/255,
stopped/moving camera, seven starting Y values and two fractional positions,
including word wrap. They exercise states0,1,3,4,5,6. State2 shares the same
$ADFC jump-table target as state1 but is not reached in these ordinary death
traces. Host and RAM-loaded native68000 comparisons pass24,944 updates and
548,768 compared bytes each, including animation, life counters, queued event
and sound command. The 760-byte native component is tested separately from the
playable cartridge; reports do not claim integration.

Remaining work: $AE99 input-sensitive reentry state7, $AF62 automatic path,
$AD10 firing during reentry, $AACF immediate movement takeover, complete
start/join/continue ownership, source collision-page gates, original visual
resources and lifecycle integration. The custom Instadeath mode needs its own
intentional life policy when the normal lifecycle is replaced.

Reproduce with `tools/arcade_life_trace.lua`, `tools/compare_arcade_life.py`
and `tools/verify_life_target.py`. The fixture stops before state7 executes.
`out/parity-audit/life-comparison.json` and `life-target/results.json` bind the
original trace, source and compiled component hashes. The playable ROM remains
`c14d4a5dfd1e2de54387d43ff2c7bbb1ebb73057f78c2fe78d24199c891b4e47`.


### Input-sensitive reentry recovered

`arcade_life_reentry` now implements $AE99 state7 with a semantic path index,
not a ROM address. Source $AF62 is direction1 for152 updates, direction5 for80,
direction1 for28, then stop. The source vectors are +288/256 and -288/256 X;
this ordinary path has no vertical movement. Phase0 falls into its sixteen-tick
countdown; the transition to path phase2 also moves and decrements on the same
update. Phase3 counts six ticks at clock&63==0 and changes the sprite code late
in the wait. The final reset or held direction input restores active1/kind0.

The sixteen-update lock suppresses firing/input takeover until the following
update. Once unlocked, the source calls AD10 before takeover and before the
outer scroll drift. Held direction bits terminate the return path. AACF then
selects state2 for fresh Up, or state3 for fresh Down above Y40; otherwise it
leaves normal state0. **AACF does not move the player immediately.** Earlier
research wording describing immediate movement was incorrect. Normal state0
at AA03 still needs integration in the native locomotion helper before this
handoff can be connected safely.

The original CPU fixture covers80 trajectories, both owners, stopped/moving
camera, two fractional starts, neutral expiry, held/fresh direction input,
Up+Down priority, the Y40 Down boundary, fire-only input and combined fire/move.
Host comparison passes14,488 updates and1,260,456 bytes, including624 original
shot births through the already verified firing helper. RAM-loaded native68000
reentry passes333,224 bytes across the same14,488 updates. The earlier24,944
ordinary death updates also pass again on the current source. Neither component
is yet connected to gameplay; the playable ROM remains unchanged.

Reproduce using `tools/arcade_reentry_trace.lua`,
`tools/compare_arcade_reentry.py` and `tools/verify_reentry_target.py`.
Reports and source-derived target cases are under `out/parity-audit/reentry-*`.
Next integration must replace fixed blink/grace timers with original visual and
kind/active transitions, preserve fire pose/input-edge order, bind reserve lives
and queued events, and keep the deliberate Instadeath life policy separate.


### Normal player initialization and unsigned clamp

The playable build now implements the locomotion portion of $AA03 normal state0:
active becomes1, attached clears, and state advances to1 without movement.
The enclosing $AB52/$AB70 bounds use unsigned camera-relative subtraction.
A position twenty pixels behind the camera therefore wraps and clamps to the
right bound (camera+208), rather than the left bound. The former signed native
comparison was incorrect.

Original CPU fixtures cover1,344 cases across both owners, camera word wrap,
seven relative positions, direction combinations, input history, fractional
positions and shoulder eligibility. Host, RAM-loaded68000 and the linked
cartridge each match22,848 compared bytes. The previous c14d cartridge fails
case0, confirming this fixture detects the regression. Animation initialization
and complete death/reentry gameplay ownership remain separate unfinished work.
The existing controller-driven two-player replay also passes1,280 updates and
20,480 field checks.

Reproduce with `arcade_player_init_trace.lua`, `compare_arcade_player_init.py`,
`verify_player_init_target.py` and `verify_player_init_linked.py` in tools/.
The new cartridge and local preview SHA256 is
`0224e9f5bf7d8ce8d081b18ec20adc53a84a71da52003a4383ec0da328d7174a`.
Headless Genesis Plus GX course checks cover42,000 refreshes and one course wrap
per mode: 1P zero missed presentations, rolling floor60, peak76.71%; 2P six
misses, rolling floor57, peak97.07%. Strict60 in both modes remains unmet.
Results are in `out/parity-audit/player-init-course-results.json`.


### Death/reentry resident art prepared

The asset extractor now walks AFFC/B00A death, AFAC/AFD0 idle and AF9A/AF9F
entry animation graphs. All16 source code/palette pairs already existed in the
resident sprite pool. The new semantic `arcade_life_tile` selector reuses them;
no additional VRAM tiles are needed. Old gen_anim.py claims that the player
vanishes without a death animation were based on incomplete captures and are
corrected. The legacy PLAYER_DEATH placeholder remains until lifecycle binding.

`tools/verify_life_assets.py` independently traverses the six source graphs,
host-compiles the selector, and compares2,048 pixel bytes with the linked ROM.
It passes all16 poses; this does not claim that the selector is called by gameplay.
The existing enemy art and full collision-column check also passes25,464 bytes.
Sprite tiles remain290..729 and background tiles730..1304, below font tile1376.
The rebuilt ROM is `5a5e0e1f6da193de0078d2cac1d113eacd4ea44b043d75ae5b73c850fbcf36af`;
the local playable preview remains the previous tested0224e9f5 build. Performance
has not been remeasured on this asset preparation build.


### Ordinary death/reentry bound to game_update

Normal-mode contact now arms a typed life owner after the entity pass. Its first
AD9B update happens on the next player tick. Public lives include the current
life; the bridge passes lives-1 as source reserves, applies the state5 decrement,
and waits for the source game-over event after the fall. The original0A death
command is emitted by the source transition. Instadeath retains its intentional
one-life immediate termination. Start/join/continue still use the legacy path.

Reentry uses the entity phase clock and samples held input throughout the locked
period. It captures the pre-drift firing pose, then hands normal movement state
back when source kind clears. Life-owned players are excluded from shoulder and
separation eligibility. Render uses source code/attr and suppresses legacy blink;
coordinate rebasing also shifts the life owner's X. Ordinary scrolling is always
enabled in the current game; original stopped-scroll lifecycle gating is pending.

`tools/verify_life_integration.py` passes9,368 source-derived death updates through
linked `game_update`, covering both owners and reserves0..2 with moving camera.
It checks all17 actor bytes, displayed life count and flow. This injects states;
it does not establish natural contact reachability, reentry integration, final
SAT behavior or start/continue parity. Those are next checks. The existing life
asset comparison also passes on this build. No fresh performance claim is made.
Current working ROM SHA256:
`6cc8d6ff3e0245b28fbe5b99092026578453c2423f6b643dcef313fe24dedd8f`.
The user-facing preview remains the earlier0224e9f5 build until the new lifecycle
has broader integration validation.


### Reentry full-loop comparison

`verify_reentry_integration.py` passes14,488 injected source cases on6cc8d6ff:
7,244 moving-camera states repeated with the teammate out and active. Both player
owners, all return phases, neutral expiry, held/fresh input and fire edges are
covered. Checks include all17 life bytes, path, ownership, flow, lives, direction
history, shot presence after the entity update, public positions/fractions and
sprite flip. The source camera must accompany each fixture: using camera0 can
correctly cull a shot after birth and is not a valid comparison.

This verifies the lifecycle bridge and teammate isolation, not complete natural
contact reachability, exact shot-birth fields, hardware SAT output or continued
normal animation timing. The current full-course performance check reports14
missed1P presentations over42,000 refreshes, rolling floor53, peak91.41%,28 maximum
entities. The changed death lifecycle also changes the combat workload. Two-player
measurement is still running at this checkpoint; strict60 remains unfinished.

The2P measurement has now finished:43 missed presentations, rolling floor39,
peak108.40%,maximum26entities over42,000 refreshes and one course wrap. Both modes
fail the strict presentation gate. Full results: lifecycle-course-results.json.


### Fixed shot-slot scan optimization

A16,000-update profile of6cc8d6ff measured the entity/collision/scheduler section
at5.884ms mean and12.443ms peak. Ordinary AD10 creates player shots only in
16/17(P1) and18/19(P2). The G/H/I shot collector now reads the current owner's
two slots, retaining fruit/semantic slot ordering and eligibility. The generic
enemy collision pass likewise scans those two shots. Survival retains its whole
pool traversal. This removes two unrelated48-record searches each ordinary tick.

On ec9f3f13,16,000 simulation-index updates match all2,110 public player/entity
bytes against6cc8d6ff. The same section measures5.247ms mean and11.775ms peak,
a mean saving of0.637ms. Reward integration passes16cases/192checks on both ROMs.
Its old fixture incorrectly injected P2 shots into P1 slots; the fixture now uses
the source-owned owner pair while preserving its reversed semantic-slot test.
The original fixture failure is saved, not counted as a production failure.

The independent42,000-refresh course tests improve from14/43 missed1P/2P
presentations to1/14, rolling floors59/55 and peaks83.94%/105.76%. Refresh-driven
input means course workloads can diverge as misses change; fixed-work comparison
above establishes the controlled timing saving. Strict60 in both modes remains
unmet. Reports are fixed-shot-equivalence/profile/course-results.json in the
parity audit directory. Current ROM SHA256:
`ec9f3f1316720f1f331da1a1bda249f17a694aae29445284d18654eb6432fc38`.
The user-facing preview remains0224e9f5 until broader lifecycle validation.

Code inspection also identified a remaining lifecycle bridge gap: enemy target
kind/active is reconstructed from old flow categories in entity_update_all and
does not retain kind1/active2 during source-owned reentry. Correct and validate
that source contract before treating current enemy behavior as final.


### Enemy target flags during source-owned life flow

`game_player_target_flags` now supplies the source active/kind pair while the
AD9B/AE99 owner is active. The old FLOW_RESPAWNING mapping incorrectly produced
active1/kind0, allowing normal targeting during kind1 reentry. The original
reentry comparison now also checks the enemy world snapshot X/Y/active/kind;
the earlier ec9f3f13 ROM fails case0. Source death checks additionally verify
active/kind until the game-over event. Legacy start/join/game-over fallback is
retained pending recovery of those contracts, not claimed as source-equivalent.

On6878a94a the death9368case check passes and an isolated reentry14488case run
passes. An earlier post-build run reported contradictory case0 failure; it is
not silently treated as success. A fresh repeat with stale result removal is
being run to confirm reproducibility. The emulator result file is now removed
before invocation so a run that produces no report cannot reuse older evidence.

Course tests on this build:42,000 refreshes per mode, one ribbon wrap each;
1P1miss/floor59/peak81.84%;2P26misses/floor42/peak106.05%. Corrected source target
flags change enemy decisions and workloads; these numbers replace older results
for the current build. Full-game fidelity and sustained60 remain unfinished.
ROM SHA256:
`6878a94a0fcf6ea144ef88f07e3e2f23becb66b36fab9961e06a6d4fd80b1527`.


The stale-result-free repeat exited without a report, so it is incomplete rather
than a gameplay mismatch. With explicit autoboot_delay0 and progress/final markers,
the next fresh run completed all14,488 cases with no errors. Its log contains
REENTRY_FINISH14488 and the report binds6878a94a. The old post-build case0 report
cannot establish a current mismatch without a fresh execution result; the
incomplete run's underlying exit cause is not yet established. Current positive
evidence is the complete logged fresh run, not the stale report.


### Contact-to-death ownership verified

`verify_contact_life.py` seeds an overlapping class-I enemy and calls the linked
full game_update twice. Both owners and lives1..3 pass six cases/108 checks:
contact is consumed at tick end, life ownership starts at state0phase0/active1,
and the next tick enters phase1/active2 with timer59 and original42/4A sprite.
Lives remain unchanged on both ticks, including the last life; game over does
not occur at impact. This proves seeded contact integration, not a natural
playthrough or other enemy families. ROM remains6878a94a.

MAME Python launchers now explicitly use window, nomaximize and512x448. Headless
checks retain video none. The contact test exited and no SonSon MAME process
remained afterward. Unrelated Super Contra processes were left untouched.


### Death pose render preparation checked

`verify_life_render.py` selects the12 distinct code/attr pairs reached in the
original death-state0..6 fixture: eight death poses, two initial idle poses,
and two entry poses. Each is tested with legacy flowTimer0 and255, for24 cases.
The compiled full game_update selects tiles whose linked bytes match the
independently decoded original graphics. Full game_render then prepares the
expected SAT X/Y, palette1, no horizontal/vertical flip,2x2size and player link.
All24 cases pass on6878a94a. This detects legacy timer-based disappearing sprites
in the prepared SAT. It does not claim VDP DMA delivery or phase3 reentry blink
coverage. The fixture resets queued transfer counters between render calls and
holds the background span resident; it is not a cold-boot rendering test.
MAME ran with window/nomaximize/512x448 and video none, and exited afterward.


### Remove invented normal firing recoil

Original AD10 execution across64 availability/input/history/kind/state/facing
combinations leaves the48-byte parent player record unchanged. Successful fire
calls6424 and queues sound08 through6341. Checking the completed sound queue
avoids treating a prefetched instruction read tap as a completed register event.
The fixture is arcade_fire_effects_trace.lua; FIRE_EFFECTS_DONE and64 rows prove
completion. It covers eight successful requests and56 rejected/held requests.

Normal game.c firing now uses08 and does not arm the invented fireAnim/recoil
pose. Survival keeps its custom behavior. Four linked full-update cases for both
owners and fresh/held fire verify shot eligibility, zero recoil timer and the
unchanged normal pose,20checks. Animation cursors are initialized before the test;
a zero timer would correctly advance the preexisting legacy animation frame.
This does not establish full normal movement animation cadence or audio waveform
fidelity; those remain pending. Current ROM:
`6517e688101f195357b4fabdeb191a7f7e2f9f594f51a89415d7b68faaebb52c`.
Performance has not been remeasured after this change; preview remains unchanged.


### Normal player animation follows source movement calls

Original normal locomotion uses the AFAC/AFBA and AFD0/AFDE walk graphs, including
while climbing. AAB5 changes direction/graph and loads the first pose immediately;
AA6C movement, AB52 left clamp (except state5), and A9C5 eligible drift each call
9617. AA03 clears the timer and advances the existing cursor. Multiple calls can
occur within one update.9617 decrements a nonzero timer without changing pose;
only a call with timer0 loads the next pose and duration4. Separate climb frames
and unconditional once-per-update ticking were incorrect in normal mode.

`arcade_player_step_animated` adds an optional typed animation owner to verified
movement. Existing motion-only callers retain the wrapper. Normal game.c now uses
this state and semantic tiles, while Survival retains its custom animations.
Death/reentry release copies the source idle cursor/timer/code/attr into normal
animation ownership. That handoff is implemented but needs its own continuity
test; initial/start/continue ownership and stopped-scroll gates remain incomplete.

New original controller-driven two-player trace includes animation fields for
1281 snapshots. Host comparison matches1280 updates:20480 motion and10240 animation
checks, including228 shoulder-riding player frames. The linked Genesis cartridge
replay matches all30720 fields as well. Tools: arcade_player_animation_trace.lua,
compare_player_animation.py,verify_player_animation_target.py. This covers the
recorded input sequence, not all possible normal initialization edge cases.
Current ROM SHA256:
`391e1873a62de0b32b15c53f5a824bca3b1a74240d19f634fa4bd3f53624382c`.
No performance claim is made for this new build yet. Emulator sessions closed;
local playable preview remains the prior tested0224e9f5 while parity work proceeds.


### Animation initialization, reentry handoff, and current pacing

On ROM391e1873a62de0b32b15c53f5a824bca3b1a74240d19f634fa4bd3f53624382c,
the extended reentry integration fixture passes14488 injected full-game updates,
including80 releases back to normal animation. It poisons the destination animation
state before every update and checks owner, next cursor, timer, code and attributes
against the source-derived reentry result. This verifies the transfer itself;
controller-reachable death-to-normal continuous trajectories remain a separate gate.

The original1344 state0 initialization cases now include animation in their exported
fixture. Host and RAM-loaded native68000 component comparisons pass29568 byte checks
each. Target code4322 bytes; fixture SHA256
c558091df41de36fb0ff01651bc2a8deceaf7721210c4e49e5e4a2ff1d714658.
Tools: compare_player_init_animation.py,verify_player_init_animation_target.py.
This target test runs a separately compiled component, not the linked gameplay path.

The current391e build completed42000 refreshes per mode in headless Genesis Plus GX,
with one course wrap each and refilled lives.1P missed1 refresh, worst60-refresh
window59 presentations, peak measured load82.2266%.2P missed26 refreshes, worst
window42 presentations, peak106.543%. Strict60 FPS therefore fails both modes.
These are refresh-driven workloads with partial enemy AI, not final arcade-equivalent
stress coverage. Preserved report: out/parity-audit/player-animation-course.json.
The playable preview remains unchanged. All test-owned emulators exited.


### Original player startup contract recovered

`arcade_start_trace.lua` executes original MC6809 $9094 over162 combinations of
camera0/255/65520, session flag99 and out flags88/89 in0/1/255, and zero/A5
preexisting records. `compare_arcade_start.py` checks all98 bytes at047E..04DF:
15876 comparisons, zero differences. Trace SHA256
1101f07afbc0c9b514e21ff7a2c5511a9706239077cdeeed2d240be68f7b8536.

P2 active is always cleared and kind/owner set to0001 first; it initializes only
when99 is nonzero and89 is zero. P1 initializes when88 is zero; an out P1 record
is otherwise preserved. Enabled players receive active1/state0/phase0, X=camera+32
with word wrap, upward Y104/136, facing1, AFAC/AFD0 cursor and animation timer0,
shot bases04E0/0520 and availability2, plus reciprocal teammate pointers.
Fractions, animation code/attributes, input history and other unassigned bytes
are retained. This routine does not tick animation; normal AA03 does that later.

The old game.c comment incorrectly identified B1C2 as player respawn. B1C2 is a
substate table reached from the separate06F8/0718/0738 object pool via B19A/B1A7.
Removed that false attribution; no runtime change is made here. Cold native setup
still starts state1, and join/continue still use legacy right-edge placement and
grace timing. Source call-site reachability and native binding remain necessary.


### Native cold-start correction

Normal cold startup now enters state0 (AA03 first-update initialization) and places
the players at screen Y135/103, matching source upward Y104/136. Legacy setup used
state1 and platform-table Y136/104, one pixel low. Survival keeps its custom setup.
ROM54495fe98f407d791993a25664534d00a4b816ebbe2ad2c1b693012e4e743eaa.

verify_cold_player_start.py drives the real Home menu into1P/2P games and observes
initialization without seeding player records. It checks X/Y/fractions/state/phase,
starting animation cursor/timer and shot availability; after the first complete
update it checks idle drift32+141/256, unchanged Y, state1phase0, cursor1timer3 and
source poses40/48 with attributesC0/C1. The391e baseline fails at initial P1Y136;
the new build passes. Existing1280-update linked movement/animation comparison also
passes30720fields. These tests establish cold start and its first idle update,
not join/continue or all Arcade front-end eligibility paths. No new performance
claim; prior391e timing failures remain recorded. Local preview is unchanged.


### Mid-game joining is a requested console extension

The actual-input arcade_join_probe.lua starts a credited1P game, inserts one more
coin during gameplay, then presses2 Players Start twice. Over301 observations
(ticks0..300), the original accepts the coin (credit word25:26 increases0to1),
reads both Start pulses (system3C bit1), leaves the credit available and keeps
P2inactive/out with session99=0. Hostile records are cleared to isolate inputs;
player records are untouched. compare_arcade_join_probe.py verifies these facts.
This bounded observation does not prove every possible join/continue condition.

Static context:92B0 is the credited start owner called from the title/high-score
front-end;97AB normal gameplay calls9A2C,A5E2,E2B3,EF51,FED8 and checks both-out
flags88/89. The tested original gameplay path does not handle these Start presses.
The user's explicit per-controller join requirement remains implemented as a
console extension; do not remove it or present its guessed legacy placement as
source behavior. No ROM changes in this investigation. Current working ROM54495
and historical performance failures remain as documented. Source probe exited.


### Preserve final-life cleanup and enemy-facing flags

Original A98A visits P1 even after flag88 becomes1, but skips P2 when flag89 is1.
After AE41, kind0/state0400/active2 therefore enters ACCE on P1's next update:
activeFF/phase1, then ACD5 clears active/state/phase on the following update.
Normal clamp/animation/drift still run on both cleanup updates. Further visits
return early with active0. OutP2 retains active2/state0400/kind0 because it is
skipped. The old native code discarded the source record on the game-over event
and exposed invented inactive/kind1 flags.

arcade_gameover_trace.lua records96 source updates: both owners, cameras0/65520,
fractions0/255, animation timers0/4, teammate inactive/alive, and three successive
cleanup visits. Source trace33f31f769b28300fa9f3cd48e8374acee83aa5cf39c7bbdd3b4247c1fd69d7d3.
verify_gameover_integration.py compares native life/motion/animation records and
enemy-world flags through full game_update, with idle input. All96 pass. The
54495 baseline fails the original48-case inactive-teammate fixture immediately.

Native normal motion now implements state4. life_owned value2 keeps terminal
source ownership, P1 executes cleanup and P2 freezes. Motion loading preserves
source active flags when the other player updates. The fixture sets both public
player owner indices explicitly; an earlier zero-filled teammate mistakenly
aliased P1 and was corrected before the final96-case run.

Current ROM6b74c8cb097f67b4086a70d88ee2f9ee577eeae73313992c5bf5818d2a9e1548:
9368 linked death cases pass including enemy flags on the final-life event
(previously excluded), and1280 linked normal animation/movement updates pass
30720checks. Natural chained death-to-cleanup, non-idle terminal input, stopped
scroll, whole-game timing and rendering remain separate gates. No current-build
performance claim; older strict60 failures remain. Preview unchanged; emulators
ran headlessly and exited.


### Current performance baseline and rejected compiler experiment

On6b74,16000 fixed completed2P updates measured entity/collision/scheduler mean
5.3798ms, peak11.8512ms. Players mean1.3788ms/peak2.6611ms; entity SAT
0.9160ms/3.6641ms. These are elapsed emulated times including interrupts.
Report preserved at out/parity-audit/before-next-optimization/profile.json.

Preventing inlining of entity_update_one produced candidate2ba86cf0... and added
2304 text bytes, crossing the512KiB padded-ROM boundary (640KiB result). Expanded
fixed-work comparison passed16000updates/2832bytes each, including private player
motion/animation/life, world, foes, weapons, Iactors and wave states. Genesis Plus
GX automated startup never reached gameplay on this candidate (level_width0,
ticks0); the restored6b74 does. Root cause is not established. The experiment was
rejected and reverted; this is not a measured speed regression. Artifacts and
failure logs remain under out/parity-audit/noinline-entity-experiment/.
The stronger comparison and startup diagnostics are retained. The second-core
runner now rejects fewer than61samples, required for its60-refresh interval metric.

Restored6b74 completed42000refreshes/mode in Genesis Plus GX, one course wrap each:
1P1miss, worst60-refresh interval59presentations, peak82.5195%;2P58misses, worst
interval48presentations, peak109.2285%. Strict60 fails both. Preserved report:
out/parity-audit/gameover-course.json. These refilled-life, refresh-driven inputs
with partial enemy AI are not a matched final arcade workload. Do not attribute
changes versus391e solely to CPU cost: behavior and refresh-dependent input timing
also changed. No ROM optimization retained this turn; preview unchanged. All
launched tests ran headlessly and exited.


### Entity-block profile on unchanged cartridge bytes

Per-function entry/RTS and stack-matched return experiments produced unmatched
entry/exit events (tail calls/prefetch were not reliably accounted for). Their
timings are invalid and discarded. tools/profile_enemy_functions.py now measures
unconditional boundaries between actor updates, collision handling and scheduling.
The global assembly labels profile_collision_begin/profile_schedule_begin emit
no instructions: rebuilding preserves ROM SHA2566b74c8cb097f67b4086a70d88ee2f9ee577eeae73313992c5bf5818d2a9e1548 exactly.

All16000fixed-input updates produced complete positive intervals. Mean/peak ms:
actor updates3.1539/6.8489; collisions1.7785/4.7729; scheduler0.4474/1.9305.
These include interrupts and are measurements of this partial native workload.
Reports: out/parity-audit/enemy-block-profile/results.json and peak-actors.json.
The slowest actor update13809 has27active records: five type7, six type1,
four player shots, two items, two type9, three flyers and five type8. This ties
further work to an observed busy scene; it does not establish arcade reachability
of all these legacy actor approximations. No speedup or new60FPS claim this turn.
Headless test exited; playable preview unchanged.


### Equivalent AABB interval optimization retained

Generic native overlap now uses one unsigned interval test per axis, preserving
strict edge exclusion: ax-bx lies in[-aw+1,bw-1]. Subtraction remains32-bit, so
opposite signed16 coordinate extremes cannot wrap into a false hit. Every caller
uses positive box sizes. This optimizes existing native rules; it does not prove
that the remaining legacy archetype collision boxes equal the arcade's.

check_aabb_equivalence.py compiles the actual helper with address/undefined
sanitizers and compares the former four-boundary definition. All feasible signed16
axis differences with sizes6/8/16 plus1000000full rectangle cases pass3359278checks.
The linked16k fixed-update comparison matches2832public/private state bytes per
update. Current8cb7d97d48c5098851bc4038b3e7a5b6be40b6b84d17102f7e828d2748389b5f
keeps the512KiB ROM and unchanged RAM allocation.

Matched MAME collision timing improves mean1.778513to1.765726ms (0.72%) and
peak4.772860to4.667521ms (2.21%). Actor and scheduler timing are unchanged.
Genesis Plus GX42000refreshes/mode still misses1refresh in1P and58in2P, worst
60-refresh presentation counts59/48; measured peak loads82.5684%/110.2539%.
Thus strict60 remains unmet; do not present the small collision gain as a frame-rate
fix. Refresh-driven course results are not controlled fixed-work CPU comparisons.
All experiment evidence is collected in out/parity-audit/aabb-optimization/results.json.
Local playable preview unchanged. Headless emulators exited.

## Large-enemy durability — original CPU evidence and linked fix

Original main MC6809 program SHA256
`09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1`:
`D58B` initializes `$8,X` to seven. `EE8E` tests overlap, consumes the
shot (`active=2`, major/minor `2/0`), sets `$9,X=1`, and decrements the
byte at `$8,X`. Only 1→0 enters enemy death (`active=2`, major/minor `2/0`)
and records the shot owner. Zero wraps to 255; it is not already dead in
this leaf. `D58B`'s `$D,X=16` is an entry countdown, not box size.

`arcade_large_damage_trace.lua` executes original routines without ROM patches.
`compare_arcade_large_damage.py` verifies 4,610 cases: two initializer cases,
both owners × all 256 HP bytes × nine overlap/boundary inputs. Trace SHA256:
`456d72b0f4d4ff8bd69877416b7e1b910e6ddb6369f89c069b25287522197b9c`.
The source overlap uses enemy+8 minus shot: horizontal accepted offsets
−28..12 inclusive, vertical −28..11 with byte wrapping. Native legacy AABB
has NOT yet been replaced with this contract; source/native coordinate and
composite sprite origins must be reconciled first.

Changed LARGE_HP 3→7 and clear large HP on its final hit. Linked
`verify_large_damage_integration.py` calls the real allocator, then seven
successive full entity updates for each shot owner: 46 assertions pass.
Baseline `8cb7d97d48c5098851bc4038b3e7a5b6be40b6b84d17102f7e828d2748389b5f`
fails immediately on initial HP=3. Candidate:
`7d1e8b8735aa5bc00d61375b5a163fe74057ff8b9621a345f9d5d762c0f172d1`.
This fixture injects an encounter, so natural reachability is not proven.
Legacy movement, damage flash, composite rendering, reward selection, source
collision scanner order and full death sequence remain unresolved. No new
60 FPS claim. All emulator processes used headless video and exited.

## Large-enemy movement recovery — full original dispatcher trajectories

`arcade_large_motion_trace.lua` now captures 48 controlled trajectories of
768 updates each through original `D55B`, with three subtypes, difficulty high
bytes 0/8/32/56, one/two players and `$9D=0/1`. With `$9D=0`, the fixture
advances camera by 141/256 per update (the `A697` increment); with `$9D=1`
it holds camera still and the dispatcher supplies the corresponding actor
drift. Player coordinates are scripted relative to camera. Four projectile
slots are captured then freed each update; child AI is not executed.

`compare_arcade_large_motion.py` is a research semantic reference, not shipped
native logic. It matches 589,824 field values over 36,864 source updates:
position/fractions, active/major/minor, subtype, direction/facing, HP,
countdowns and selected target pointer. Current trace SHA256:
`97c0f09349267aa510be4e335e1485d2c4b0db60e7fb20407e4058cd1f822586`.
Observed input states include entry 0/0,0/1,0/2, pursuit 1/0,1/1,
retreat 3/0,3/1 and offscreen exit 2/4,2/5,2/2. There are 87 recorded
projectile births; this does not yet validate a native projectile producer.
An earlier fixed-camera fixture/report is preserved under
`out/parity-audit/large-motion-static/`; it did not cover retreat.

Recovered movement requirements:
- Entry initializes a 16-update countdown, then chooses a subtype vector;
  it does not periodically reverse direction.
- `D612` compares unsigned camera-relative X to a difficulty/subtype table.
  Subtype is added as a BYTE offset, without word scaling; preserve this
  surprising original indexing rather than correcting it by intuition.
- Pursuit reselects its player/vertical direction every 48 updates, moving
  immediately on the selection update. The selected player is the eligible
  player farther right, with P1 winning ties. Source target-kind/inactive
  branches are still outside these trajectories.
- Pursuit lifetime decrements only when source clock low six bits are zero.
  Expiry enters retreat; retreat uses different vectors and reselects the
  vertical target each update. The low byte of relative X >=232 triggers exit.
- Source HP=1 can invoke the RNG steering override at `D706`; that branch is
  deliberately rejected by this reference until its contract is captured.
- Four-cell animation (`D52A`/`D924`), aligned firing (`D74A`), full kill-death
  states, scheduler's three owned actors (`FB2B`) and render origins still need
  translation and integrated checks. Existing production lane patrol remains
  unchanged in this turn; the reference is groundwork for its replacement.

## Large-enemy player selection and low-HP steering

`src/arcade_large.c` introduces typed native helpers for original `AD53` and
`D6F3`/`D706`/`95F4`. `arcade_large_target` returns the original player index
and an independent eligibility result. Eligibility is `(~kind & active & 1)`;
P2 is considered only with tracking enabled and wins only when eligible and
P1 is ineligible or P2's unsigned X is greater. With no eligible player, the
original pointer still names P1. Pursuit ignores that carry result; retreat
uses it, so callers must preserve the distinction.

Low-HP steering starts with unsigned byte `(enemy_y+12)<target_y`, choosing
4 or 2. Only HP=1, changed direction and clock bit5 clear invoke `95F4`.
The 16-bit random state shifts left, inserting complemented high/low XOR
bit6 into bit0, and writes the low byte to retained sample `$7A`. The odd
candidate 1 or 3 replaces the vertical direction unless it equals the prior
direction. Other paths preserve both random state and last sample. This helper
does not own a separate RNG; eventual integration must pass the shared state.

Original `arcade_large_steering_trace.lua` supplies 8,192 independent CPU cases:
tracking on/off, active flags 0/1/2/255, kind flags, X ties/boundaries, Y wrapping,
HP 0/1/2/7, five prior directions, clock phases and deterministic seeds.
`compare_arcade_large_steering.py` matches 40,960 native C outputs, including
699 RNG advances, 7,493 retained states and 163 cases rejecting the random
candidate because it equals the previous direction. Trace SHA256:
`aa9c97214764f43a80ff11f1206225f9e8a84e24383b08bfdd12f4229c588ee9`.

These are component helpers, not yet called by gameplay. The linker discards
unused functions, so production ROM remains
`7d1e8b8735aa5bc00d61375b5a163fe74057ff8b9621a345f9d5d762c0f172d1`.
`verify_large_steering_target.py` independently compiles the helpers for the
68000 and runs a RAM-loaded marshaling wrapper against the same source cases;
its machine-readable result is `large-steering-target/results.json`. Check
that report for completion; compilation alone is not a passing target test.

Target result confirmed: all 8,192 cases pass, 498-byte component including
wrapper, code SHA256 `3f18c06c3edacce073e447ce3701ea174faad1055d789d6ceda7b447b12f0b15`.
All emulator sessions in this check used headless video and exited.

## Large-enemy four-cell animation component

Added reproducible `gen_arcade_large_animation.py`, typed frame data and
`arcade_large_animation.c`. The native cursor indexes semantic frames rather
than original ROM pointers. Each frame sets four sprite codes, a countdown
and a next-frame index. `D52A` decrements an existing nonzero countdown without
changing the pose; otherwise it loads the next four codes and duration.
`D924` consumes a pending hit, selects the HP/facing script, immediately loads
its first frame, and replaces four attributes only for odd facing values.
Even facing values preserve existing attributes. A zero hit flag is a no-op.

Original CPU oracle `arcade_large_animation_trace.lua` covers 256 updates of
each of 19 script selections plus 384 damage-refresh cases (all HP0..7,
facing1..4, subtype0..2, flag0/1, timer0/255). Host native comparison passes
5,248 cases / 62,976 output bytes. The 104 distinct frames include entry,
offscreen exit, initial death and both facing/HP walk script families.
The later score-display scripts selected through `D8A0` are NOT included:
their terminal duration/cursor behavior needs its own death-lifecycle contract.
The complete actor is not yet implemented or wired to gameplay.

Trace SHA256:
`fb76def658400dc30dfa5dbc65c2e86aa68620b935508a0c542312f2acafa694`.
Native SHA256:
`750e4c0435fb734035311fe9777ac59d95e3c185de6a7bd845bcaf055af75d5d`.
`verify_large_animation_target.py` also passes all 5,248 cases using the actual
68000 component loaded in RAM (1,336 bytes including test wrapper and data).
Code SHA256:
`a51d243e7606cd79608fb0b2289cc21bdbac9f698e2c94fd5409d3505ab4a690`.
This is component evidence, not linked gameplay or VDP/render verification.
All emulator sessions were headless and exited.

## Large-enemy aligned firing component

`arcade_large_fire.c` implements the original `D74A` firing decision as a
native birth event. It decrements a nonzero countdown and does nothing else
that update. At zero, it checks P1 then P2: any NONZERO active flag qualifies,
regardless of kind or the tracking flag. The wrapped byte
`enemy_y+16-player_y` must be <=8. This deliberately differs from AD53's
bit-based target eligibility. It chooses the first inactive slot among four
shared children; a full pool or no aligned player leaves countdown zero.

Successful birth reloads the period without decrementing it, including a
zero period; child Y is wrapped enemyY+12. A player strictly right of enemy
selects facing1 and X+6, otherwise facing3 and X+20, with word wrapping.
The birth adapter must write child active1, major/minor0/0, kind2, X, Y and
facing only. Fractions and all other retained child bytes remain untouched.
The helper returns the event; ownership of that shared child pool and child
AI remains to be integrated with the complete large actor.

Original `arcade_large_fire_trace.lua` uses poisoned parent/child records,
all16 occupancy masks, nonzero flags1/2/255, timers0/1/255, period wrapping,
alignment boundaries and X/Y wrapping. `compare_arcade_large_fire.py` matches
all8,192 original calls and all1,441,792 parent/child record bytes through an
explicit partial-write test adapter. Birth coverage: slot0=1361,slot1=691,
slot2=397,slot3=202; no birth=5541. Trace SHA256:
`54dd9d720c6622eddbebe683085543ed941052c39b7362adeeb8f51e7efc9cae`.
Native SHA256:
`b8dfade0a280cebb84ad90b9c04e0b8f73521c3262b2c0b91c25140f184351d4`.
`verify_large_fire_target.py` runs the source-derived events through the actual
68000 helper with a RAM-loaded test wrapper. This remains component evidence,
not production scheduler/projectile integration or whole-game parity.

68000 result confirmed:8,192 cases pass,448-byte component including wrapper;
code SHA256 `cb9cf68515a216b65c3487fa7a04bac3ba59dd3143f666ed9d109c8378058b36`.
All emulator sessions used headless video and exited.

## Large-enemy complete death and offscreen-exit component

Added typed `ArcadeLargeBody` and `arcade_large_death_step` for all six `D7B4`
phases. Kill entry loads four EA attributes and the death script, sets the
shared HP/countdown byte to32 and advances phase. After32 subsequent updates,
it selects owner-specific E4/E3 score attributes, the variant/subtype score
pose and a12-update countdown. Expiry enters cleanup; the following update
clears active,major,minor. Exit instead selects subtype FA/FB/FC attributes,
the exit script and16-update countdown before the same cleanup.

The generator now includes eight distinct score poses selected by the12-entry
`D8A0` table, bringing the frame table to112. These poses have duration127;
the original following pointer is not a live animation loop. Native uses a
self-referencing terminal frame because the object is removed before that
countdown can expire. This normalization is limited to that proven lifetime.

`arcade_large_death_trace.lua` executes original `D55B` for48 complete64-update
trajectories: three subtypes,four score variants,two credited players,and kill
versus exit. `$9D=0` isolates the death handler from outer camera compensation.
`compare_arcade_large_death.py` advances the native instance continuously,
not reinjecting each state. All3,072 updates /58,368 semantic bytes match;
other original record bytes are verified unchanged. Removal occurs on indexed
tick45 for kill and17 for exit (entry is tick0). Inactive objects stay inactive.
Trace SHA256:
`8fbd325087d1c6910a2d4183006dabf462b1934f0e40d2ece3aa7f1fd9b73448`.
Native SHA256:
`835a10d6c8447b8f33f49a2d87230a7b65e2c67a3da85f8ec458358e42132108`.
The previous5,248 animation cases also still pass against the extended table.
Whole actor/controller integration and score awarding remain separate.

RAM-loaded68000 verification also passes all3,072 source-derived updates;
component+wrapper+data size1,780 bytes, code SHA256
`7b3371efbc7e44cd018f23f8321d3caaaac3666bdaf1aa6dc7b9623e794c5978`.
Emulator sessions used headless video and exited.

## Combined native large actor dispatcher

Added `arcade_large_actor.c` and reproducible `gen_arcade_large_motion.py`.
`ArcadeLargeActor` combines typed body/animation with fixed-point position,
direction/facing, hit flag, pursuit lifetime, firing timers, selected player
and entry/retreat vector selection. The ordinary `D55B` dispatch now calls the
verified targeting, steering, four-cell animation, firing and death components
in source order. Entry, pursuit, retirement, kill and offscreen exit are all
implemented; shared/global bonus states remain outside this ordinary dispatch.
The function returns a child-birth event for the caller-owned shared pool.
It does not allocate SGDK sprites or own a separate random stream.

Source ordering retained includes: immediate movement on pursuit selection;
48-update direction timer; animation before retreat targeting/movement;
firing before pursuit lifetime/hit refresh but after retreat hit refresh;
edge check overriding a same-update retirement transition; and outer `$9D`
compensation even on the update that clears active. Difficulty/subtype entry
thresholds retain the source's byte indexing (including odd offsets).

`compare_arcade_large_actor.py` advances one native actor continuously for each
of48 original trajectories, rather than reinjecting intermediate results.
All36,864 updates /1,511,424 canonical bytes match: positions/fractions,
major/minor state, all four codes/attributes, animation cursor/timer, target,
HP/timers, vector selection,87 child births and retained RNG state/sample.
Scripted camera and player coordinates are inputs; shared child slots are
cleared every update in this oracle. Low-HP collision injection is not part of
these trajectories; the independent8,192 steering checks cover that component.
Native actor size is34 bytes on the host; target arithmetic/ABI verification
uses `verify_large_actor_target.py` with a RAM-loaded compiled component.

This is a combined actor implementation, not yet gameplay integration. The
existing renderer/scheduler still use the legacy large-enemy path. Required
next work includes three-slot source controller, shared child pool/AI binding,
collision/reward transactions, composite renderer and integrated performance.

### Controller integration notes recovered while verifying actor

`FAFA` controller at `$06C8` does not gate on all three slots being free. Its
state0 writes remaining=3 and pointer=$0E48, increments state and returns.
Each subsequent state1 call visits exactly one slot (stride48), even if it is
occupied, advances the pointer and decrements remaining. After the third visit
it clears active/state/phase. A free visited actor receives active1,
X=camera+232, Y from `FB27[remaining]` (176,160,96), variant=parameter,
subtype=3-remaining and state/phase0, then calls `D58B` initialization directly.
Unwritten fractions and other old fields survive. Initialization here does not
run the outer `D55B` compensation. These are static integration requirements;
controller CPU fixtures and native translation are still required.

Combined actor target result:36,864 cases pass on68000,4,376-byte component
including wrapper and tables. Code SHA256
`be4af49f7a0055f54042e8b3cac97db339504e2feaf565c4574cf212e0364310`.
The initial90-second harness deadline expired before a result; its process
was terminated. A300-second retry completed with no differences. No gameplay
ROM change is claimed. All test emulator sessions were headless and exited.

## Native three-slot large-enemy controller

Added `arcade_large_wave_step` for `FAFA..FB62`. Native controller auxiliary
stores the three-slot cursor; wait stores remaining visits; parameter carries
source variant. State0 sets remaining3/cursor0 and increments state while
preserving phase. Each state1 call visits one slot, advancing and decrementing
remaining even when occupied. The final visit clears active/state/phase.
Free actors receive source birth fields and direct `D58B` initialization.
`arcade_large_initialize` is now shared by entry dispatch and controller so
controller births do not accidentally receive `D55B`'s outer drift.

`arcade_large_wave_trace.lua` tests512 complete six-update scenarios, covering
enabled/disabled controllers, all eight occupancy masks with active1/2/255,
four variants, four difficulty selections and cameras0/65530. Actor records
are poisoned to verify retained fractions/fields; source `$9D=1` specifically
checks that a controller birth does not apply compensation. The native host
comparison matches all3,072 updates continuously, including384 births and
occupied-slot skips. All nonowned source record bytes remain unchanged.
The initializer extraction also reruns and passes the36,864-update complete
actor host regression. Target verification is `verify_large_wave_target.py`.
Gameplay still uses the old fallback until shared projectiles, collisions,
resource residency and composite rendering are connected and checked.

68000 controller check passes all3,072 cases; compiled component+wrapper+tables
4,542 bytes. Source trace SHA256
`71ed103df5f33316f4d81cb2bf8263c953250cc0ed690cac6d0736d530fd7034`;
code SHA256 `8c2b4e85547efd72c6d386eb2d7f462ccfe8faa29a75049e21f88aac81aa1269`.
Emulator sessions ran headlessly and exited.

## Large-enemy shared-pool projectile (kind2)

Added `arcade_k.c` / `ArcadeKProjectile` for kind2 in `CBC8`'s four shared child
slots. This is the projectile emitted by `D74A`, not the unrelated `DD78`
enemy death handler. Kind0/1 in the same pool remain separate pending work.
The typed record preserves X/Y fractions, animation, active/state/phase,
facing, death countdown and the source origin word. Origin's low byte doubles
as the startup timer; native writes preserve its high byte until flight stores
current X there, matching the original alias lifetime.

`CE51` initializes code89/E4 animation and six-update wait without movement.
`CE64` counts down without animating. `CE73` records origin on its first call,
moves +/-608/256 for facing1/3, animates, checks low-byte relative X>=248,
then adds141/256 only when `$9D=0`. Offscreen cleanup starts at3/2. Impact
starts3/0, selects codes8D..91 and a16-update countdown before cleanup.
`CBEB`'s global purge runs after each visited handler, even if that handler
just cleared active, and can leave inactive state3/2. Inactive records are
then skipped on subsequent updates. Source collision `EE44` sets active2
and state3/0 for a shot hit; the trajectory fixture injects state3/0 only and
does not claim natural collision reachability. The native dispatcher accepts
any nonzero active flag, as CBC8 does.

`arcade_k_projectile_trace.lua` covers144 continuous160-update trajectories:
left/right, fractions0/255, three screen offsets, camera0/65530, stopped/running
scroll, ordinary/impact/purge scenarios. `compare_arcade_k_projectile.py`
passes23,040 updates /368,640 semantic bytes and144 removals. Unowned record
bytes are also checked. Trace SHA256:
`8c1087e288f477852fec28460650d64cc95ad9fa2d871fba0c7d8fc9e4d5962c`.
Native SHA256:
`c637b94d52a9ddda9903298883bd920d369efbc9912accfae98d5e8315917a3a`.
Target check is `verify_k_projectile_target.py`; shared pool integration,
actual contact/shot collisions, score events and renderer remain required.

Target confirmation:all23,040 cases pass on68000. Component+wrapper848 bytes,
code SHA256 `f084e235d4adee73c3cfaf724431e7718ca3000e127ba3047e4f78b1c201e1d1`.
All emulator sessions ran headlessly and exited.

## Shared projectile collision scans and cancellation

Added `arcade_k_collision.c`: `EE10` shot scan selects the first of four slots
with active exactly1, nonnegative major state, wrapped byte Y within +/-7 and
word X within +/-9. `ED9F/EDB1` contact scan requires player active1 and child
active nonzero with bit1 clear, no major-state gate, and +/-6 bounds on both
axes. The contact selector can return bit6 bonus objects; callers must branch
to bonus handling rather than apply ordinary damage. The bonus path is not
covered by the present ordinary-projectile fixtures.

`arcade_k_cancel` implements `EE44`: both shot/child active2, child state3/0,
shot state2/0, score event0430 or04B0 according to credited player. Other state
remains untouched. Source ordinary player contact instead writes child3/2,
sets player kind1 and clears its major/minor state. That latter transaction is
recorded by a host adapter for future player-life integration; the native
helper currently only selects the contacted child.

`arcade_k_collision_trace.lua` executes full original scans, including flag
and coordinate boundaries, wrapping and overlapping slots. Host comparisons
pass8,192 cases:758 cancellations,523 player contacts;1,310,720 parent/child
record bytes checked including retained fields. Trace SHA256:
`67ae26b2bbcaebcaf6e3eaafb67911af1c1922d95965f87e4760421d5c0c1390`.
Native SHA256:
`de08da5f9edd0d2160217654c02ee00649a0f3bd0d67a80c11a6b4af92c6f365`.
`verify_k_collision_target.py` checks the actual selectors and cancellation
routine on68000; it does not present the contact test adapter as gameplay code.
Global collision-page order, bonus pickup handling and actual score/life
binding remain required before gameplay integration is verified.

Target confirmation:8,192 cases pass;654-byte component and wrapper.
Code SHA256 `482e70e95fd07b067705dfe0c12fd15382fe34adbaa1e74a72cf07aa788463ae`.
All test emulators ran headlessly and exited.

## Large-enemy accepted-hit rewards and geometry

Added `arcade_large_collision.c`. Its shot-overlap helper retains the original
asymmetry: wrapped enemyY-shotY+28 <40, but word enemyX-shotX+28 <=40.
The original4,608 hit/miss leaf cases match this helper on the host.
Eligibility and three-slot scan ordering remain caller-owned and need their
own integration checks.

`arcade_large_hit` consumes the player shot (active2,state2/0), sets pending
hit and decrements byte HP, including0->255 wrap. Every accepted hit requests
sound7. Only1->0 changes enemy to active2,state2/0, assigns the credited owner,
queues the variant/subtype score event and applies E407 difficulty+0x80.
The helper uses the existing verified difficulty addition rule, including
16-bit wrap and rejecting a resulting word>=0x3F00.

`arcade_large_reward_trace.lua` runs original EE8E followed by the actual4CBE
BCD-score consumer for kills. All3 subtypes ×4 variants ×2 owners ×HP0/1/2/7
×8 difficulty boundaries give768 cases. Host comparisons match HP, flags,
phases, owner, sound, event, points and difficulty. Reward rows by variant:
3000/4000/6000;4000/5000/8000;5000/6000/9000;6000/7000/10000.
The legacy gameplay's flat800 is still pending replacement when source
subtype/variant ownership is bound; it cannot be corrected faithfully by
changing a single constant while those birth fields are missing.
Trace SHA256:
`05177a3836695fc8c72fb9e6342d90518ffe832cf8dca66e1a0a6828c3234f5a`.
`verify_large_rewards_target.py` tests the accepted-hit routine and difficulty
addition on68000, not the separate overlap helper or collision-page ordering.

68000 hit/reward check passes all768 cases;584-byte component+wrapper.
Code SHA256 `fd039700983e7b93a711c102ae45a18db811d12905a48dcd2ac077d04423e11c`.
All emulator sessions ran headlessly and exited.

## Large-enemy three-slot collision scans

`arcade_large_shot_target` and `arcade_large_contact_target` reproduce EE71
and EF09 slot order. Both require parent active == 1 and enemy active == 1;
neither filters on the enemy major state. Shot geometry uses the asymmetric
EE8E bounds described above. Contact uses byte Y and word X differences,
adding 24 and accepting values <=32 on both axes.

`arcade_large_scan_trace.lua` executes the original scanners for 8,192 cases,
covering overlapping slots, active flags, major states, exact boundaries and
coordinate wrap. Host comparison selects the same slot in every case (960
shot hits,169 contacts), and checks 1,441,792 raw record bytes after composing
nonlethal hit effects. Contact effects in this test are an adapter, not the
native player-life integration.

`verify_large_scans_target.py` passes all 8,192 selectors on the 68000.
The RAM-loaded component and wrapper total 698 bytes, SHA256
`4d4ff4ea29a61b028c9321cce7add05bf5dc732d0fe4e4d0b298cea2193aee40`.
Source trace SHA256
`3fef931f266193f6c5c81041e94ae3f3e2ce2390a522755b0e22ad2f2ec253ef`.
The headless test session exited. Gameplay binding, global collision order,
four-cell rendering and full-game performance remain pending.

## Large-enemy four-cell emission

`arcade_large_cells` reconstructs original FF6A output as raw hardware cells,
retaining byte coordinate wrap, original code/attributes and column order:
left lower, left upper, right lower, right upper in arcade screen coordinates.
X is low(actorX-camera), with +16 for the right column. Lower Y is
byte(-actorY-16), upper Y subtracts another16. Any nonzero active flag is
accepted, including bit7. Admission requires at least4 remaining sprite slots;
it emits all4 or leaves both output and remaining budget untouched.

`arcade_large_render_trace.lua` runs the original FF6A code for4096 cases:
flags0/1/2/127/128/255, budgets0/1/3/4/5/24/255, all byte Y values,
wrapped camera-relative X and varying independent code/attribute bytes.
Host and RAM-loaded68000 output match all4096 cases (7772 emitted cells).
The component plus test wrapper is288 bytes, code SHA256
`fa093fa22385317d63a13bff0a6e698d5c0582ac099d9936864e69a56a89c55b`.
Source trace SHA256
`f5555551df63964821ceefcc522e647e299112c8a09d9efc723299bc7f3d4eac`.
This establishes source cell assembly, not Genesis tile/palette conversion,
pixel fidelity, SAT publication, whole-scene sprite priority or gameplay
integration. The test sessions were headless and exited.

## Large-enemy and kind2 projectile art

`gen_arcade_large_art.py` extracts reachable code/bank pairs from entry,
exit, both walking tables, death and score scripts, plus kind2 projectile
normal/impact poses.216 pairs deduplicate to187 exact16x16 cells (23936 bytes).
Generated lookup returns local frame IDs, not resident VRAM addresses, and
preserves flip independence (attribute bits6/7 do not change the frame ID).
Unsupported code/bank pairs return65535.

`verify_large_art.py` independently decodes raw graphics-plane bits and PROM
indices:55296 pixels match generated data; all65536 code/attribute lookups
match the extracted mapping;102620 cells from source actor/death/projectile
trajectories have valid art. The independent decoder initially assumed the
wrong bit direction; MAME's explicit xoffset7..0 and MSB-first bit convention
confirm physical bit0 is the left pixel within each half. See
https://github.com/mamedev/mame/blob/master/src/mame/capcom/sonson.cpp
(spritelayout). The existing bake's half-mirror is therefore retained.

All187 cells need748 tiles, exceeding the current free resident area. Three
large actors plus four projectile slots need at most16 simultaneous cells,
or64 tiles. A bounded upload arena can hold those cells, but its exact VRAM
ownership, admission, DMA costs, wrapped placements, SAT ordering and display
output still need implementation and validation. Generated resources are
currently unreferenced by gameplay and stripped from the cartridge. These
checks prove palette indices, not RGB palette accuracy or VDP output.

## Large/K bounded upload arena

`arcade_large_upload.c` reserves tiles[1308,1372), after level dictionary
end1305. A runtime guard refuses upload if the active SGDK font allocation
falls below1372. Each of16 cells owns four tiles: large actors occupy0..11,
shared projectile cells12..15. No eviction, allocation or decompression is
needed. At most2048 bytes change per presentation; unchanged graphics queue
nothing. Pose IDs are local art frame IDs, with65535 for an unused cell.

Admission checks the entire requested set before enqueue: valid frames,
descriptors (including possible128KiB splits), transfer capacity and caller
reservations for remaining SAT/map work. Source data are immutable ROM arrays.
The ordinary SDK DMA queue helper retains bank splitting and Z80 protection.
The cache records accepted uploads, so callers must flush the previous queue
before the next presentation, never clear an accepted queue, reset on video
initialization, and publish new SAT references only after successful admission.
The normal main-thread render path must be the only concurrent queue producer.

`verify_large_upload.py` passes ASan/UBSan host queue tests covering insufficient
byte/descriptor capacity, invalid frame IDs, font overlap, full initial load,
unchanged reuse, single-cell changes, exact reserved-byte threshold, rejected
pose retry, hide/reappear reuse and reset. Deferred copies land within the
64-tile arena. This uses a mock queue, not VDP hardware; actual bank splits,
VRAM output, sound-under-load and performance still need target checks.
The uploader is not yet invoked by gameplay and remains linker-stripped.

### Target upload verification

`verify_large_upload_target.py` builds a separate SGDK test cartridge using the
production uploader and art. Test-only linking places the pixel array at
0x1FFC0 so cell0 straddles a128KiB ROM DMA boundary. The normal SDK queue
splits the first16-cell upload into17 descriptors, as expected. Production
out/rom.bin is hash-checked unchanged by the test.

Twelve complete16-cell batches cover all187 frames. After each normal
SYS_doVBlankProcess flush,68000 code reads actual VDP VRAM and compares1024
words against ROM. All12288 words match. Sentinel words immediately outside
the64-tile arena remain intact. A2047-byte capacity rejects the2048-byte
initial pose without queueing; its retry succeeds. Each repeated pose queues
no transfers. The target test completed with error0, then MAME exited.
Test cartridge SHA256
`72a7b03d7262d6a4eea9c614c1645acb7012ecc59242759d18e9a26be3bea263`.
This verifies actual SGDK DMA/VDP behavior in MAME, including the split; it
is not a real-hardware check, gameplay integration or audio/performance test.

## Source cell to Genesis SAT conversion

`arcade_cell_sat` translates one raw source hardware cell into clipped Genesis
2x2 sprite entries. It preserves source copies at(x,y),(x-256,y),(x,y-256),
without inventing a diagonal copy. Attribute bits6/7 are active-low H/V flips;
tile/palette references use a ready PAL1 four-tile block. Visible copies retain
source ordering, link to the next slot, and leave final-chain termination to
the renderer. Insufficient capacity returns65535 without modifying the SAT.

The viewport is explicit. This helper does not silently choose how the arcade's
240x240 visible rectangle should map to the current256x224 Genesis mode.
`verify_arcade_cell_sat.py` checks2940 host cases over original and cropped
viewports, including331590 visible synthetic pixel identities, links, clipping,
flip direction, sentinel preservation and rejected writes. These are host
conversion tests; full renderer ownership/priority, target SAT output and
visual viewport policy remain pending. No gameplay call is bound yet.

## Composed large/K presentation

`arcade_large_present` connects the source cells, generated art lookup,
fixed64-tile uploader and SAT conversion. Stable cell ownership maps three
actors to cells0..11 and shared projectiles to12..15. It prepares the complete
SAT contribution in a private512-byte stack buffer, resolves only visible
art, and publishes caller SAT words only after every upload is admitted.
An invalid art reference or insufficient SAT/DMA capacity leaves the caller
SAT unchanged. It preserves inactive cached art for later reuse. Source24-cell
admission and final chain termination remain the caller's responsibility.

The standalone target cartridge now also exercises this composition: a full
16-cell presentation, exact SAT positions/tiles/links, actual64-word SAT VRAM
readback, capacity rejection without writes, unchanged-pose reuse, horizontal
and vertical wrapping, active-low horizontal flip and invalid-art rejection.
All target checks pass, together with the existing12288 graphics-word checks.
Updated test cartridge SHA256:
`9bfeebdba0ddafc958ee42283d0bc91c114e4b4b021c92835981f6f946b9ff72`.
MAME ran headlessly and exited. This is still an isolated cartridge: normal
gameplay does not yet invoke these components. Full scene priority, viewport
policy, queue reservations under gameplay load and sound/performance remain
unverified.

## Live large/K integration and current release gap (2026-10-22)

The normal scheduler now creates the three dedicated large actors; the live
update and render paths invoke their recovered behavior and kind-2 projectiles.
Earlier sections describing these components as unbound are historical.
The current 655360-byte ROM uses a flat MEGA DRIVE header, correcting an
inherited SSF mapper declaration that prevented Genesis Plus GX startup.
Current ROM SHA256:
`a1431a77ace00007f393d19700f1e3bdc53c3882dd85f279421ba8cf2e041500`.

On this exact ROM, `verify_large_binding.py` passes 8 injected scenarios and
312 checks: scheduler births, seven successive hits, death and score ownership
for both players and all four variants. This does not prove natural encounters,
whole-game update ordering, rendering or arcade parity. MAME ran with video
 disabled and exited successfully.

The current Genesis Plus GX report has 2000 sampled refreshes per mode:
1P misses 4 presentations (worst 60-refresh window: 56), and 2P misses 2
(worst window: 58). Neither workload completes a ribbon wrap. Strict 60 FPS
therefore fails, and these short runs are not full-course performance evidence.
Entity-count metrics currently omit the separate large/projectile arrays.

Remaining release blockers include other approximated enemy families, shared
projectile/bonus pool ownership, whole-game collision order and timing,
viewport and sprite priority, sustained 1P/2P performance, matched audiovisual
validation, and final screenshots/source export. The new art includes 188
frames; the previous standalone upload report predates that additional cold
projectile pose and must not be presented as current-build validation.

## Projectile contact before H/G/I bodies (2026-10-22)

Original E2F8 calls ED9F before the H/G/I contact calls at E2FE/E301/E304.
The live port previously checked H/G/I first, allowing their death request to
skip destruction of an overlapping kind-2 K projectile. K contact now runs
before those bodies. Other generic contact ordering remains unresolved.

`verify_k_contact_order.py` injects simultaneous I/K contacts through the linked
`entity_update_all`, alternating player ownership, contact offsets, I flags and
invulnerability. The previous a1431a77 ROM fails the first overlap case; its
report is preserved in `out/parity-audit/k-contact-order/before.json`.
The corrected ROM passes all 300 updates / 1500 checks. Ordinary I contact
regression passes 300 updates / 900 checks and large scheduler/damage binding
passes 8 scenarios / 312 checks. All emulator runs were headless and exited.
Current ROM SHA256:
`25562fd9b2b198316ce35acce1a49bd0597ea96af38741863c6432394628710f`.
These are injected integration checks, not a natural playthrough or a complete
collision-sequence comparison. Previous performance results belong to the
previous ROM; sustained 60 FPS remains unproven.

## Current large/K graphics target revalidation (2026-10-22)

Re-ran `verify_large_upload_target.py` with the current 188-frame art, including
the cold projectile pose. All 12 batches and 12288 video-memory word comparisons
pass; the bank-crossing fixture uses 17 DMA descriptors as expected. Composed
SAT, admission rejection, reuse and wrapping checks also pass. Standalone test
ROM SHA256: `e0e60964c90d34f09a58c2f8c6f9b46032ccfae309deba3433e9c040c63168d0`.
Production ROM remains `25562fd9b2b198316ce35acce1a49bd0597ea96af38741863c6432394628710f`.
This supersedes the older 187-frame upload coverage only. Full gameplay sprite
priority, audio stress, natural encounters and performance remain unverified.
MAME ran with video disabled and exited.

Shot-order investigation: E2BF..E2CE scans pools based at0828,0A28,0AE8,
0B68,0BA8,0768 before I/H/G atE2D1..E2D7. The port currently invokes its
I/H/G shot helper before the generic pool, so relative priority is unresolved.
Before reordering the entire generic pool, establish each native archetype's
source-pool mapping: legacy descriptive comments mix enemy identities and
cannot serve as an independent contract. No speculative ordering change made.

## Scheduled generic shot-family priority (2026-10-22)

Verified the source dispatcher pool mappings: B9BA->0828 (enemyMain),
BBDE->0A28 (C), BF30->0AE8 (D), C2D5->0B68 (E), DE95->0BA8 (F).
These correspond to native THROWER, CREATURE_C, EMITTER, COMPOSITE_E, FLYER.
E2BF..E2CB scans them in that order before I/H/G atE2D1..E2D7.
The live collision pass now preserves that family-major ordering and stable
slot order within each family. Survival retains its old single scan. Legacy
non-scheduled archetypes retain a fallback after I/H/G; the source0768 family
and other provisional behavior still require complete ownership recovery.

A reverse pool walk builds six short linked lists once, avoiding repeated full
pool scans. The initial repeated-scan implementation was functionally correct
but missed153/123 presentations in2000 refreshes (1P/2P); its failed report is
preserved as generic-shot-order/repeated-scan-performance.json.
Current list-based ROM SHA256:
`3f7ac1d643b1ac7be97f0adbdbee40a72c5a76a2108311462053631522931f92`.
Current short performance runs miss18/3 presentations (worst60:50/58).
This remains a strict60 failure, and workload changes preclude attributing all
variation to CPU cost alone. No full ribbon traversal was covered.

New `verify_generic_shot_order.py` fails on prior25562fd9 ROM when an emitter
and I/H/G overlap a shot, then passes on the corrected build:68 updates,
736 checks, including four owner/reservation overlap cases. Existing I/H/G
priority integration passes68/740, large birth/damage integration passes8/312.
These linked injected checks do not establish full natural gameplay parity or
correct provisional hitboxes/rewards. All emulator runs headless and terminal.

## Skip empty ordinary shot scans (2026-10-22)

When both slots on the selected player page have active!=1, all ordinary shot
handlers have no effects. The live pass now jumps directly to player contact,
avoiding family-list construction and the shot scans. Survival is unchanged.
The fixed-input comparison now also captures the dedicated large/K arrays:
4000 completed 2P updates,2998 state bytes/update, zero before/after mismatches.
K/I overlapping contact regression passes300 updates/1500 checks with no shots,
confirming that contact still executes on this path.

Fixed-work MAME collision elapsed time changes from mean2.677776ms to2.314432ms
(13.57% reduction). Peak changes4.639100ms to4.657091ms (+0.018ms); the additional
shot-presence test costs active-shot frames. Retained intentionally for the
measured average saving with identical state, not as a worst-case improvement.
Actor/scheduler timings are unchanged. Reports: idle-shot-profile-before/after.
The profiler accepts separate build/output directories for bound comparisons.

Current ROM SHA256:
`2ff9cd8719e8fa4f360ff0ea5af3bfaf6598031b765bfb9edfe459d9f5bf1d22`.
Genesis Plus GX2000-refresh runs still miss18/3 presentations (1P/2P), unchanged
from before. Strict60 remains failed. No full-course performance or hardware
claim follows from these samples. All test emulator processes exited.

## Empty large/K presentation admission (2026-10-22)

The headless runner now records the optional Genesis Plus GX 68000 PC export
at refresh boundaries. On2ff9cd87, all21 missing-presentation samples were in
rendering/VDP/DMA preparation, including the large/K presentation/upload path.
This is a point sample, not exclusive attribution of whole-frame cost.

When the source cell mask is zero, arcade_large_present now returns zero after
SAT bounds validation, without constructing frames or checking DMA reservations.
Previously, queue pressure could return failure even for zero sprites, causing
the caller to reuse a previously accepted SAT contribution. Zero sprites must
not depend on upload capacity. Resident art remains untouched.

The new target regression fails before the change(error53), then passes with
all existing12288 VRAM-word checks, SAT/reuse/wrap/admission tests. Empty-mask
capacity exemption and invalid SAT bounds are both checked. The before failure
is saved in before-empty-present/target-failure.json. Target test ROM:
`d848eed565871102265c755c760fc0321f587a7c723ff2ef3b595cfd57793adb`.
Production ROM:
`2690edaf063b971569558945161588443c66cebb9a818752d55c42efa6e1d39a`.
Short Genesis Plus GX performance is recorded in out/genplus-results.json;
strict60 and full-course parity remain unproven. All test sessions exited.

## Long-run coverage and constant-time art lookup (2026-10-22)

Extended Genesis Plus GX sampling to42000 refreshes per mode. The2690edaf
baseline missed1246/1931 presentations (1P/2P), with worst60-refresh windows
of30/30 and CPU peaks128.22%/149.66%. It traversed1/0 full ribbon wraps.
New counters include the dedicated large/K pools: all3 large actors were
observed, with up to3/4 projectiles. Long-run-stalls.json groups observations:
951 of1246 1P misses and1305 of1931 2P misses had a large actor active.
These correlations and refresh-boundary PC samples identify rendering as a
priority; they do not isolate the complete cost of each function.

The generated art lookup now uses an attribute-page table and256-entry byte
pages instead of binary search for each emitted cell. Unsupported pages/codes
use255 internally, expanded to the existing65535 result; the generator asserts
fewer than255 frames. Flip bits remain ignored exactly as before. Lookup data
is ROM-only, with no added persistent RAM or changed pixel resources.

All65536 code/attribute cases,55552 raw-ROM palette-index pixels and102620
source trajectory cells pass host checks. The standalone target test passes
12288 VRAM-word comparisons plus SAT/admission/reuse/wrap regressions.
Target test ROM9ede6ed9d41b8a56dd0cb9b536c21faecdde6957d9d9eb6644f3c5de383e2929.
Production ROMcf201fee8d05771f783a509c855adf0b4bfa1fb7618bcebd2a2ff08bbe12491f
remains655360bytes. Before ROM/symbols/time series are saved in before-art-lookup.
New long-run report is out/genplus-results.json. Refresh-timed input means paths
can diverge after different misses, so this is end-to-end observed performance,
not a fixed-work CPU speedup claim. Full arcade parity/strict60 remain pending.

The42000-refresh candidate results are673 misses1P and2081 misses2P,
worst60 windows36/30, peaks116.26%/130.66%. Compared with baseline this is a
mixed end-to-end result; do not report a universal FPS improvement.
4000 fixed updates match2998 state bytes exactly, but contain no large actors;
section timing is identical, so that short profile does not measure this
lookup optimization. Retained for exact constant-time lookup and lower observed
CPU peaks, with the 2P missed-frame regression explicitly unresolved. The next
fixed-work performance comparison must include large-actor encounters.

## Matched large-encounter lookup measurement (2026-10-22)

Expanded the before/after fixed-input comparison to16000 completed updates.
All2898 captured bytes per update match, including492 updates with large actors.
Earlier reports said2998 bytes; that was a reporting arithmetic error, now
replaced with a count derived from the actual captured fields. Historical2732
byte captures before adding large/K were similarly mislabeled2832.

For the492 matched large-actor updates, entity rendering mean changes
8.131396ms to6.433243ms
(20.88% reduction), peak
11.903860ms to9.931226ms.
Across the full16000, entity rendering mean1.380191ms ->1.328388ms,
peak11.903860ms ->9.931226ms. ROM read-tap timings include any interrupts;
small shifts in other sections are not claimed as simulation optimizations.
Reports: art-lookup-profile-before/after and art-lookup-matched-summary.json.
This supports retaining the exact direct lookup despite divergent long-run
controller trajectories. Strict60 still fails; other enemy and global fidelity
gaps remain. Production ROM unchanged(cf201fee). Test processes exited.

## Rejected cached clipping experiment (2026-10-22)

Cached three wrap-copy visibility flags to avoid repeated coordinate tests.
Host2940 cases/331590 pixel identities and target VRAM/SAT checks passed;
16000 fixed updates matched2898 captured bytes exactly. However matched
entity rendering mean only changed1.328388ms ->1.326802ms (about0.12%),
peak9.931226ms ->9.926532ms. Refresh-timed42000 runs worsened to815/2907
misses (1P/2P), against673/2081 before, with divergent gameplay routes.
The negligible fixed-work benefit does not justify retaining this mixed result.
Reverted only this clipping experiment; the proven direct art lookup remains.
Candidate967d4e34 ROM/source/report preserved in sat-visibility-experiment.
Current production is restored tocf201fee. These measurements narrow the next
optimization choice; strict60 and full arcade parity remain unfinished.

## Interior sprite-cell fast path (2026-10-22)

Nested timing on492 matched encounter updates identified arcade_cell_sat at
mean2.185813ms of total entity rendering6.431157ms. Added an interior fast path
when viewport-relative X/Y are both0..240: neither negative256 wrap copy can
intersect, so only the original copy needs clipping/admission and SAT output.
The original general path remains for all other positions. No art/frame drops,
priority changes, enemy reductions or simulation changes are introduced.

Host2940 geometry cases/331590 pixel identities and target12288 VRAM-word plus
SAT/admission/wrap checks pass. All16000 matched updates agree on2898 captured
bytes. New nested timings over the same492 encounters: cell conversion mean
1.237385ms (43.39% less), total entity rendering5.396145ms (16.09% less), peak
8.661679ms versus9.929140ms. Art lookup timing/call counts remain identical.
Inclusive durations include interrupts and must not be summed.

Current ROM63ebf050c2c6f08718878ca33182ccff1c7b2a1c6e2a8f75b5c75083c042eb3d,
655360bytes. Target-test ROMd08b18905d42ca1d303eeb165286854edc3a9fa8d3798e624bd24a5ff608b090.
42000-refresh runs:533/1155 misses1P/2P, worst60 windows40/30,
CPU peaks103.81%/131.20%, one ribbon wrap in both modes. These are improved
end-to-end runs, but strict60 still fails and gameplay routes diverge with
refresh timing. Baseline profile saved in sat-fastpath-profile-before;
current nested profile in large-render-profile. All test processes exited.

## Shared G/H bonus conversion source contract (2026-10-22)

Recovered FF dispatch entries G/H through the original CPU.1408 isolated calls
cover both families, phases0..3, active flags1/2/65/128, bonus indices0..10,
and camera-relative positions0/8/247/248. Full32-byte record and next global
bonus index match the explicit contract (45056 record bytes,1408 index checks).
Tools: arcade_foe_bonus_trace.lua, verify_foe_bonus_contract.py; trace SHA256
`e753dd640a1a1534e55d00a92317633698e79c87a4907df45743785bf2818bc4`.

Phase0 (B351): increment phase; active=(active&FD)|40; copy globalA2 into
kind; incrementA2, wrapping to0 when above7B; select B291[kind] animation,
clear animation timer and execute9617. Phase1 idles. Phase2 (B36A) increments
phase and executes B1FD (advance animation; force attributeC3 if hit_style is
nonzero). Phase3 idles. The family wrapper applies offscreen release to6/2
when low-byte(X-camera)>=248. Source A92E subsequently uses globalA1 for
purge; this fixture explicitly clearsA1. An initial probe accidentally leftA1
at boot state and observed phase reset; corrected before recording this trace.

Native G/H FF integration remains pending: shared bonus-index ownership,
collectible animation art/VRAM capacity, contact/reward consumption and purge
lifecycle must be connected together. No production ROM change in this step.
H state3 dispatch target87CD points into repeated data bytes in this revision;
it must not be invented as an attack state without reachability evidence.
MAME ran headlessly and exited. Current production remains63ebf050.

## Converted G/H pickup award transaction (2026-10-22)

Original contact ED18 changes actor active to2, increments its phase, and copies
player ownership into hit_style, then calls shared E838. Captured both G/H
families, both owners, all11 valid kinds, and combat values0/1CFF/3E80/FFFF:
176 original CPU cases include actual4CBE packed-BCD score consumption.
Rewards are1000..10000 for kinds0..9,20000 for kind10. Event selectors09..12,2A
are ORed with0400 and owner80. Sound command is5 (6415), and combat increases
by00C0 using the existing wrap/cap rule. This is distinct from provisional
ordinary fruit rewards elsewhere in the port.

Added native arcade_gh_bonus_collect in arcade_rewards.c. Host compiled checks
match all1232 output fields against the original trace, including actor phase,
owner, score event, sound, difficulty and points. Invalid kinds leave state and
output unchanged. This helper is not yet wired into the live contact scan:
conversion animation/art, shared bonus counter/purge lifetime and collection
must be integrated coherently. Target CPU validation of this helper remains
pending. The release goal is not complete.

Trace8ff399a39ba9d986b6251968f9de2989b8e247872cb534a7006a2f0de3fcc49b;
reports foe-bonus-collect.csv and foe-bonus-award/results.json. Build passes;
production ROM SHA256:63ebf050c2c6f08718878ca33182ccff1c7b2a1c6e2a8f75b5c75083c042eb3d.
Original MAME test ran with video disabled and exited.

## Resident converted-enemy artwork (2026-10-22)

Generated22 typed animation records from the11 B291 roots and their collected
score poses, preserving the source kind10 score-loop target(kind9 root).
All reachable normal fruit and both-owner score images fit as33 distinct16x16
cells in previously unused VRAM tiles[136,268). Scene tiles end135 and existing
animation tiles start290. No background/dynamic-large arena relocation required.
Generated resources: arcade_bonus_art.h/.c, gen_arcade_bonus_art.py. They are
loaded once by game_init; compile-time overlap guards cover adjacent owners.

verify_bonus_art.py independently checks8448 palette-index pixels against raw
source graphics/LUT. The cold-start verifier now reads actual VDP VRAM through
the ports when bonus resources are linked: all2112 words match their ROM data
at normal Home2P initialization, and the first gameplay update still passes.
This validates resource loading, not visible bonus lifecycle behavior or RGB
palette fidelity. Conversion/contact/animation ownership integration is pending.

Current ROM097af461f7455e8fe4e528aa897c3507e6ed78214a4bed0177ce9c9ecc6ecde6,
655360bytes. MAME ran headlessly and exited. No sustained60 claim on this build.

## Live G/H shared FF lifecycle and pickup binding (2026-10-22)

arcade_foe_bonus_step now implements the recovered conversion/idle/score/idle
phases, offscreen6/2 transition and post-handler purge wrapper. The live entity
bridge invokes it for G/H stateFF and uses tagged pose IDs for resident bonus
art without changing the20-byte foe layout. Phase0/2 match1408 original CPU
snapshots in21120 host field checks. Shared next-index/limit storage resets at
session startup; its natural powerup trigger/limit progression is still pending.

The H/G contact scan now recognizes active bit40, invokes the verified native
award transaction, consumes the collectible, credits its owner, applies combat
progress and emits sound5. It preserves one contact per family before proceeding
to the next family. I bonus behavior remains separate/unimplemented. Rendering
now admits G/H converted actors while retaining bit80 suppression; score poses
use the correct owner tile variant.

Injected live update+SAT tests pass44 scenarios/968 checks: both families,
both owners,11 kinds; conversion, shared index, nonlethal pickup, points,
difficulty, owner score tile and SAT reference, offscreen release and cleanup.
Current ROMb5ada581a46b5c341c374c6794678ecff504f3adbb237dbac877f8b4408073ed,
655360bytes. This is integrated state behavior, not proof a player can trigger
conversion naturally. Original powerup object/global purge lifecycle and other
families still need recovery/binding. No current-build sustained60 claim.
MAME tests ran headlessly and exited. Ordinary contact, K contact priority and
I/H/G shot integration regressions were also run on this build.

## Natural conversion pickup contract (2026-10-22)

Original CPU E7D6/E7DF special-object contact scans now have a dedicated fixture:
tools/arcade_conversion_trigger_trace.lua. All900 injected cases pass6300 field
assertions, covering both owners, object active0/1/2, initial bonus limits
0/1/2/3/4/255, and X/Y contact boundaries including coordinate wraparound.
Only active1 is collectible. Contact uses unsigned16(playerX-objectX+3)<=6
and unsigned8(playerY-objectY+7)<=14. For kindFF, a hit changes active to2,
state/phase to3/0, and copies player ownership; globalA1 becomes1, A2 becomes0,
and global7B increments with byte wrap then resets to0 if above3. Thus the
normal limit sequence is1,2,3,0; it does not naturally grow to10 through this
pickup. Trace SHA256:
f8568925852235e1e0462afcf247c069b1083a15ef8e8e08c4f04b96ec2772a4.

Important lifetime correction: A1 is a single enemy-update-pass pulse, not a
timed effect owned by the pickup. A5E2 calls special-object handling B19A before
enemy families, and A61F clears A1 after the family updates. B286 only clears
the retired special object's active/state/phase; it does not clear A1. This
ordering is statically established; an integrated pulse-order trace is pending.

Spawn chain recovered statically: every eighth ordinary special pickup calls
FB95, sets second special slot kindFF, and arms descriptor0558 with0100 and
placement byte68. B0D1 processes descriptors0550/0558 unless global9D blocks it.
Its six phases wait for globalA4/global8B as applicable and write background
cells; phase2 B12E activates the object at the descriptor-selected placement.
These gates and background writes need source traces before native binding.
Do not replace this with an immediate generic fruit spawn. Three special slots
are scanned in source order and can all be collected in one player scan.

This step changes research/tests only. ROM remainsb5ada581; natural special
object spawning and the live conversion pulse are not implemented yet. MAME
ran with video disabled, window flags set, and exited successfully.

## Native special-pickup spawn sequencer (2026-10-22)

Added arcade_special_spawn.h/.c: four-byte native descriptor and explicit
background/spawn events for the six B0D1 phases. This does not retain a source
CPU interpreter or source display buffer. Source CPU fixture
arcade_special_spawn_trace.lua executes both descriptor slots, all six phases,
eight combinations of the9D/A4/8B gates, active0/1, and placements00/68/F8/FF.
verify_special_spawn.py compares descriptor state, object state and65 strip
bytes (including the row31 word crossing the nominal strip boundary):
768 cases /80640 bytes pass. Initial comparison caught a reversed A4 gate;
the corrected phase0 waits until A4 is nonzero. Global9D suppresses all phases;
phases1..4 advance only with8B nonzero. Phase2 activates and resets the object;
phase5 clears descriptor active without an8B dependency.

Trace SHA2564657492d67e756a403148271a7f75a5e7e6e0839e20bf6243135354a271b1ec8.
Native source SHA256f30b7c6190558bac07f5215caeaef29602b7b9532c23b3430af94e06dc48da39.
Build succeeds; unused sequencer is not yet bound into gameplay, so production
ROM remainsb5ada581a46b5c341c374c6794678ecff504f3adbb237dbac877f8b4408073ed.
Next: recover the native owners of readiness/advance, bind special-object
animation/contact and background events, then target and controller-driven
validation. These host comparisons do not establish normal-play reachability.
MAME ran without video output, with window flags, and exited after each trace.

## Live scroll-column signal and pickup readiness ownership (2026-10-22)

Recovered A697's column pulse: advance the unsigned24-bit scroll position by
008D; if the new integer camera bit3 equals latch93, toggle that latch and
emit8B=1, otherwise8B=0. Startup8FE7 initializes the latch to8. Added
arcade_scroll.h/.c and routed world_scroll_tick through it while preserving
the existing ribbon-start clamp. game_arcade_column_advance exposes the result
for special-object scheduling. Stop-state dispatch remains unported and this
helper does not pretend to implement it.

arcade_scroll_signal_trace.lua invokes actual A697 including column expansion:
3584 fixtures cover every fractional byte, seven cameras including tile and
16-bit wrap boundaries, and both latch values. verify_scroll_signal.py passes
14336 host field comparisons. verify_scroll_signal_target.py passes all3584
fixtures against the linked 68000 routine. The first source experiment showed
one initial fixture not executing its scroll update; a stable RAM-loop warmup
before fixtures resolves that initial execution issue. Every recorded fixture,
including the first, is now compared; no cases are skipped.
Trace SHA2564381e5b0633e73ab6aecd37a7d4c87074bc15012009e57198d33197f0efac3bb.

Readiness A4 is the remaining source24-cell sprite budget: FECF initializes24,
FF4A/FFB3 consume one visible admitted cell, FF6A consumes four for large actors;
C324/C424 also adjust its reservation. Therefore pickup phase0 must not use an
unconditional readiness value. Full ordering/reservation ownership remains to
be bound. Background event fields were renamed codes/attributes: the buffers
03A0/03C0 are tile codes and attributes, not two vertically stacked tile rows.
The spawn oracle still passes768 cases/80640 bytes after this semantic rename.

Current ROM92d1516e32acdb115c4ef40f6dd6368f7188df2984165f485653841e7cbc9ec3,
655360bytes. Controller-driven Home2P cold-start and first-update verification
passes, including2112 resident bonus-art words. Full gameplay/powerup binding
and sustained performance remain unverified. All test emulator processes exited;
tests used video disabled and explicit small-window flags.

## Original full sprite admission order (2026-10-22)

Added arcade_sprite_budget.h/.c, a native logical-cell allocator with nine-byte
admission mask. FED8 initializes24 cells, visits special3, player shots4,
players2 (second suppressed when89 nonzero), ordinary55 in ascending record
order0758..0E18, large3, then K4. Singles skip active0 and bit80; large actors
skip only active0 and require four available cells atomically. Source admission
does not cull by viewport or charge extra for coordinate-wrap presentations.
Admission stops per actor, not per pool: a large actor rejected with1..3 cells
remaining does not prevent later K singles from using those cells.

arcade_sprite_budget_trace.lua executes original FED8 on512 deterministic
fixtures, including ascending saturation and mixed0/1/2/65/128 active flags,
with both second-player suppression states. verify_sprite_budget.py checks
all49152 output sprite bytes, ordering, cleared tail and remaining budget;
72 large actors are admitted in the fixture set. All checks pass.
Trace SHA2566b49b9baefe03ace820444b01b47bd17aa51602509ff649ff0e9e1ca877f523e.
Source SHA2560f01bbca2fdd0452913da7cfbdf8978b1e320fd63f57870b0025fb6db435c31f.

This allocator is not yet connected to the entity pool: the live renderer still
derives some readiness from emitted Genesis SAT entries, which is not equivalent
to source logical-cell admission. Mapping the source pool order and C-family
reservations remains necessary before using A4 for pickup readiness. Attract
overlay behavior and target CPU validation remain pending. Build succeeds;
unreferenced helper is discarded and ROM remains92d1516e. MAME ran headlessly
with small-window flags and exited. No new performance or whole-game parity claim.

## Shared G-weapon / large-projectile child allocation (2026-10-22)

Mapping source sprite slots exposed separate native allocations for two users
of the same source CBC8 pool at0EC8..0F27 (handler pointers0ED8..0F38).
G throwing previously searched only arcade_weapons; large firing searched only
arcade_k_actors. Both could claim the same source slot, admitting eight children
where the source has four. Added arcade_foe_step_shared with an opposite-kind
reservation mask, retaining the original no-reservation API for isolated source
replays. The live G spawn sees every active large projectile, and large spawning
sees both types. Within each scan, the first jointly free slot wins. Existing
step ordering is preserved; neither side temporarily modifies the other's state.

verify_shared_k_binding.py reproduces overlapping slot0 on pre-fix ROM92d1516e
(before.json retained), and passes32 live entity_update_all scenarios /256 checks
on the new ROM: both producer types, all16 opposite-type occupancy masks,
including all full/empty and fragmented combinations. Both pools still use
separate typed storage; cross-kind stale-field retention, score children and
shared rendering/collision order remain unresolved. Do not equate this allocation
fix with complete shared-pool parity.

Regression: host G/H replay19200 updates /1293960 fields passes; linked68000
replay19200 cases /1656776 checks passes. Current-build large binding8 scenarios
/312 checks and bonus lifecycle44 scenarios /968 checks also pass.
ROM216d9b090a9a2ae4ccd4c5fc9ab43904ec90f58a7f91ab6de9a9d436f3cdab1c,
655360bytes. Build succeeds. Natural powerup binding, source sprite-order mapper
and sustained60 remain open. All emulator tests ran with video disabled and
small-window flags, and all sessions exited.

## Cross-kind child subpixel retention (2026-10-22)

Shared slot ownership also requires retaining source record bytes7/9 across
kind changes. C84E..C87E and D76A..D798 overwrite integer birth positions but
leave fractional X/Y untouched. Existing isolated source traces preserve these
bytes through each kind's cleanup. Separate native records previously retained
their own old fractions, losing the last occupying kind's position remainder.

After each active G-weapon/K update, the live bridge now copies the two fraction
bytes to the opposite typed view, including the update that releases the slot.
Inactive slots do not overwrite their retained value. This also covers a release
during the later collision pass because the active update already synchronized
its final fractions. It does not alter birth coordinates or arithmetic within
either verified projectile implementation.

verify_shared_k_reuse.py executes cleanup then opposite-kind birth through two
live entity_update_all calls for both directions/all four slots. The regression
fails on216d9b09 at reused X fraction0 (before.json retained), and passes eight
scenarios /24 assertions on the new ROM. The32-case shared occupancy regression
still passes256 checks; K contact priority passes300 updates /1500 checks.
Other source shared fields and score-child ownership remain unresolved; this
fix is specifically for the motion fractions, not complete record equivalence.

ROM91d797b8743bb6b0ee7a55b1d3cee6e523f2d4dd3fe1b482726149bac306e591,
655360bytes. Build passes. No current-build sustained60 result. Tests used
headless MAME with small-window flags and all sessions exited.

## Mixed shared-child contact and shot ordering (2026-10-22)

Live contact and player-shot scans now visit both typed child representations
in source slot order0..3. Original EDB1 scans active nonzero with bit2 clear,
using byte Y+6<=12 and word X+6<=12; EDF0 sets state/phase3/2 while retaining
active until cleanup. EE15 requires active1 and nonnegative state, uses
Y+7<=14/X+9<=18, and EE44 sets both active2, child3/0, player shot2/0, with
event0430 worth30 points. Neither source scan selects by projectile kind.

Previously G weapons reached a later generic AABB contact scan and disappeared
immediately, while kind2 always won even when its source slot followed a G
weapon. G weapons were also absent from the shared shot-cancellation scan.
The bridge now applies the source geometry, deferred cleanup and30-point
cancellation to G weapons too, and excludes their slots from generic contact.

verify_shared_k_contact.py reproduces the wrong slot selection on91d797b8
(before.json retained); new ROM passes600 updates /4800 checks. Fixtures cover
both kinds in competing slots, both owners, Y/X boundaries, invulnerability and
simultaneous I contact. verify_shared_k_shot.py passes200 updates /1600 checks
for mixed priority, both owners, exact shot boundaries, active gating and points.
These are injected live updates, not controller-driven encounter claims.
K-before-I regression300/1500, large binding8/312 and shared reuse8/24 also pass.

ROM7ecc6c8611bebfbc20b622dd2ea4fc33ea1a5c07842d47f56d8b3b748d45a939,
655360bytes. Build passes. Converted/score K children, full shared-field storage,
sprite-order binding, natural powerup spawning and sustained60 remain open.
All emulator sessions exited; tests used disabled video and small-window flags.

## Current-build performance baseline after shared-child fixes (2026-10-22)

Re-ran headless Genesis Plus GX for42000 refreshes per mode on7ecc6c86:
1P823 missed presentations, worst60-refresh window40, peak reported CPU114.45%,
one ribbon wrap; 2P2467 missed, worst window30, peak141.99%, zero ribbon wraps.
Both modes reached29 entities and three large actors; maximum kind2 projectiles
was3/4 respectively. Lives are refilled by the established controller workload.
The 2P result does not establish complete-course coverage. These are current
absolute performance failures, not equal-work comparisons against older logic.
Core hash88314cd8554a3cc5eebf00a4139e6f0dee9ea579dcb2321a0959579f1902fa09.

Saved ROM, symbols, executable, raw time series and results under
out/parity-audit/performance-baseline-7ecc6c86. verify_second_core now removes
stale results before starting and checks symbol identity as well as ROM identity.
Expanded fixed-work snapshots to include RNG, bonus indices, scroll phase,
shot budget and reservation/availability counters. Two identical-build runs
match16000 completed updates with2914 state bytes per update.

Current section profiles (mean/peak elapsed emulated milliseconds, including
interrupts): players1.709/2.692; entity simulation+collision+scheduling6.741/16.823;
bookkeeping0.200/2.080; background0.079/1.658; entity sprite preparation1.264/8.676;
player sprites/HUD0.472/2.685. The entity split is actor updates3.850/8.818,
collisions2.410/7.698, scheduler0.482/2.371. Peaks occur on different updates and
must not be summed. This identifies actor updates and collision as the next
CPU profiling/optimization targets, while keeping large-render spikes visible.

No gameplay optimization or ROM change in this measurement step. Strict60
remains failed. All headless core/MAME processes completed and exited.

## Actor profiling coverage correction (2026-10-22)

Added profile_actor_functions.py for direct call-site boundaries in the matched
16000-update workload. Initial entry/RTS instrumentation was rejected because
it could not reliably bracket optimized tail calls; a dynamic return-address
experiment also failed and is not retained. The retained probe uses caller
instruction/return boundaries. Its first capture includes work between tap
installation and gameplay fixture activation, so it is explicitly excluded
from per-update statistics (15999 measured updates). The initially reported
hundreds-of-milliseconds peaks were warmup accumulation, not runtime stalls.

Corrected mean/peak inclusive elapsed milliseconds: entity_update_one
1.949/6.860 across155143 calls; arcade_foe_step0.499/2.894 across37514 calls;
arcade_i_actor_step0.100/0.643 across10853 calls. Weapon/large/floor calls use
paths not observed by this direct-call probe and are marked unmeasured, not
zero-cost. Do not sum nested durations or infer source parity from timings.
The earlier unconditional section-boundary profiles remain the broader cost
evidence. No ROM change or optimization claim in this step; all MAME sessions
exited. Current production remains7ecc6c86 and the60-FPS goal remains open.

## Retained shared-child scan optimization (2026-10-22)

shared_k_target now rejects empty/ineligible active flags and negative shot
target states before loading coordinates. Slot ordering, byte/word geometry,
contact/cancellation writes and points are unchanged. All800 mixed collision
fixtures pass (600 contact /4800 checks,200 shot /1600 checks). The matched
two-player workload compares16000 completed updates at2914 state bytes each,
including RNG/shared counters, with zero differences against7ecc6c86.

On identical completed-update inputs, collision mean falls2.409568 to2.368544ms
(1.70%), peak7.697720 to7.666170ms. Actor-update timing is identical; scheduler
mean0.481592 to0.481489ms. This is a modest measured CPU improvement and is
retained. It does not establish a60-FPS floor or completed source parity.

New42000-refresh Genesis Plus GX runs still fail:1P794 missed presentations,
worst60-refresh window40, peak CPU108.84%, one wrap;2P1634 missed, worst29,
peak129.15%, zero wraps. Actor counts/encounter timing differ from the baseline
under refresh-timed input, so lower totals are not equal-work speedup evidence;
the2P worst window is still below the prior30. Use the fixed-work profile above
for the optimization claim and these absolute failures for release readiness.

ROMd39df0b0bc73263bb058715723e048be11c5c504df880f7cd2f7820255b0db43,
655360bytes. Reports: workload-comparison, shared-k-scan-optimization, shared-k-
contact, shared-k-shot, and out/genplus-results.json. All emulator/core sessions
exited. Remaining fidelity and performance requirements are unchanged.

## Native FE/FF special-pickup lifetime (2026-10-22)

Added arcade_powerup.h/.c, a14-byte typed actor for special kindsFE/FF.
Recovered B19A phases: initialize Y=placement*8 modulo256, X=camera+248,
32-update appearance hold, then animate and retire when unsigned16(X-camera)
is at least248. State3 clears active/state/phase on its next update. FE has an
eight-frame cycle with attributeF9; FF a four-frame cycle with attributeCB.
Both animation timers are5, with decrement-only updates before advancing.
The exit latch changes only if previously zero. Ordinary fruit/scoring states
are deliberately outside this typed component and remain to be implemented.

arcade_powerup_trace.lua runs original B19A for24 continuous512-update
trajectories: both kinds, four placements including byte wrap, moving/stopped
camera with16-bit wrap, and scripted state3 retirement. Every trajectory reaches
inactive, including natural offscreen retirement. verify_powerup_lifecycle.py
passes12288 source updates /159744 native field comparisons with continuity
checks and preservation checks for source bytes outside handler ownership.
An initial fixture left boot globals live and stopped returning; clearing
unrelated direct-page globals each injected update isolates the routine. No
claim is made about the cause of that initial fixture failure beyond isolation.

Trace SHA256d3b9d04b7222937def70e9b563d55b3305a3ec39a0b60d5512bd4a896f21a17f.
Native SHA25603a64b6466b231035f67eb607bd5ee0024860345728f3fe9ac15b83b4ef3692c.
Build passes. This component is not yet bound to live spawn/contact/render;
unused code is discarded, leaving production ROMd39df0b0 unchanged. Target CPU
validation, powerup graphics residency and normal-play integration remain open.
MAME ran headlessly with small-window flags and all sessions exited.

## Special-pickup canonical art (2026-10-22)

Added gen_arcade_powerup_art.py to decode the original B2A7/B2C1 animation
cycles into nine distinct ROM cells (1152 bytes), with direct code/attribute
lookup and explicit unsupported selectors. Source inputs are hashed in
out/parity-audit/powerup-art/manifest.json. No VRAM addresses are assigned: the
22-tile pre-animation gap is insufficient for all36 tiles; live residency and
spawn/contact/render integration remain pending.

verify_powerup_art.py independently decodes raw bitplanes and palette LUT,
using observed original CPU lifecycle poses as selector coverage. All2304
pixels and65536 selectors pass, including flip-bit invariance and unsupported
selectors. This verifies canonical palette indices, not final screen colors.
Generated source SHA256:787b2de4d73927da4eafbddafcf67ce1e2815a7eb6bf979a37f817acf357ed13.
No emulator launched during this step. Strict60FPS and full arcade parity
remain open.

## Special-pickup VRAM upload arena (2026-10-22)

Added arcade_powerup_upload, reserving [268,280) for the three source special
object slots. Each slot uploads only its current16x16 cell, so all nine
canonical poses fit over time without moving the scene, bonus, animation,
world, large-enemy or font arenas. Compile-time overlap guards check both
neighbors. At most384 bytes change per presentation; unchanged slots reuse
resident art. Sources are immutable ROM pointers. Preflight reserves later
map/SAT bytes and descriptors and accounts for128KiB DMA source splits using
the pinned SGDK queue API. No custom bus protocol or extra queue flush.

verify_powerup_upload.py passes ASan/UBSan mocked deferred transfers, invalid
selectors, byte/descriptor refusal and retry, dirty reuse, reset, arena
guards and all729 simultaneous cell combinations. This is host queue testing,
not target VDP/split-transfer/timing evidence. Live actors and SAT publication
are not yet bound; the helper is discarded from the production ROM. Build
passes. Source SHA2566a5be7969947bfa4790b8fbc7879ac1c0683b0d058cf2363a8b617ed655a085e.
No emulator launched. Arcade parity and sustained60FPS remain incomplete.

## Native conversion-pickup collection (2026-10-22)

Added arcade_powerup_collect_conversion for original E7F3/E868 FF collection.
It requires active exactly1, byte Y delta within7 and word X delta within3;
writes active2/state3/phase0 and collector ownership, sets the conversion
pulse, resets next bonus index, and increments the limit with byte wrap then
clamps values above3 to0. Player eligibility and source slot iteration remain
caller responsibilities; FE behavior is not silently mapped to FF.

Added owner to ArcadePowerup (host size16); lifecycle verification now checks
its preservation against raw source records. verify_powerup_collection.py
passes all900 original CPU cases, both owners, active0/1/2, six limit values
and coordinate boundaries/wrap, including preservation of unrelated fields.
Trace SHA256f8568925852235e1e0462afcf247c069b1083a15ef8e8e08c4f04b96ec2772a4.
Current native source SHA256bd43ed98b166bf7d7875e1ca434ac29ba8e6b1f356a3a48e0b614a2ffb8495a7.
The helper is not yet live-bound. Natural spawning/background updates, sprite
admission and conversion signal lifetime remain necessary before end-to-end
claims; target execution and normal-play validation are pending.

## Ordinary pickup award and conversion request (2026-10-22)

Original E7D6/E7DF plus score consumer4CBE captured480 successful collection
fixtures: kinds0..9, both owners, collection counters0/6/7/8/254/255 and four
combat grades. Results confirm1000..10000 points, sound5, combat+C0 through
source clamp, byte counter increment, and FB95 request when incremented count
is at least8. Count255 wraps to0 without a request. FB95 changes conversion
kind toFF and descriptor active1/phase0/placement68; it does not directly
activate the conversion actor or clear unrelated descriptor fields.

arcade_pickup_collect implements this transaction and the source byte-Y/word-X
contact bounds. Host comparisons pass480 cases/5760 recorded fields; existing
converted G/H award regression passes176 cases. Build passes. Trace SHA256
f8faafa5d5b1a92bcf3c2dc0ca91f348c0386b478b22cff7aaca2d6a25f9a434.
This new trace covers successful contact only; misses are not independently
validated for ordinary kinds here. Kind10's variant-dependent award is still
missing. The new helper remains unbound: four manually placed legacy fruits
and their incorrect ordinary scoring are still live. Source ordinary spawning,
animation and collection binding must replace that path. MAME ran headless
with small-window flags and exited. No full parity or60FPS claim.

## Ordinary pickup lifecycle and missed-fruit orb (2026-10-22)

Added arcade_pickup_step for kinds0..9, reusing canonical bonus animation
records. Source B19A establishes the32-update appearance hold; uncollected
fruit stays static. Collection state1 advances once to the score pose, applies
owner2 attributeC3, sets96 updates then immediately processes the first timer
decrement. Offscreen checks use unsigned16(X-camera)>=248. Missed fruit with
zero animation timer and no exit latch requests an orb (variant1 only for
fruit kind9), halves its kind, latches exit and retires. The helper emits the
orb request; caller ownership/reference binding remains pending.

New original-CPU trace covers60 continuous512-update trajectories: ten kinds,
both owners, moving and stopped cameras with16-bit wrap, and scripted
collection at tick64. Native host comparison passes30720 updates/430080 actor
fields plus orb event/pointer checks. Raw untouched actor fields are checked.
Trace SHA256f0b6d3702324b665975ca766c8dc9e5d168b86aebb496079e8c993abd5de7165;
native SHA2566fcad50223e01e634e90848a0f606bfa78c62f3a41ba972c01207acf3b438702.
This is isolated source/host evidence, not a target or normal-play claim.
Kind10, source food-tile collection/spawning, background edits, orb binding
and replacing the manually placed live fruit remain open. MAME ran headlessly
with small-window flags and exited normally.

## Six-food ordinary pickup selection (2026-10-22)

Added arcade_food_collect for E607. Signed food codesF4..07 map to values1..10
in pairs. After six values, C29C divides the sum by6; the source rounds halves
up, subtracts1 and selects from eight ten-entry progression tables atE652.
Grades above7 select row7. The transaction resets count/sum, assigns ordinary
pickup kind and clears its exit latch, then requests descriptor active1/phase0
with the collected food's Y. It preserves the descriptor row and does not
activate the pickup directly. Native input guards reject out-of-domain counters
and food codes; actual upstream food-code reachability still needs integration.

Original CPU fixtures cover all28200 combinations of count0..5, each possible
aggregate of that many values1..10, both raw codes per value, all eight table
rows plus grades8/255. Host comparison passes225600 recorded output fields.
Trace SHA256f50978e09ae744f6c8184ae75aa57376eb0c0914e85491f4a8f3dc1583474a27;
native SHA256539868570c45e99ceaae3cfb40dcb892bbe757ae6962b16dc81519ec3f8d8fac.

Initial rangeF2..05 inferred from stale linear disassembly labels was corrected
toF4..07 against raw bytes. A separate first-fixture contamination produced
sum51; entering the fixture RAM loop for one warmup frame before injection
removed it. Both failed CSV probes are preserved. No specific interrupt cause
is asserted. Corrected original runs exit normally, headless with small-window
flags. Build passes. Food contact/streaming, kind10 reward, target validation
and replacing live manual fruit placement remain pending; no gameplay or60FPS
improvement is claimed from this isolated component.

## Background food contact transaction (2026-10-22)

Added arcade_food_contact for E569/E5BB kind0 food tiles: player active exactly1,
tile active bit0, unsigned camera-relative X<256, byte-Y/word-X bounds6.
Successful contact sets active4/phase4, emits source event0401..0408/0432/0433
with owner bit, sound4, and10..100 points. The caller invokes the six-food
aggregate only on successful collection. This background food is distinct
from the ordinary16x16 pickups (1000..10000 points).

Original E55F/E565 plus actual4CBE BCD consumer fixtures cover1240 cases: both
owners, all20 valid raw food codes, player flags0/1/2, tile flags0/1/2/3,
visible/offscreen camera gate and contact bounds including byte/word wrap.
Native composed contact/count transaction passes8680 recorded fields. The
28200-case aggregate regression also passes. Trace SHA256
c233b8057ec6d5ecafad7b5053ead5005a594af206fc5ff4c02a281047cee4ac.
Current native source SHA256fbb797cd7c5253bbc3daf35420bc83a0014153b8c9838a2023ed69c79906dd79.
Build passes. MAME ran headlessly with small-window flags and exited normally.
World food descriptors/background streaming, multi-tile source scan ordering,
target tests and replacing the existing live fruit path remain pending.

## Complete background object placement and rotating pool (2026-10-22)

Extracted the complete348 ten-byte records at5587..631E and27 segment
boundaries from5551. Counts per segment match four records per screen and
all87 source screens:242 kind0 food records,64 kind1,42 kind2. The generated
native table preserves world coordinates, initial states, variants, timers
and source-map reference metadata. Source-map values are not Genesis VRAM
addresses; their rendering translation remains pending.

arcade_world_object_load replaces four records in a20-object pool, advances
the source block and wraps five destination quarters, matching54E7. It does
not search for inactive slots. Source exhaustion leaves pool and cursors
unchanged. Original CPU oracle440 cases covers every87 source blocks plus
end sentinel at every destination quarter. Compiled native comparison passes
88000 canonical pool bytes, source/destination cursor checks and27 segment
offsets. Trace SHA256d83b0a9c0939a78b82c415779ca8921bb0af64b9d01e468c1b414a5c82b1a3be.
Pool source SHA256854ebbfa4c8a5188ab1b8fbb8243e021ac55a3a1ec52c358f0a0456e9c2142e4.
Build passes; MAME ran headless with small-window flags and exited. These
components are not yet bound to live scrolling, object update/render/contact,
or validated on target CPU. Full parity and60FPS remain open.

## Food appearance and collected-background phases (2026-10-22)

Added arcade_food_step for original B36F kind0 phases0..5. Phase0 waits for
nonzero Y and unsigned(camera+256)>X, then falls through to first-column
processing. Column advance admits the first and second halves independently;
second-half completion sets active1/phase3. Collection phase4 emits the full
2x2 eaten-food patch and sets active4/phase5. Idle phases3/5 do nothing.
Canonical source patterns atB403/B5C3 are generated for all20 signed food
codes; events retain character/attribute roles for later Genesis conversion.

Original CPU fixtures2160 cover all foods, phases, advance gates, entry
thresholds255/256/257 and Y0/104/255. Native comparison passes21600 actor
bytes and157680 source background bytes, including column-buffer row31
straddling. Trace SHA2564a55e04fba4d4a86f3321d228e3f85b6b602251f79412f10426b59fadf59e5e8.
Native source SHA2568306180923d36bc0b18e470077665e10bdcf68c06ced83808459cfe651d30aa2.
MAME ran headlessly with small-window flags and exited. Genesis graphics
translation, live pool scheduling/render/contact and target execution remain
pending. Full parity and sustained60FPS remain incomplete.

## Food tile art and resident sharing (2026-10-22)

The previously pending build completed successfully. Food/eaten patterns need
88 distinct character/attribute selectors. World-only reuse leaves85 new
tiles; all immutable scene/bonus/animation/world banks reduce this to76.
World horizontal/vertical exact sharing would save only1 tile (575 to574),
so no world repacking was retained.

Added gen_arcade_food_art.py, emitting40 four-tile poses and2432 new pixel
bytes. Proposed ranges are[280,288) and[1372,1440), borrowing exact immutable
resident tiles for the other references. Pinned SGDK64x32 plane layout places
the first table atC000, yielding font base1440 (96 tiles); earlier1376 notes
were conservative assumptions rather than confirmed current configuration.
These new allocations require target verification and a font-base guard
before live use. They are not yet loaded by gameplay.

verify_food_art.py independently decodes raw character planes and palette LUT
against all compiled maps and new/borrowed pixels:10240 pixel checks pass,
covering all40 poses. It also rejects overlap with existing immutable banks,
powerup and large-enemy arenas. Source SHA256
73bfd7940253b7c83596a9dc1d908e79a8d08e381ccfc2b3abc4fbc0a9550c32.
No new emulator launched in this step. Target VRAM readback, live food binding,
source parity and sustained60FPS remain pending.

## Food art target load/layout validation (2026-10-22)

Added arcade_food_art_load for cold scene loading only (display off and prior
DMA flushed). It rejects font base below1440 and uses ordinary SDK immediate
ROM DMA for eight tiles at280 and68 at1372. Compile-time guards check neighboring
powerup/animation/large allocations. This loader remains unbound to game_init.

verify_food_art_target.py builds an isolated SGDK target with food pixels placed
at1FFC0, forcing the first transfer across a128KiB source boundary. Actual
68000/VDP testing confirms H32/64x32 layout font base1440, rejection under
the incompatible64x64 layout, all1216 words (76 tiles) read back, and four
boundary sentinel words unchanged. Report done1/error0. Standalone test ROM
SHA256cad1f49a2ae6dcfb7bab2ba608f48f521fcb0357c804e20807e2b61a9135f363;
loader SHA25627490f8dec41905df59a0ce69ba3a3266a3400bb1ee4d2c528c4532060ad0b31.
Production ROM identity was checked unchanged by the isolated test. MAME ran
headless with small-window flags and exited. This proves the specified cold
load/layout, not live moving backgrounds, gameplay parity or audio stress.

## Initial world-pool preload, verification still open (2026-10-22)

Added arcade_world_object_begin: copies the first16 source records, clears
only active flags in the remaining four records, sets block4/quarter4. This
implements the data-copy interpretation of8FC5 plus54B0 for segment0 and
compiles successfully. It remains unbound and its original-CPU startup test
is NOT passing. Do not include it in the440-case scrolling-loader claim.

Injected54B0 probes returned toPC4006. The routine's ANDCC EF IRQ enable is
a suspected interference point, not an established cause: IRQ-vector RTI
isolation and an operand-read override also failed. Both probe scripts and
boot-probe-failure.log are retained. No altered-IRQ fixture was accepted as
source proof. Original54E7 trace was regenerated separately and still passes
440 cases/88000 pool bytes with unchanged trace SHA256d83b0a9c0939a78b82c415779ca8921bb0af64b9d01e468c1b414a5c82b1a3be.
Startup validation, full live food integration and all prior fidelity/FPS
requirements remain pending. All emulator processes from this step exited.

## Preload copy and initial food redraw (2026-10-22)

The completed global operand-tap probe now passes five segment0 preload
cases. verify_world_objects.py compares all200 pool bytes per case, source
and destination pointers, and tail-field preservation. It reports this
separately in boot-copy-results.json: ANDCC EF was overridden with ANDCC FF,
so this proves the isolated copy result, not uninterrupted arcade startup.
Trace SHA25638cb264244fd30ba94923c427cd7b230d1d4e4aa8ea40de015aca8611e102668.
The unmodified54E7 regression still passes440 cases/88000 bytes.
The earlier memory-read trace establishes IRQ entry at910D immediately after
54E0 unmasks IRQ; the exact subsequent reset cause remains unproven.

Added arcade_food_redraw for sourceB922/B952 kind0 behavior: activebit4 and
unsigned wrapped(camera+256)>object.x select a full normal-food patch and
set active1/phase3. This differs from ordinary scrolling entry: no advance
signal or nonzero-Y gate. The original CPU fixture tests all20 food codes,
activeflags0..7, deltas255/256/257, normal and wrapping cameras and three
vertical rows. verify_food_redraw.py passes2880 cases,28800 actor bytes and
23040 background bytes. Trace SHA256
3a615869c47a692ea0c3c2b594b3c355e6d5a285b9b643cf1c2b7acbe0aa9522.
Existing scrolling/eaten food regression2160 cases also passes. Build passes;
production ROM remains d39df0b0bc73263bb058715723e048be11c5c504df880f7cd2f7820255b0db43.
MAME ran headless with small-window flags and exited. These helpers remain
unbound; live background patches, gameplay food contacts, special spawns,
remaining enemy logic and sustained60FPS still require completion.

## Live food background/contact integration (2026-10-22)

Added arcade_food_world to own the20-record rotating source pool, initial
redraw, streamed food phases, contact scoring and six-food descriptor request.
Standard gameplay now uses this owner instead of the four manually seeded
fruit sprites. Survival keeps its existing custom fruit behavior. Cold game
initialization disables display and flushes old transfers before loading the
food art; normal rendering queues immutable ROM tile references after world
column streaming. Pending column masks survive admission refusal. Tile writes
use the64x32 Plane A ring and the current ribbon phase. Existing ribbon-loop
reset policy is preserved; full arcade end/restart behavior remains unproven.

Standalone actual68000/VDP test passes initial four tiles,20-point F6 pickup,
rejection of a second owner's duplicate contact, queued eaten patch, and a
horizontal ring seam. Three batches/12 nametable words, error0. Test ROM SHA
9dfc4423fe80f108a36982a8a3e527b4a2aea0a2d43cd5a2da2b194637e2e007.
Native world owner SHA
9e9750092d95f882ca7cf69dc5e62a9a34f9c96379e4f685ae14eef1ca33a9ba.
Host contact1240, six-food28200 and initial redraw2880 regressions pass.
Production build succeeds, ROM SHA
5160a99bc09e (prefix; full identity in build-manifest.json).

This is an incomplete live integration, not whole-food-system parity:
shared ordinary/special pickup spawning still needs to consume the retained
arcade_food_spawn/arcade_food_pickup request; request grade ownership, exact
whole-frame update order, source score-popup events and all non-kind0 objects
remain open. Runtime currently supplies native stage as grade, which requires
source call-site verification before bonus binding. Sound04 is issued once
per owner with a successful contact batch. Frame timing must be remeasured on
this new ROM; prior FPS evidence applies only to the previous ROM.

Controller-driven captures completed for1P and2P, five screenshots each, lives
refilled but no food/enemy state injection. New files foods-1p-01..05.png and
foods-2p-01..05.png with food-manifest.json bind the ten images to full ROM SHA
5160a99bc09e28ddce4cde08edf99ab67e10ba25a3939ef0f32ee766226aca3c.
First2P attempt timed out before output and terminated; the retry completed.
Both successful runs exited; no emulator from these tests remains running.

## Food grade, scheduling and hot-path correction (2026-10-22)

Source E630 selects the six-food table with007E, not base/combat difficulty.
8F68 clears007E on new game;9A18 increments it at campaign completion.
Live game now owns a separate g_foodRound, initialized0 and incremented by the
existing ribbon-completion path, and passes it to food contact. Full arcade
campaign-ending flow remains pending; this fixes the previous erroneous
DIP/stage-dependent bonus grade without claiming the ending is equivalent.

A5EE callsB36F before enemy logic and E2E0/E334 food contacts. Moved the live
food step ahead of entity_update_all and contact checks. A successful pickup
now leaves phase4 until the following food update, matching this source order.

Avoid constructing contact copies for inactive/non-food/offscreen records and
skip food-step calls for inactive/non-food and stable phases3/5. Deterministic
host equivalence against the prefilter-free owner passes80000 updates in1P/2P,
375 successful contacts, pool recycling, contact edges, queue refusal/retry and
final nametable comparisons. Native SHA
63a2569d3a6443eef3718ebcb3a746f3eeea4d9df81c201c880d446a025507c5.
Actual68000/VDP food-world test still passes three batches/12 words,error0.

Initial MAME timing attempts on intermediate ROM0d4a8e2f... ended early:
30370/42000 samples1P and23824/42000 samples2P. They demonstrate missed
presentations but are incomplete workload runs, not accepted full benchmarks.
The optimized, reordered ROM is being measured separately. Bonus request
consumption, source logical sprite budget binding and special-spawn background
strip translation remain pending; no bonus appearance fidelity claim yet.

Completed Genesis Plus GX measurement on ROM
412f4ff52fb61ac32223938cc32f47e2f4442ad4c8d76783005c4fb8d8afbb94:
both modes42000 refreshes.1P1239 missed presentations,worst60-refresh window35,
peak119.824% CPU,one ribbon wrap.2P3760 missed,worstwindow29,peak140.674%,
no ribbon wrap. Strict60FPS gate FAILS both modes. Full reports/CSVs preserved
under out/parity-audit/food-live-optimized. Changes to gameplay workload mean
these are current-state measurements, not an isolated optimization speedup.
All emulator/core tests from this step exited; no visible fullscreen windows.

## Ordinary six-food bonus live binding (2026-10-22)

Added arcade_food_bonus owner. B0D1 descriptor events now write the four entrance
columns and activate the ordinary pickup at phase2. B19A lifecycle runs after
the descriptor, including the birth update. E823 collection feeds score,
combat difficulty and sound05; the actor shows the existing ordinary score
pose with the correct owner palette. Bonus cells participate in the native
body/SAT budget. Source sprite priority still differs because player cells
remain first in the existing renderer.

Entrance characters1A..1D and0A..0D,attribute42, exactly reuse resident scene
tiles60/57/58/59 and56/53/54/55. gen_arcade_food_art.py now emits these mappings;
no extra VRAM or pixel data is needed. verify_food_art.py independently checks
10752 pixels including all eight entrance tiles against raw planes and LUT.

verify_food_bonus_target.py passes actual68000/VDP checks: blocked/capacity
phase0 gates, queued entrance columns, descriptor birth with same-tick actor
initialization,6000-point award and combat increment, duplicate rejection,
owner2 score pose and SAT wrap copy. Three batches,error0. Target ROM SHA
9f863a2b1e195c177195addc30852a04bfd6c4571eeff117ff894837f6c374a6;
bonus owner SHA36a126b8070047e0cf7b231adc0886f6188f6432dde62e488cf0be4d549e5b6d.
Existing480 award cases and30720 lifecycle updates still pass.

Live controller runs, with lives refilled only:1P did not collect six foods
in the short route.2P produced a natural request atframe3230, birth3259,
collection3531, score expiry3629. Full transitions retained in
out/parity-audit/food-bonus-live/2p.csv. No food/pickup actor injection was used
in that live run. Standalone target request is injected and separately scoped.

Remaining integration limits: descriptor blocked input currently follows the
native always-scrolling path; ready uses the existing prior-frame native
sprite-capacity flag, not the verified FED8 allocator. Missed-fruit orb and
eight-bonus conversion requests are retained but have no live consumer yet.
These are incomplete features, not discarded arcade requirements. Complete
source scheduling, rendering priority, natural1P bonus capture and sustained
60FPS remain open. Final build ROM SHA
0ef717dc2229c66e36d0bcbdc2989cce2a6ed4a90dc5fc432d81b3950aeebe98.

Final ROM full42000-refresh tests fail strict60FPS in both modes:1P1637 misses,worstwindow34,peak130.127%;2P6005 misses,worstwindow29,peak144.043%. Results preserved under food-bonus-live/performance. Both short live capture runs and both core performance runs exited.
The final-build focused preview run exited at41 emulated seconds before its
first requested capture; no bonus preview image was produced. It is not a
completed live reproduction. The earlier2P transition trace remains the
natural-spawn evidence for the integrated bonus owner before the body-budget
accounting correction. Final-build normal-play reproduction remains open.

## Conversion pickup and pulse binding (2026-10-22)

The second B0D1 descriptor0558 now shares the live entrance renderer, after
0550, using separate pending-column storage. It activates the FF actor, which
runs the verified special-pickup lifecycle/animation. The fixed second special
VRAM cell272..275 receives only changed immutable pixel data through the
checked uploader. SAT publication waits for successful upload admission.
Collection uses the verified FF geometry/owner contract and produces the
conversion pulse. The first conversion limit is corrected:8F6E initializes
7B toFF, so first collection wraps to0, followed by1,2,3,0.

Call-site evidence:97AE invokesA5E2 (actor update,ending withA61F clearingA1),
then97B1 invokesE2B3 contacts. E1AF is an actor dispatcher, not the player
contact pass. E2FB invokes the special-pickup contact scan. Thus collection
setsA1 for the next actor pass. Native entity_update_all snapshots the pending
pulse into arcade_world.purge, updates supported actors, then clears it before
contacts. G/H enter the verified FF bonus behavior; shared K projectiles and
G weapons use their existing purge behavior. Other enemy-family conversions
are still incomplete and must not be represented as full-screen parity.

verify_conversion_live_target.py passes actual68000/VDP tests for eight
ordinary awards requestingFF, both descriptors' overlapping entrance writes,
birth atcamera+248, animation pixel upload/readback, pickup collection with
initial limit255, and pulse surviving pickup retirement. It extends ordinary
bonus tests:three batches/16 nametable words,error0,plus64 sprite-pattern
words. Standalone ROM SHA
58c503da5ad157b1c9067ca7b7f765d4b3ee6ca4d5960969ff03a9087dff650a.
verify_conversion_pulse_integration.py invokes the production entity loop:
two updates/16 checks pass for G/H phase reset then advance, first kind0,
pulse clearing, and K projectile purge/cleanup. Actor states are seeded;
this is integration evidence, not natural eight-bonus reachability.
900 source collection and12288 lifecycle regressions also pass.

Production build succeeds,ROM SHA
f68140a278fb5acdb9f2d0a0668ca7215f07c5062d208e4766b0835f3f866206.
Remaining: missed-fruit orb owner, other converted enemy families, exact
logical sprite allocator and scroll-stop gate, rendering priority, natural
controller-driven conversion collection, audio stress and sustained60FPS.

Current conversion build full42000-refresh runs:1P1887 missed presentations,worstwindow36,peak129.590%;2P9671 missed,worstwindow29,peak154.395%. Strict60FPS fails. Both runs ended before a ribbon wrap; course coverage is partial. Full reports preserved under conversion-live-performance. All target/core test processes from this step exited.

## Measured food scheduling optimization (2026-10-22)

Saved production baseline f68140a2... under conversion-perf-baseline. Expanded
compare_native_workloads to include food pool/counters, pending columns,
ordinary/conversion actors and descriptors, pulse/orb requests and campaign
grade:3229 canonical state bytes per completed update. The food-owner dirty
flag is disambiguated from the unrelated static dirty symbol by verified
location. Render upload cache comparison remains outside this snapshot.

Baseline fixed16000-update profile: entity actor/collision/scheduler mean
6.480ms,peak16.315ms. Its inner sections average3.735ms actor updates,
2.262ms collisions,0.482ms scheduling. Food world step averages0.674ms.
These are elapsed emulated times including interrupts; nested durations
must not be summed. Direct-call profiler did not observe indirect food-contact
or floor calls, so zero in those entries is missing coverage, not free work.

Changed only food-step admission: phase0 records beyond the right edge or
withY0 skip the no-op handler, and phase1/2 records skip it without a source
column-advance event. Phase0's transition to1 still runs without advance when
visible; collection phase4 is unaffected. Native art, allocation and rendering
rates are unchanged. Host prefilter-free comparison passes80000 updates with
375 successful contacts. Actual68000/VDP food-world test passes. Completed
baseline/candidate comparison passes16000 updates x3229 bytes with0 mismatches.
An earlier comparison exited early(8336/297 updates) and was not accepted;
its terminal processes were rerun alone, producing the full passing result.
Candidate ROM SHA
53c132ab78b03767d0d92e2df659fd86a40d0d1a86440f81a8c69d964b93ba22.

Matched profile confirms food-step mean 0.673986ms -> 0.353460ms (47.56% reduction),15999 post-warmup updates. Full42000-refresh performance still FAILS:1P1210 misses,worstwindow37,peak117.578%;2P4788 misses,worstwindow29,peak145.117%.1P crossed one ribbon;2P did not. Refresh-driven routes differ with missed frames, so their miss-count improvement is not a fixed-work speedup measurement. Results archived under food-gate-optimization. All emulator/core processes exited.

## Alternating pickup dispatcher and latest performance (2026-10-22)

Corrected live food and special contacts to run inside collision_pass for the
source-clock-selected player. The former game_update loop tested both players
after the scheduler had advanced the clock. Original E2B3 trace covers32
clock/disable combinations: food E2E0/E334 then special E2FB/E34F. Duplicate
instruction reads are collapsed; this verifies visitation, not call counts.
Compiled production entity-loop fixture passes32 updates/224 checks, and
conversion pulse regression passes2 updates/16 checks. The food counter is
reset between independent fixture cases to avoid triggering the sixth-food
bonus accidentally. This is a semantic correction, not baseline equivalence.

Production SHA f1b0e9b9352e33b60c384be324cabedad42bd23b4ec4ec7d6fb70d0baa54d4a0.
42000-refresh runs still fail strict60FPS:1P1409 misses,worstwindow39,
peak114.844%,one ribbon wrap;2P5948 misses,worstwindow29,peak134.863%,
no ribbon wrap. AI and course coverage remain partial.

Earlier fixed-work profile on53c132ab splits ordinary actor updates(mean3.416ms)
from large actors(0.319ms), collision(2.261ms), scheduler(0.482ms). The assembly
boundary label adds no instructions; ROM stayed byte-identical. An attempted
entry-stack profiler was rejected because instruction prefetch made its return
addresses unreliable; its failed logs are not performance evidence.

## Missed-fruit carrier source contract (2026-10-22)

Original revision SHA09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1,
main6809 program space E1AF/E296. B25A arms the unique carrier at0E18, binding
its parent pointer to ordinary pickup0708; variant1 is requested by kind9.
A61C updates it after ordinary pickups and other actors. The existing generic
ET_ORB hovering/lethal behavior is not an implementation of this contract.

New arcade_orb_step preserves the source state transitions: initialize X to
camera-8, take parent Y (plus16 for variant1 with byte wrap), use animation
E26F/E24D, direction1 and a16-update initial hold. E207 moves before decrement;
on expiry E212 moves again in the same update. X is24-bit fixed point, default
speed0x140. Retirement compares only the low byte of X-camera against248.
State2 reactivates the parent, copies X, resets parent state/phase and clears
the carrier. Source animation timer3 means four updates between frame loads.

arcade_orb_trace.lua and verify_orb_lifecycle.py pass12288 continuous updates,
24 trajectories: both variants, Y0/104/240/255, stationary/moving/wrapped camera,
and24 parent returns. Native fields and parent side effects match the original
CPU traces, including untouched parent bytes. Trace SHA
70735600208035e50a27d5a8b579bbcd879ba45208ad42570cdc33741d0ed737.
This new component remains unbound. Live rendering, parent ownership, external
collision/direction changes, actual68000 integration and natural missed-fruit
reachability are still required. No new full-game parity or performance claim.
MAME ran headlessly with window flags and exited normally.

## Live missed-fruit carrier binding (2026-10-22)

arcade_food_bonus now owns the persistent carrier with the ordinary pickup as
its parent. B25A's request initializes only active/state/phase/variant, leaving
source-retained fractional fields intact. The entity loop invokes the carrier
after large actors (A61C), before clearing the conversion pulse and contacts.
Cold bonus reset clears the carrier. Returning fruit reuses the existing pickup
slot and is initialized by B19A's native equivalent on the next update.

The sprite decoder now includes E26F/E24D: ten additional exact cells,19 total.
Observed source poses validate4864 pixels and65536 selectors. Existing three-cell
upload arena stays unchanged: conversion uses272..275; carrier uses276..279.
The carrier obeys wrapped source-cell placement and consumes the shared SAT
budget. Rendering priority and source24-logical-slot allocation remain partial.
Sanitized upload tests cover6859 simultaneous combinations, deferred sources,
capacity failure/retry and untouched arena boundaries.

verify_orb_live_target passes on actual68000/VDP: both request variants, kind
halving, wrapped-camera initialization,128 pixel words read back across both
variants, parent return, and next-update fruit reappearance. Existing conversion
checks in that cartridge also pass. An initial fixture incorrectly required a
visible SAT cell for offscreen Y240/0; it was corrected to visible Y104/120.
Byte-Y wrap remains covered independently by the source lifecycle comparison.
Standalone test ROM c7b3b2693cfe7200f7963079c0497a6c6472623b45779d8a02d415c3f0d42b49.

Production build dcd4e763572b86a6dc7d2bd038fa869efe6f105eca9b44fbfe01c82e636d2e5a,
655360 bytes. Production pickup-dispatch32/224 and conversion-pulse2/16
regressions pass. This is seeded integration evidence: natural missed-fruit
reachability, source sprite ordering and complete enemy parity remain open.
The old generic ET_ORB code has no spawn references outside its generic type
handling and is not used for this carrier. No full parity or60FPS claim.

Latest42000-refresh runs on dcd4e763 still fail strict60FPS:1P1605 misses,worstwindow38,peak118.555%;2P5096 misses,worstwindow29,peak142.432%. Neither run crossed a ribbon wrap. Reports archived under orb-live-performance. Refresh-driven trajectories differ and do not establish isolated speed changes. All test processes completed; no emulator window was opened.

## Special kind10 award contract (2026-10-22)

Original E81F/E852 selects E89F using actor offset+2, independently of kind10.
Added arcade_variant_pickup_collect with an explicit selector; the ordinary
live API retains its kind0..9 restriction until the special actor owner and
animation are recovered. Source award selectors0..7 yield4000,5000,6000,7000,
8000,9000,10000,20000 points and events0C..12,2A. Unlike E838, E852 does not
invoke E407: combat difficulty stays unchanged. Shared geometry, owner credit,
collection count and eight-pickup conversion request are preserved.

Independent source contact plus actual BCD consumer trace passes384 cases and
4608 recorded fields: both players, four combat values and six counter values
including255 wrap. Native ordinary award regression also passes480 cases.
Trace SHA af772e388a4306079c2cb6eeaafa831b4c434c1ea10613f74915a27bebd4f451.
The trace corrected an initial table interpretation that assigned20000 to
selector6; source shows10000 there and20000 at7. This fixture proves the award
routine only: selector7's natural reachability and corresponding score-pose
range are unresolved. No new actor layout or implicit variant-zero binding.
Build SHA 5716868d5904f32f4d25fab9ab3965fdab3a772ddbbf4e749f9ab62af0d7a649. New helper remains unbound; no new full-game parity or performance claim.

## Stopped-section completion owner (2026-10-22)

Located kind10 creation at original main6809 A7D0..A7EF: descriptor0550 gets
active1/phase0/placement104, ordinary pickup0708 gets kind10 and selector007D;
007D increments only below6. This establishes the normal selector ceiling6,
so award selector7's20000 case remains routine behavior outside this normal
progression. A787 is scroll state1, selected through A689/009D. It decrements
009F only when clock0024 modulo64 is0, with byte wrap. If this decrements to0,
it clears scroll state and sets0F48 to0100 without awarding a bonus.
Otherwise all20 background objects are scanned. Any nonzero-active kind2
whose unsigned X is <= wrapping(camera+256) keeps scrolling stopped. If none
remain it clears the stop, sets0F48 and requests the graded special bonus.
The comparison includes the right edge; it is not a signed offscreen test.

Added native arcade_stop_step with explicit release/bonus/variant events.
Independent original A787 traces match1536 cases/12288 recorded fields:
three camera positions including wrap, four clock values, four timers,
eight grade values and four object scenarios with the final pool slot used.
Trace SHA f1e22a523e69d4834f29b19201b108c9a85cebaf07d29e5273bf4df98907e4fc.
The helper is not live yet: section transition table6485, kind2 background
object lifecycle/contact, bonus animation and state2 ending still need binding.
Existing native arcade_world.scroll_stopped=0 remains a known fidelity gap;
connecting this gate without those owners would deadlock or prematurely clear
sections. Source test ran headlessly and exited. Production build succeeds.

Actual68000 standalone test also passes all1536 source fixtures, error0. Test ROM c1831e4a182d6e9099b3932df18534e77ab011b09c308382ec97297c56cbe498. This proves target arithmetic and the local gate, not live stage reachability. Emulator exited normally.

## Destructible background shot scan (2026-10-22)

Recovered original EB11/EB49 for kind2 world objects. A shot active exactly1
scans all20 pool slots, testing active bit1 and kind2. Source centers give
wrapped byte Y test (objectY-shotY+10)<=20 and word X test
(objectX-shotX+14)<=22. Each hit sets object phase4 and shot active2/state2/
phase0. The scan does not recheck shot active after a hit: overlapping objects
can both be hit in one call. Animation lookahead byte(cursor+4)==4 emits042F
with owner bit for each applicable hit. No immediate object deactivation is
performed; the kind2 lifecycle must finish the destruction sequence.

Added arcade_barrier_shots with explicit animation-lookahead input and returned
event count. It remains unbound until the barrier lifecycle owns those cursors.
Independent original CPU fixture passes1680 cases/10080 recorded fields:
wrapped X/Y boundaries, object active0..3, shot active0..2, one/two overlapping
objects with slot19 included, and lookahead phase3/4. Trace SHA
45a4d84cdd7c24b4aebce3c261bd94adc55582c628814b17565a5e3b9238eeec.
The first source harness stopped after4 cases; a32-byte NOP entry runway made
the fixture complete. Incomplete output was rejected; only the full terminal
1680-case run is used. This is a test-entry repair, not a source/game change.
Source MAME ran headlessly and exited. Host comparisons pass; actual68000
runtime validation and live collision ordering remain pending.

## Kind2 barrier lifecycle and background writes (2026-10-22)

Recovered B7CB/B883/B89F/B8C3/B8EA/B91C, shared B753/B75E and B77A/B7AB.
New native arcade_barrier_step retains the source active/phase bytes, timer,
pattern index and explicit native animation cursor. Four nine-record sequences
and36 four-character/four-attribute patterns are generated reproducibly from
verified source ROM by gen_arcade_barrier_data.py. The source loop marker is
represented by a bounded native cursor slot; no arcade CPU interpreter is used.

Phase0 enters only after wrapping(camera+256)>objectX, initializes from its
variant and falls through into the first-column gate. Phases1/2 stream on the
column-advance event; phase2 forces active3/phase3. Phase3 changes only palette
attributes through4D,49,45,41 using clock bits2/3. Phase4 processes the damage
sequence; a nonzero timer decrements without redraw, zero loads the next record
and writes the full2x2 character/attribute patch. Offscreen cleanup compares
only low-byte(X-camera)<=4 and does not skip the remaining phase handler.
The shot lookahead helper matches source cursor+4 for all tested shootable
states and feeds the preceding EB11 contract.

Independent B36F fixture passes6144 continuous updates across12 trajectories:
four variants, stationary/moving/wrapped camera, repeated scripted phase4 hits.
Every mutable actor field and animation cursor matches; exact streamed-column
bytes and full background character/attribute writes match each update.
Trace SHA b730b3ab9f2fd4e4efeb8dac69bf21b2362057d877dc8e3311b363b4d82b468f.
These are original-CPU/host comparisons, not natural-play or Genesis runtime
claims. Native target integration, background art decoding, cold redraw,
player blocking and the live world pool/shot/stop owners remain pending.
The source emulator ran headlessly with window flags and exited normally.

## Player/barrier blocking and left-edge crush (2026-10-22)

Recovered E569/E6FF contact behavior for kind2 objects. Eligibility is player
active exactly1, object active bit0 and unsigned16(objectX-camera)<256.
Geometry uses wrapping byte(playerY-objectY+5)<=10 and word
(playerX-objectX+12)<=24. Negative displacement pushes left by12-abs(dx),
nonnegative displacement pushes right by12-dx. A resulting unsigned screenX
<=6 sets source player kind1 and state/phase0 (crush/life-state entry).
Crucially, E569 caches player X/Y once for the full20-object scan, while E729/
E738 modify live X. Overlapping barriers therefore apply repeated pushes
using the entry position, not updated geometry; the native helper preserves it.

Added arcade_barrier_player returning the crush signal separately from typed
player movement fields. Independent source trace passes5040 cases/20160 fields:
wrapped coordinates, both sides of every contact boundary, object active0..3,
player active0..2, first/last overlapping objects, screen offsets6/100/256.
Trace SHA db6dd110a0fa635fe027cdbe221f839cc1005fc6482566dc98aecce25c44e10a.
The helper remains unbound pending shared world rendering and life ownership;
calling it after an independently reordered scan would not prove parity.
Source test ran headlessly and exited normally. Production build succeeds.

Actual68000 player/barrier test passes all5040 source cases, error0; standalone ROM07e1dd1f0a61778791d243aee7a03f007babefe431aa92bc46102353050189b4. Shot regression1680 cases still passes. Test processes exited. No full parity or sustained60FPS claim.

## Barrier art preparation and VRAM inventory (2026-10-22)

Generated the complete36 barrier patterns with normal attributes plus four
flash attribute choices. The resulting180 poses deduplicate to220 raw8x8
patterns(7040 bytes);31 have exact immutable residents across scene, bonus,
animation, world and food banks, leaving189 additional unique patterns.
These are logical ROM indices, not published VRAM addresses. Resident hints
are verified against every borrowed bank. The full180-pose domain includes
flash combinations beyond currently observed reachable animation states.

verify_barrier_art independently decodes source character bitplanes and palette
LUT:46080 pixels pass, along with all31 resident hints. Index-pixel equivalence
does not establish original analogue RGB or final CRAM fidelity.
Skill tile_sharing_audit confirms complete byte reconstruction; mirrored
sharing saves no barrier patterns and only1 of575 existing world patterns.
Reports/raw hashes are retained under barrier-art. Thus flip-sharing alone
cannot provide enough VRAM; a bounded live cache or a verified residency-layout
change is required before publishing these maps. Per-pose new-tile need is at
most4, but that is not the simultaneous-scene allocation requirement.

Production build succeeds. New art remains unbound and linker-discardable;
no live graphics, performance, hardware-fit or full-parity claim is made.

## Upper-VRAM barrier arena and target upload (2026-10-22)

Inspection of installed SGDK vdp.c64x32 setup found its documented free region
F700..FFFF, above SATF400. Reserve F800..FE00(48 tiles,1984..2031), preserving
font and every existing art bank. This corrects the earlier assumption that
only gaps below the font could be used. No VDP table addresses are changed.

The original348-record source data and actual rotating20-slot loader produce
84 initial/post-load pool snapshots, maximum12 kind2 barriers. A deterministic
conflict graph gives fixed hardware slots for source pool indices:
0,1,10,11,2,3,4,5,6,7,8,9,0,1,10,11,2,3,4,5.
All simultaneous source-pool kind2 slots have distinct hardware slots. Allocation
metadata is generated with the art. This establishes source-pool exclusivity;
retired map references during scrolling still require integration verification.

Added immutable contiguous128-byte pose records(180 poses,23040 bytes), trading
ROM space for a single DMA operation per changed barrier pose, possibly split
at128KiB. The original logical dictionary remains research/reconstruction data
and can be linker-discarded when unused. arcade_barrier_upload checks the full
64x32 table layout, inputs and queue capacity, reserves caller's later work,
uses immutable ROM DMA sources and publishes cache residency only after queue
acceptance. Callers must reset on cold loading and flush before reuse.

Actual68000/VDP standalone test passes180 poses through all12 slots and reads
back11520 words. Cached repeat requests add no transfer, reset forces reload,
invalid slots/poses/layout/reservations reject, and sentinelsF7FE/FE00 remain
intact. Test ROM afca8b9e1eeef68350c2c0bbdbc95f676a6f30747f74fe0abc6c6a826198caa1.
Compiled contiguous poses also exactly match all logical pixel records;46080
independent source pixel comparisons still pass. Production build succeeds.
Live background-map publication, retired references and heavy-scene timing
remain pending; no full-parity or60FPS claim. Headless test exited normally.

## Live barrier world/render binding (2026-10-22)

The shared food/world pool now advances kind2 objects with per-slot native
cursors. Production passes the same entity source clock used by the collision
owner. Pending poses and column masks survive upload refusal, merge partial
column writes, and reset when source slots recycle. Full flashes rewrite the
four nametable entries with a cached immutable four-tile arena. Rendering
preflights the complete pose plus map writes and reserves later frame work;
map DMA sources are immutable constants, never temporary stack buffers.

The fixed arena sharing is also checked against successive world positions.
All30 same-hardware-slot successor pairs are separated by at least1040 pixels.
The required separation is528: new art starts at newX-256, while both old map
columns are overwritten by oldX+16+256 in the64-column ring. Thus the normal
monotonic course retires old references before slot reuse. Generator asserts
this property for changed source maps. Campaign reset clears residency and
world/pending state; abnormal arbitrary camera jumps are outside this proof.

Standalone actual68000/VDP test traverses the production world owner through
camera3800 and checks the first barrier's four nametable indices plus64 pixel
words for its flash pose. Existing conversion/carrier tests in the same ROM
pass. Test ROM422770e27d03b824f0e1e3bbf805a558aae76fb97571167803f54eeed17b2fb0.
Food prefilter equivalence passes80000 updates/84 contacts, with new barrier
state included and barrier DMA deliberately rejected by that host fixture;
target tests cover actual transfers. Food-specific target regression passes
12 nametable words/3 batches. Test link inputs now include the barrier owner.

Barrier player/shot helpers are not yet connected, and scrolling stops remain
pending. This binding makes barrier appearance/flash live; it does not establish
natural destruction, section completion, whole-game parity or60FPS.
Production ROM SHA 2e039e3132bd9fb50134bb3274aacd67dfc8b07150e7e828e1813d31bfab7652.

Pre-flash-dirty optimization full42000-refresh runs still FAIL strict60FPS:1P3784 missed presentations,worstwindow29,peak170.996%;2P6612 misses,worstwindow29,peak176.465%;neither crossed a ribbon. Reports archived under barrier-live-performance (ROM2e039e3132bd9fb50134bb3274aacd67dfc8b07150e7e828e1813d31bfab7652). Source flash attributes repeat four updates; native now retains pending writes but avoids requeueing unchanged map data. This optimization needs its own performance run before any speedup claim.

Post-optimization target regression passes, including zero queued work for the unchanged flash tick; test ROMd6862ead6b84d6aebc9300e007f4584e1eec0ee4ebc0202232fe2a39468282ba. Food prefilter equivalence passes80000 updates again. All test processes exited.

## Live barrier shot binding (2026-10-22)

arcade_food_world_shot now exposes the shared world pool/cursors to the verified
barrier scan and returns source event042F awards. Static source4D84 selector2F
points at4EAA, consuming four BCD bytes at4EA6:00001000 (1000 points/event).
The production collision pass visits both stable shots of the selected player
after food contact at E2E3/E337, then the existing later shared-K/large scans.
It propagates active/state/phase back to native shot records so subsequent
handlers see the retired shot. No optional shot caps or object deletion shortcuts.

Actual68000/VDP live-owner test now fires repeatedly at the loaded first barrier:
three hits,2000 total points, full damage animation and final shot pass-through.
The initial expectation of1000 was rejected: the original animation cursor+4
observations show phase4 lookahead on the second and third hits, each emitting
1000. Corrected test agrees with both the source shot contract and lifecycle.
Standalone ROM9d5868110c42d40435cdac3737a1c89a757812af5e88fd3555ecf0cb8ad7078e.

New production entity-loop integration passes32 updates/192 checks covering
alternating clock0..7,1P/2P eligibility, game-over gate, shot retirement, damage
phase and exact score owner. Source-pickup dispatcher32/224 and conversion
pulse2/16 regressions also pass. Production ROM
074a6bc5ce24dea3f422298c487bea6d1a8e79bbbb850a26302f42e16ad47927.
Player blocking/life entry and stop/stage owners are still unbound. Overall
collision order still contains provisional enemy-family paths; these tests
are scoped contracts, not whole-game parity or60FPS evidence.

Shared-K shot ordering regression passes200 updates/1600 checks on the same production ROM. All test processes exited; no emulator window opened.

## Live player/barrier blocking (2026-10-22)

The selected player's food-contact owner now invokes the verified barrier scan
on the same shared world pool and entry X/Y. Food collection does not alter
coordinates, so its separate pass retains the source snapshot used by E569.
Pushes update both the native movement record and displayed player position.
The crush result raises the existing deadReq; game_update invokes the native
kind1 life owner after the complete collision pass, preserving current-pass
pickup checks rather than prematurely changing eligibility mid-dispatch.

New production entity-loop fixture passes64 updates/576 checks: clock0..7,
1P/2P eligibility, game-over gating, centered push and left-edge crush, correct
player ownership, pending death request, and source movement state/phase reset.
Pickup32/224 and barrier-shot32/192 integration regressions pass. Existing
source death-state injection through full game_update passes9368 cases.
Production ROM8eb5da625096fc8891346c8edeb0d053622e2122e021d8e08c59771888623324.

These establish linked blocking/request behavior and separate death-machine
regression; controller-driven crush/reentry across a complete section remains
unverified. Scroll stops and their bonus/state2 ending remain unbound.
All test processes exited. No full-game parity or60FPS claim.

## Section transition contract (2026-10-22)

Recovered native A730 column-boundary owner and all26 duration/stop/timer
triples from fixed MC6809 descriptors6485 (programSHA
09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1).
Generator checks this exact revision. Native owner preserves byte decrement
wrap, boundary column reset, difficulty saturation at28, speed low byte,
previous descriptor stop/timer handoff, checkpoint and26-to0 index wrap.
Its return signals original54E7 pool load and background/collision refresh;
those effects remain explicit caller responsibilities.

Independent original-CPU fixture ran2496 cases covering all26 indices,
remaining0/1/2/255, columns0/1/31/32/64/255 anddifficulty0/27/28/255.
Host comparison passes37440 fields, including untouched values on early
return and original loader pointer progression. TraceSHA
162d3f4f5b0d1851c3463fbdd1d0c693c3bddb17669352ba250fbf977469331c.
Actual68000 cartridge also passes all2496 source observations; test ROM
38a1de2c419d8d20ce2fee530531c05ea270e5f2bf0c71862ddec2903a446337.
Resource addresses are represented by descriptor index; native gameplay
contains no source CPU interpreter or source address dereferences.

Initial900B setup subtracts one preloaded screen, so first stop follows
15 screen boundaries (camera3840), one screen before the visible descriptor
boundary4096. Startup/reentry paths need their own verified owner; do not
bind the visible segment event directly. In particular9021 uses BLS, unlike
A74F BCS, so arbitrary injected startup indices must not silently inherit
normal transition wrap semantics. New section component is not yet bound:
kind10 reward lifecycle, stop consumers, ending state2 and live integration
remain pending. These tests do not establish60FPS or full-game parity.
All source/target tests headless and exited normally.

## Section-clear kind10 pickup lifecycle (2026-10-22)

Added arcade_variant_pickup_step with explicit spawner-owned grade0..6;
ArcadePowerup ABI stays16bytes. Shared B19A lifetime preserves initialization,
32-tick appearance, collection score duration, unsigned camera-edge retirement
and exit latch. B213 selects seven exact score codes and C4/C3 player palettes,
resets animation timer to127, and consumes its terminal score record. Native
cursor22..35 denotes the consumed record; original resulting pointer words
are never dereferenced again in this state. Out-of-contract grade/owner values
are rejected, not silently mapped to grade0. Grade7 award lookup remains a
separate routine boundary case, unreachable from normal saturated A7D0 grade.

Generator extracts B327 codes and asserts both player records and existing
exact score-art coverage. No additional tiles/VRAM are needed:33cells/132tiles.
Existing independent raw-art test passes8448pixel comparisons. Ordinary
kinds0..9 lifecycle regression still passes30720updates/430080fields.

New original-CPU continuous trace covers42trajectories/21504updates: seven
grades, both owners, wrapped moving camera, stationary camera, and scripted
collection. Native host comparison passes301056fields plus untouched source
bytes and absence of carrier requests. TraceSHA
33792929dca64e93cd01c3c064047f49128899131068e7d403c57d934737ef81.
An initial capture exited incomplete and was rejected by the exact count gate;
complete rerun reported VARIANT_TRACE21504 and exited normally.
Actual68000 cartridge matches all21504 observed before/after updates;
test ROM6d2ef388444ecf640591cdd495ae1ada92f068bc1ec8790c00578c1a334f70fa.
Production build succeeds. Tests ran headlessly and terminated.

Live section-stop reward creation/contact binding is still pending. These
component comparisons do not prove controller-driven section completion,
full-game arcade equivalence, or60FPS. Next bind grade to the persistent0708
pickup owner, then connect verified stop/section owners and their consumers.

## Live section bonus owner and fresh performance (2026-10-22)

arcade_food_bonus_section applies exactly A7D0 request writes: descriptor
active1/phase0/placement104 and persistent pickup kind10 plus separate grade.
It preserves existing actor fields and unrelated descriptor fields. Live bonus
step/contact now dispatch kind10 through verified grade-aware lifecycle and
award routines; ordinary kinds retain their existing path. Reset clears grade.
Score rendering reuses the existing exact art, with player-specific palettes.

Actual68000 owner fixture covers all7grades/bothowners, request preservation,
blocked/capacity gates, birth, collection award/event/combat, duplicate rejection,
score animation/SAT and retirement. Initial test omitted event type/owner bits;
corrected expectation includes original0400 andowner<<7, consistent with the
independently verified reward contract. Ordinary food-bonus target regression
passes12checks. Production pickup dispatcher passes32updates/224checks on ROM
d14fd608b8ebfb01ab0c835ab5199675dbf24dd1f218bc666f90cb2ae33cd2f7.
Stop caller still pending: this wires persistent pickup ownership but does not
yet make natural section completion issue the request.

Fresh42000-refresh Genesis Plus GX workloads on that ROM still FAIL60FPS:
1P2923misses,worst60-refreshwindow38,peak130.957%;2P7583misses,worst29,
peak152.832%. Neither traversed a full ribbon. Archived complete reports in
out/parity-audit/section-bonus-performance. This is current-build evidence,
not an isolated optimization comparison: intervening collision changes also
change the workload. Source logic/workload remains incomplete.

Expanded owner fixture passes177checks, additionally alternating ordinary and
section pickups through the shared eight-collection counter and verifying
conversion request creation only on collection8. Target ROM
7a42a404e16d9f434f4d6a3d65902dad845084ea615f483b9997e652ec9769dc.
All emulator/core test processes exited; no fullscreen window was opened.

## Stopped-player prerequisite (2026-10-22)

Before enabling the section owner, inspection found that the native player API
had no stopped input: it always executed A9C5 idle drift/animation. Original
A9C5 first tests009D and skips both when nonzero. Added explicit
arcade_player_step_stopped with a stopped parameter and retained running-scroll
wrappers for existing callers. Movement, clamp and fire-pose ordering remain
unchanged; only the source drift/attached-player drift/animation branch is gated.
This is a prerequisite, not yet the live camera-stop binding.

Independent source traces force009D before each complete player update, using
controller inputs and original collision maps; A689 can clear it later during
the same update. Host continuous comparisons pass1280updates each for1P and2P:
1P10240motion/5120animation checks,2P20480motion/10240animation checks.
These stopped trajectories do not exercise shoulder riding. Running2P regression
passes1280updates with228shoulder-riding frames. Actual68000 cartridge replays
all1280stopped2P before/after observations successfully; ROM
7d444c6b4f1c3f31df292c70872e663dedcc8b226b3672969cb59a084e4d32e1.
Production build succeeds. All headless test processes exited normally.

Remaining integration must propagate section state into this API, death/reentry,
enemy world and bonus spawn blocking. Source A787 does not clear008B, unlike
A6C6, so the stopped column signal must follow source semantics rather than
being unconditionally zeroed. Ending state2 remains separate/unimplemented;
normal section transitions must not be tied to visible scenery seams.

## Live barrier-stop integration (2026-10-22)

Production game_arcade_scroll_tick now tracks A730 lookahead sections independently
of visible scenery. Cold descriptor0 starts with4remaining preloaded screens;
column signal increments the source column counter before the boundary call.
Difficulty/combat high-byte changes occur at that source transition; normal
visible-segment bookkeeping no longer increments them a second time. Survival
keeps its separate behavior. State1 calls A787 against the actual shared world
pool, preserves008B during hold/release, and hands a clear reward to the persistent
bonus owner. Timeout releases without reward/grade increment.

Players consume entry stop state, including A9C5 drift suppression and death/
reentry stopped inputs. Enemies and special-spawn blocking consume the resulting
state after scrolling, matching update order. Explicit barrier-stop getter gates
onlystate1; state2 ending remains provisional, not an implemented arcade ending.
The0F48 release visual sequence A893 is also still pending. Do not interpret
this milestone as campaign/ending parity or all enemy-family stop correctness.

Production68000 scroll-owner fixture passes6979updates/13970checks: advances
from camera0 to first stop3840, descriptor4/timer20/difficulty4, holds with a
real world-pool barrier, retains advance signal, requests kind10 clear reward,
saturates via separately verified gate, releases on timeout without a reward,
and resumes. Request/destruction reachability remains distinct from this
fixture (barrier active state is seeded/cleared by fixture).
ROMfcd5d593fd4bd2e20852ea9761b58692712c3f5057bb24b99e8e908959a086f8.
Linked player-animation regression passes1280updates/30720fields; fullgame
injected death regression passes9368cases; pickup dispatcher32/224 passes;
world prefilter equivalence80000updates/84contacts passes.

Controller-driven Genesis Plus GX42000-refresh workloads now independently
reach camera3840 at simulationtick6972 in BOTH1P/2P, hold there through8194,
then resume beyond it. This verifies natural stop/timeout/resume reachability,
not successful controller-driven barrier-clear reward.1P maxcamera18533,
2P16034. Neither reached ending/campaign wrap.
Strict60FPS still FAILS:1P3427misses,worstwindow36,peak131.543%;2P9180misses,
worst29,peak148.486%. Full reports and stop-observations.json archived under
out/parity-audit/live-stop-performance. Changed timing changes workloads;
these are current-build measurements, not an isolated performance comparison.
All emulator/core runs were headless and terminated normally.

## Current-build CPU profile and barrier-shot scan optimization (2026-10-22)

Established10000-update2P fixed-simulation-input workload on live-stop ROM
fcd5d593fd4bd2e20852ea9761b58692712c3f5057bb24b99e8e908959a086f8.
Snapshot coverage now includes section state/grade, bonus variant, barrier
cursors/pending masks/poses and fruit carrier:3319canonical/raw state bytes per
update. Baseline self-repeat matches all10000updates. Read-tap timing identifies
entity_update_all mean6.427ms/peak15.219ms (inclusive); actor block mean3.015ms,
collision block2.932ms,scheduler0.479ms. World-shot mean0.235732ms per workload
update over5069calls. These include interrupts; do not sum inclusive timings.

Optimized world-shot owner geometrically filters candidate barriers before
building cursor lookahead. Empty-hit scans now return without the second pool
walk. On hits, the original ordered scan still performs all mutations/events;
X/Y and pool membership are unchanged during prefilter, so only initialized
next[] entries can be consumed. Multiple overlaps still retire the shot once
and process every matching barrier, matching the original contract.

Independent prior-implementation differential covers20000fixtures/2960000bytes
with coordinate wrap, overlap, active/kind gates and cursor boundaries. Production
shot integration32updates/192checks passes; live target destruction/VRAM test
passes; food-world prefilter regression80000updates passes. Full10000-update
fixed workload matches all3319bytes/update against baseline. Candidate ROM
beb74d5bc2adb17124136ffecf4de2ff90040b79dc8fc2b1123865a703d44fc0.
Several MAME captures exited incomplete; exact count gates rejected them, and
missing captures were rerun with the same generated inputs. No incomplete
capture is used as passing evidence. Explicit1000emulated-second bounds added
to these profiling/comparison commands; fixtures terminate sooner on completion.

Matched-prefix measurement completed after rejecting incomplete captures.
Profiler now accepts explicit build/output directories and a bounded prefix
of the already verified10000-update workload. Serial complete1000-update runs
(discarding warmup update0) observe579world-shot calls on both ROMs. Mean
world-shot cost per update falls0.243756682ms to0.164267342ms (32.61%); total
entity mean8.846176ms to8.760304ms. This prefix has no active barriers and proves
the empty-hit scan saving only; hit-heavy costs remain unmeasured. No broad
speedup claim from this localized result. Full10000-update state equivalence
and separate overlapping-hit differential evidence remain valid.

Candidate full42000-refresh Genesis Plus GX runs still FAIL strict60FPS:
1P3439misses,worstwindow38,peak127.148%;2P8402misses,worst29,peak156.396%.
Neither crosses a full ribbon. Reports and matched-prefix measurement archived
under out/parity-audit/shot-prefilter-performance. Refresh-timed routes differ
from baseline, so missed-frame differences are not isolated CPU evidence.
No test process remains live. Source/ending/release visual parity, broader
collision optimization and hardware validation remain incomplete.

## G/H movement width measurement (2026-10-22)

Expanded profiler supports explicit direct-call symbol lists. Current-build
opening actor profile shows G/H step mean1.197489ms/update over5465calls in999
measured updates; Iactor0.354119ms, all entity_update_one3.090621ms. Inlined or
indirect routines report observed_direct_calls=false, not evidence of zero cost.

Native G/H move formerly promoted fractionalX+dx to32bits. Extracted dx is
-384..384 and fractionalX0..255, so sum fits signed16bits(-384..639).
Narrowed only this sum; generator now asserts the velocity bound. All19200
original-CPU cases pass actual68000 G/H/weapon verification (1656776checks),
including independent floor arguments/results. Matched1000-update2P state
comparison passes3319bytes/update against saved preceding ROM.
Candidate d2c19b0065c705e8f8af7f63d1f4f2f9f4d9e255d7d791a1416505beb9e5eeed.

Complete matched timing: G/H mean1.197489→1.191978ms/update, peak1.994406→
1.984237ms; entity-update mean8.760304→8.752747ms. This is a SMALL saving
(~5.5microseconds/update in G/H), not a meaningful step to60FPS by itself.
Retained because semantics/target checks pass and measured cost improves,
without extraRAM/art changes. Reports under foe-move-before/after and
foe-move-comparison.json. No new whole-game60FPS claim; larger update/collision
and presentation costs still need structural work. All tests exited headlessly.

## Release animation source contract (2026-10-22)

Added arcade_release.c/h and ROM-extracted release patterns/destinations via
gen_arcade_release.py. A893 initializes eight poses without drawing on its
initial update, writes on clock modulo4 ==1, and retires after emitting pose7.
Restart writes active/phase only, matching A787; phase0 resets frame/count.
verify_release.py passes102400 offline updates across256 starting clock values
and five restart schedules, including wrap. It checks inspected source bytes
and all extracted data against the fixed source ROM hash. This is an offline
contract check, not an independently executed arcade or68000 trace.

The target build succeeds. The unused component is linker-eliminated, so the
production ROM remains d2c19b0065c705e8f8af7f63d1f4f2f9f4d9e255d7d791a1416505beb9e5eeed.
No live rendering change is claimed. Next: prove source ring addresses map to
the native scrolling plane, allocate exact art, bind after scroll/before world
updates, and validate the visible sequence and deferred-transfer lifetime.
Neither MAME nor RetroArch was launched for this work.

## Release background bound to production (2026-10-22)

The A893 release owner now starts on A787 release (clear and timeout), steps
after game_arcade_scroll_tick and before world actors, and publishes before
world-object map writes. Cold initialization resets it and loads14 additional
exact tiles at FE00..FFBF; eight poses reuse other pixels from the immutable
world dictionary. Barrier art remains F800..FDFF. No per-pose graphics upload.

Source A922 destinations are columns30/31 at rows28/27,24/23,20/19,16/15,12/11,
8/7. The renderer selects the visible32-column source-ring instance, maps it
into the native64-column plane, and retains the physical column so a pending
write survives ribbon rebasing. A whole24-word pose is admitted before queuing,
using immutable ROM sources; failed admission retains the latest pending pose.
Natural stops are screen-aligned; runtime comparison must still confirm source
and native ring alignment in real play, write priority and final-pose lifetime.

verify_release_background.py passes159744 map-word checks across64 plane
phases,13 camera positions and8 poses, plus32 exact decoded pixel cells.
It tests cold reset, queue-capacity and byte-budget retry, final retirement,
and queued-source stability across a subsequent pose. These host tests use
a bounded fake DMA implementation, not actual VDP execution. The earlier
102400 timing updates also pass. Production build succeeds and includes the
new owner, renderer and pixels. ROM SHA256:
bcda058e530a639a16af76b5d2bc1767a7adc725459f6652b75fd4d945e85b3b.

No emulator was launched. Actual target/visual comparison and new frame-budget
measurements remain pending; this is not full arcade parity or60FPS proof.

## Final-section gate and results flow extraction (2026-10-22)

Added arcade_finish.c/h plus generated A81F/A827 patterns. A7F0 calls A807
first: clock modulo16==0 writes the2x2 tile at10F4; clock bit4 selects
4C attributes when set, B8 when clear. Then timer009F decrements only when
clock modulo64==0. A zero result clearsA5, requests top-level08=4 and
clears0B/0E. Timer input zero wraps to255; expiry still writes the flash.
The native helper emits typed write/pattern/results events, without introducing
a scroll release or barrier-clear award. It is not yet called by game.c.

verify_finish.py passes65536 isolated timer/clock cases and256 complete
15-count countdowns, with source instruction bytes guarded and extracted art
bytes compared against the fixed ROM. These offline checks are not independent
source execution or target parity. The native build succeeds; unused components
are stripped, so production behavior is unchanged by this addition.

The extracted source-contract.json records the three results substates at9801:
9807 settles by continuing gameplay until the next clock&63==0;9835 waits for
source screen-command completion and loads command011A;9861 dispatches seven
presentation phases9877,9898,98DF,992C,9989,99E9,9A18. Those initialize the
course-number display, eligible P1/P2 portraits and optionalA5 marker, three
animated objects, wait five64-clock intervals, issue result commands, then
increment7E, clear section67 and65/66, call645C and return08 to1.
A7F0 clearsA5 before this path, so ordinary course completion has no optional
marker. This is not a direct seamless-wrap transition. The current production
state2 path remains provisional until the entire results/restart owner is bound.

No MAME or RetroArch process was launched during this extraction.

## Native results sequencer (2026-10-22)

Added arcade_results.c/h for97F9..9A2B. Typed events separate gameplay settle,
screen preparation/ready acknowledgement, board setup, eligible player setup,
decoration setup, animation, awards and restart. Source byte widths are retained:
course255 wraps the displayed next-course byte to0, while10..255 clamp to
packed display10; restart increments the course byte. P2 selection uses bit0
of the two-player flag. Marker setup requires exact owner1/2; award selection
maps any nonzero/non1 marker to the second marker as99E9 does.

verify_results.py passes87324 host updates covering256 clock phases,180 player/
marker combinations,256 course bytes, screen-busy waits on both sides of
preparation, the five64-clock presentation intervals and single restart event.
Build passes. No live owner calls this component yet; renderer assets, exact
score-effect consumers and preserved-state campaign restart remain necessary.
Production ROM remains bcda058e530a639a16af76b5d2bc1767a7adc725459f6652b75fd4d945e85b3b.

Extended finish source-contract extraction with three decoration records9962:
world-independent positions(144,32),(80,32),(112,32), animation roots9971/9979/
9981. Each loop has timers10 and16, hence11 and17 displayed updates under9617;
they must not be normalized to equal frame lengths. Restart645C queues sound12.
No MAME or RetroArch was launched. Target and live source parity are unproven.

## Results points and label assets (2026-10-22)

ArcadeResultsEvent now carries two32-bit point awards. Source selector00 reads
the dynamic BCD bytes DC..DF via4D84 pointer00E0: the displayed course bonus
is10000 per first course, through90000, then100000 cap. The byte increment
wrap atcourse255 yields0, preserved explicitly. Selector34 points to00030000
for the optional marker award. Existing source checks guard both pointers/data.
Eligibility masks remain independently evaluated; a marker award is not gated
on its player's ordinary bonus eligibility. Production consumers must apply
these amounts through the score/life-threshold owner and honor attract mode.
verify_results.py now passes87580 updates including all256 course awards and
180 player/marker combinations. Actual score application remains unbound.

gen_arcade_results_art.py decodes original47DA label streams for commands021A,
0220..0223. Generated native asset:17 distinct exact tiles,54 sparse map cells,
five groups. Independent expected strings/locations check CONGRATULATION,
BONUS/PTS and AND/30000 PTS for both players, including source2C attributes.
The scene owns its eventual tile base; no gameplay arena is overwritten yet.
Player/decor sprites, dynamic digits, scene load/restore and live transition
remain outstanding. Native build passes; unused components are stripped.
No emulator was launched, and full arcade parity/60FPS is still unproven.

## Results decoration assets and native animation (2026-10-22)

Added arcade_results_decor.c/h and generated decoration data. Three source
records at9962 create actors at(144,32),(80,32),(112,32); all six distinct
poses occupy24 native tiles with original code/attribute selectors retained.
992C invokes9617 immediately after initialization, so begin publishes pose0
withtimer10. Subsequent updates decrement until zero, then publish the next
pose:11updates on frame0,17 on frame1,28 per full loop. No guessed equal-rate
animation and no runtime source-CPU interpreter is introduced.

verify_results_decor.py passes3072 host updates, all six exact sprite-cell
pixel comparisons and all three source positions. Reinitialization after
animation is checked. Build passes. These assets are not yet loaded by a
live results renderer, so target/SAT presentation remains unverified.

9898 and98DF set player active/X/Y but do not reset their code, attributes,
animation cursor or timer. 99C5 advances those retained animations through9617,
and likewise the optional marker at0748. A fixed player portrait substitute
would therefore not reproduce the source. The future live owner must retain
these animation states through screen clearing. No emulator was launched.

## Results background composer (2026-10-22)

Added arcade_results_map.c/h. It consumes the typed result events and writes
a caller-owned32x32 map of relative results-art tile indices. CLEAR reproduces
011A: only rows5..30 change, preserving HUD rows0..4 and bottomrow31. BOARD
uses the exact title group. PLAYER adds the appropriate bonus label, number
at116D/126D and optional marker labels. Number formatting follows4EBE: eight
BCD nibbles with leading zeros blanked except the last digit. Valid packed
course values0..9/10 produce0..90000/100000.

Results art now includes ten exact digit tiles plus the source leading-blank
and scene-clear variants, deduplicated with labels:25 unique tiles total.
verify_results_map.py compares135168 output cells in132 cases against original
character pixels and independently specified source label positions. It checks
all valid numbers, both player masks and marker owners, untouched HUD cells,
and the precise clear interval. Build passes. Map publication/VRAM ownership,
player animation retention, and full campaign results/restart binding still
need integration; no runtime or60FPS claim follows from these offline checks.
No emulator was launched.

## Results native display owner (2026-10-22)

Added arcade_results_display.c/h. Cold begin loads25 background tiles and24
decor tiles into the existing bonus arena136..184; cold restore reloads the
original132 bonus tiles. Compile-time check bounds the shared arena. Gameplay
bonus rendering must be suspended during ownership. Calls require disabled
display and flushed prior DMA; the eventual game-flow caller must enforce this.
The caller also owns zero scroll, player/marker SAT retention and full map
restoration on course restart; this module alone does not change those states.

A private32x32 map and dirty-row mask drive32-word rows into a64-column BGA.
Only changed label/number rows are marked after initialization. SGDK
DMA_copyAndQueueDma copies stack-row words, preserving deferred-source lifetime
across subsequent events. Failed admission or copy retains the dirty row.
Decor SAT uses original FFB3 Y transform and the existing arcade-cell helper,
appending three cells after players/marker and refusing insufficient capacity
before any write. Caller terminates SAT. Restore clears pending map/decor work.

verify_results_display.py passes a host test with a two-entry copied-DMA queue:
all2048 plane cells, load/restore bounds, byte-budget/copy failure retry, exact
64-column stride, HUD/right-half preservation, dirty row selection, immutable
queued data across map mutation, and decoration SAT coordinates/capacity.
Native build succeeds. Source/target runtime, scroll ownership and full game
flow integration remain pending. No emulator was launched.

## Separate course entity reset (2026-10-22)

Source9A18 returns08 to1, which enters96A6/8FBC. It does not revisit new-game
8F5B. Existing entity_init calls entity_schedule_init and clears RNG, clock
and conversion progression, so it cannot be used unchanged for course restart.
Added entity_course_restart wrapper which saves/restores arcade_rng seed/sample,
wave_clock and arcade_bonus_next/limit around the existing pool/cache reset.
It is not bound to game flow yet. Other residual actor/controller fields and
source reset ordering still need validation before claiming restart parity.

verify_course_reset.py compiles the production wrapper with a deliberately
destructive entity_init stub.65536 cases prove session retention and one
reset call, including actor clearing through the stub. This does not prove
the underlying full native entity reset matches source96A6. Build succeeds.
Player state, section state, scene restoration and live results binding remain
pending. No emulator was launched.

## Course player/section preparation (2026-10-22)

Added arcade_players_course_begin for the typed motion/animation fields written
by9094. P2 active is cleared before eligibility; out P1 is untouched. Eligible
players get camera+32, source Y104/136, active1/state0/phase0/facing1, idle
script cursor/timer0. Fractions, input history, other motion fields and current
code/attributes are retained. Partner pointers, source kind/owner fields outside
these structs, shot budgets and live lifecycle ownership remain caller work.
verify_player_course.py passes252 poisoned-state cases across eligibility,
noncanonical nonzero flags and camera wrap. This is a host source-contract
check, not a source trace or live restart test.

Added arcade_section_course_begin restricted to descriptor0 after9A18. It
clears checkpoint/index/column/stop, loads four remaining screens, copies old
lookahead timer before replacing it, and writes difficulty to speed's high byte
while preserving the low byte. verify_course_section.py passes65536 combinations.
Cold game_init now uses this shared section initializer. The full results
transition still is not connected; this does not claim a working course ending.
No emulator was launched.

## Ending/results now connected to game flow (2026-10-22)

State2 now holds the camera and feeds arcade_finish_step instead of continuing
normal scroll. Both nonzero section states reach stopped consumers. Timer expiry
starts ArcadeResults with the current completed-course byte. game_results_update
lets normal gameplay run during the settle interval, then owns the updates,
advances the clock explicitly, waits for pending results background transfers,
and dispatches the native display. Retained normal/life animation states advance
through newly exposed existing animation helpers. Results rendering appends
retained player cells before the three decorations and owns zero horizontal scroll.

Eligible awards update each player's score through the existing extra-life
threshold check; attract mode suppresses points. Restart restores bonus VRAM,
refills the level map, resets section/scroll/player state, resets actor/food owners
and retains scores/lives and entity RNG/clock/conversion progression. Course byte
comes from the results sequencer, and sound12 is queued. Ordinary game-over
countdown does not abort an active results sequence. Home Survival bypasses this.

verify_results_integration.py passes2048 cases using the actual dispatcher and
sequencer but stubbed display, animation, extra-life and restart effects. Checks
cover clock handoff, busy waits, awards/attract suppression and one restart.
Display, course-reset and finish-gate offline regressions pass. Build succeeds.
This is production integration, not proof of full runtime/source parity.

Known unfinished parts: A807 end-tile flash is not rendered yet; source multi-
update course initialization timing is collapsed into one native restart;
source residual actor-field preservation vs native pool reset needs comparison;
source player shot-counter/partner bookkeeping and out-player behavior need
a full restart audit; results HUD point publication and controller joins during
the sequence require runtime checks. Full target visual comparison, natural
1P/2P course completion and frame-budget measurements remain pending. No
emulator was launched during this integration.

## End-marker flash bound (2026-10-22)

A807 flash events now publish both source poses at10F4/10D4/10F5/10D5.
Eight exact baked tiles occupyF700..F7FF, above the layout's SAT and directly
below the F800 barrier arena, with compile-time overlap protection. Cold game
and course startup load the art and clear pending writes. Each flash queues
four immutable map words; admission failure retains the latest pending pose.

verify_finish_background.py passes5120 map-word checks across64 plane phases,
ten camera positions and both poses, plus eight source pixel-cell comparisons.
It checks admission retry, reset and dirty retirement. Build succeeds with ROM
555bfd3130edf2448c9cf2c484807e9a16736b25b4fc948f9c2329df467b5e53.
A70000-refresh-per-player Genesis Plus GX windowless workload was then started
for runtime/pace evidence; results are to be recorded after completion. This
uses the library test harness, not the RetroArch application or MAME.

## Windowless ending runtime and performance (2026-10-22)

Genesis Plus GX library harness completed70000 sampled refreshes in each mode
on ROM555bfd3130edf2448c9cf2c484807e9a16736b25b4fc948f9c2329df467b5e53.
Both modes reached camera22016, held there, reset to0, and resumed scrolling.
This establishes controller-driven course-reset reachability with lives refilled,
not full source state parity or visual fidelity. Detailed reset timing and raw
CSV/logs are archived under out/parity-audit/ending-runtime.

The60FPS assertion FAILED in both modes:1P5557 missed presentations out of
69999 intervals, worst60-refresh window34, peak CPU129.10%;2P14667 missed,
worst window29, peak156.88%. These are measured failures, not completion.
The new ending sequence does not solve the existing gameplay CPU bottleneck.
The harness has exited. No MAME/RetroArch frontend or emulator window opened.

## Course restart shot-counter eligibility (2026-10-22)

Source9094 writes parent+$0C=2 at90CF/9108 only after player eligibility
checks. The native entity_init reset both counters unconditionally. Course
restart now accepts the two-player and out flags, preserves counters around
entity_init, then resets only eligible players. Cold new-game reset is unchanged.
The live game caller passes its captured out flags and multiplayer state.

verify_course_reset.py passes65536 cases covering all counter byte values and
player eligibility, together with prior RNG/clock/conversion retention. It checks
the source ROM hash and both immediate counter stores. Its entity_init remains
a destructive stub: this proves the production wrapper, not the complete pool
reset, source initialization timing, or controller-driven restart parity.
No emulator application was opened.

## Course background refill no longer cold-starts players (2026-10-22)

The live course restart called level_restart(FALSE), which reset public player
poses, fractions, fire history and legacy animation before source9094 setup.
Excluded players were never restored. Background refill is now its own helper,
used by both new-game setup and course restart. Only new-game setup applies
the legacy cold poses. Course motion publication/life ownership reset also
requires P2 multiplayer eligibility, matching the source9094 branch.

verify_course_player_binding.py compiles actual game_course_restart, motion
adapters and player/section setup with stubbed hardware/entity owners.2048
cases cover every fractional/history byte, eligibility and player counts; they
assert excluded public player records remain unchanged, eligible poses and
fractions/history are preserved correctly, and scores/lives and current art
survive. Existing252 player-course and2048 results-dispatch cases also pass.
Native ROM builds successfully. This does not prove full actor-pool reset,
source multi-update startup timing or visual/runtime parity. No emulator opened.

## Course actor residual-state retention (2026-10-22)

Source96C0..9789 dispatches9793 for each pool.9793 clears only the active
byte; it does not zero full actor records. entity_reset and
entity_schedule_reset now distinguish cold startup from course reset. Course
reset clears active flags in recovered I/large/K/G/H/weapon records without
wiping fractions, target, animation or other residual fields; cold clearing
remains separate. Generic player-shot records are no longer memset on course
restart either; the entity active sweep still deactivates them.

verify_course_actor_retention.py guards source ROM and9793 bytes, compares all
bytes of27 recovered actor records over256 poison values, and checks cold
initialization. The actual reset function runs with scheduler/renderer stubs.
The65536-case session/shot-counter reset check passes and the ROM builds.
This is not full restart parity: wave-controller residual fields, reset phase
timing and newly spawned actor behavior across real course transitions remain
to audit. No emulator application opened.

## Selective course wave-controller reset (2026-10-22)

The previous course path called arcade_waves_reset(255), erasing producer
state/mask/cursor, delay and every controller. Source9048 clears0628 to0,
904F..9065 clears0638..0643, and96C0..9775 clears selected controller active
bytes. Producer062A and other fields survive;06C8/06E8 are not cleared here.
Added arcade_waves_course_begin matching those writes, used only for course
reset. The first controller retains its parameter; others retain all fields
except the explicitly cleared active bytes. Class-I cursor/remaining also
survive; its duplicated control is synchronized from the common wave record.

verify_course_waves.py compares256 populated native states to a source-address
write model and guards ROM revision plus every relevant CLR and the12-byte
clear sequence. Actor-retention regression now checks class-I cursor/remaining
and synchronization too. These are offline contracts, not proof of natural
repeated-course behavior or the original multi-update reset timing.

## Course-retention changes: windowless runtime check (2026-10-22)

ROM8f981dc10d7f3b0ac7d524a53c7c0dd202b599bb60913430e095cbc054ab91d9
completed70000 sampled refreshes in each Genesis Plus GX library mode.
Both held camera22016, reset to0 and resumed scrolling to218 within400
refreshes.1P reset at54800 (hold1333refreshes/1309ticks);2P at63968
(hold1312refreshes/1309ticks). This is controller-driven reachability with
lives refilled, not source-state parity or unassisted playthrough proof.

60FPS checks still FAIL:1P5665 missed intervals of69999, worst60-refresh
window31 presentations, peak132.42% CPU;2P14920 missed, worst29, peak151.12%.
These are not a controlled speed comparison with the prior ROM because inputs
are refresh-keyed and missed frames change the simulation/input alignment.

New report_course_runtime.py requires completed1P/2P reports matching the ROM,
ending-specific reset and resumed scrolling, then archives the ROM, symbols,
raw logs, performance and reset evidence. Current archive is
out/parity-audit/course-retention-runtime. The windowless harness exited;
no MAME or RetroArch frontend was opened.

## Simulation-index windowless benchmark (2026-10-22)

verify_second_core.py now accepts --ticks N. This sets gameplay inputs from
the native update counter relative to the detected gameplay start, and stops
after N observed updates. The original --frames mode retains refresh-timed
input. CSV adds simulation_elapsed and input_step; result metadata identifies
the clock and requested work. A bounded4N+6000 refresh ceiling prevents hangs.

Two independent2P runs of3000 updates on ROM8f981dc10d7f3b0ac7d524a53c7c0dd202b599bb60913430e095cbc054ab91d9
produced byte-identical CSVs:3194 sampled refreshes,194 missed presentation
intervals, worst60-refresh window33, peak113.38% CPU. Thus the60FPS assertion
still fails. Repeatability and input-step lag checks pass; evidence archived
in out/parity-audit/simulation-input-check. This validates same-build
determinism, not equal states across optimized builds. Counter sampling can
observe an in-progress update, so canonical-state comparisons remain required.
No emulator frontend opened and the harness runs exited.

## Food contact prefilter: bounded fixed-work measurement (2026-10-22)

The existing geometry prefilter was tested against the same current source
with only that filter removed.50000 host cases compare11300000 output bytes
including collected objects, counters and pickup requests. On the3000-update
2P simulation-index workload, unfiltered ROM49f0d6acc62478ae90347499f0f0c45c9da9f71753f75a0b3d8b1db209933d60
took3206 sampled refreshes (206missed); filtered ROM8f981dc10d7f3b0ac7d524a53c7c0dd202b599bb60913430e095cbc054ab91d9
took3194 (194missed). Both worst60-refresh windows were33. Peak CPU was
113.92% unfiltered versus113.38% filtered. Camera and actor-count fields
matched at all3000 last-sampled tick views. These are coarse workload checks,
not canonical full-game state parity. No large enemies occur in this prefix.

Emitted68000 code confirms byte/word overlap rejection precedes stack
argument construction and the contact call. Assembly, both ROMs/symbols,
unfiltered log and comparison are archived in food-contact-fixed-work;
optimized repeated logs remain in simulation-input-check. Restored filtered
source and rebuilt byte-identically to the archived optimized ROM. This is a
small measured2P prefix improvement, not sustained60FPS or a universal gain.
No emulator frontend opened.

## Skip inactive large-actor setup (2026-10-22)

entity_update_all now rejects inactive large slots before assembling the four
shared child occupancy flags and calling arcade_large_step. The callee already
returns without effects for inactive actors. Active actor order and per-actor
occupancy refresh remain unchanged, including earlier actors filling child slots.
verify_large_idle_skip.py compares the actual loop with/without the skip using
real large functions over368640 updates, all8active masks and256shared child
occupancy combinations. Actor, child and RNG states match.

Fixed simulation-index target runs compare baseline8f981dc10d7f3b0ac7d524a53c7c0dd202b599bb60913430e095cbc054ab91d9
with candidate7c09103bb035a2778035e42a65ffae9fb0c957e797ed698324e9a755811e746b.
1P3000updates:3166->3139refreshes,166->139misses,worstwindow40unchanged,
peak101.76->99.46%CPU.2P10000updates:10757->10668refreshes,757->668misses,
worstwindow30unchanged,peak129.64->129.69%CPU. The tiny peak variation does
not support a peak-cost improvement claim. This workload includes3large
actors and2Kchildren. Coarse camera/actor counts match all3000/10000 sampled
update views. The shorter2P3000update run also improves194->153misses.

Candidate retained for lower missed presentations in both measured modes.
These checks do not prove full-game state equivalence or sustained60FPS.
Artifacts, assembly and comparison results:out/parity-audit/large-idle-skip.
Candidate ROM/symbols restored after baseline testing; no emulator frontend opened.

## Rejected body-contact prefilter experiment (2026-10-22)

Tried rejecting nonoverlapping H/G/I body contacts before the six-argument
arcade_body_contact call, using equivalent wrapped public-coordinate geometry.
100000 differential cases pass for the actual loop, with real contact geometry
and deterministic stub bonus awards. Coarse camera/actor-count states match
all3000 1P and10000 2P update views.

Candidate4b709455969a3d341e7f8ebc309f0483043b71a9b149f42823b092a609430248
versus retained7c09103bb035a2778035e42a65ffae9fb0c957e797ed698324e9a755811e746b:
1Pmisses139->138 but peak99.46->102.20%;2Pmisses668->716 andpeak129.69->131.84%.
Worst windows unchanged40/30. Reverted the candidate because the duplicated
geometry worsens measured2P performance. Source restored exactly; experiment
test reconstructs the rejected filter for reproducibility. Evidence and both
ROMs/assembly retained under out/parity-audit/body-contact-prefilter.
No emulator frontend opened. Further collision improvements need to remove
work, rather than duplicate the small overlap helper.

## Recovered ordinary enemyMain wave controller (2026-10-22)

Added typed arcade_main_wave_step forEFED..F0C8 and a deterministic extractor
forF0C9's16 pointers/192 Y-subtype pairs. The controller separates phase0
bulk reservation of12 slots from phase1 selection and phase2 one-slot refill
of the16-slot source pool. It preserves the exact ready>=12 bulk gate,
nonzero-ready/exactlive16 refill gate, first-free order, twelve or one RNG
calls, byte-state transitions, camera+248 wrapping and vsync23/clock24
selector difference. Birth events specify only source-written fields; actor
fractions and unrelated fields must remain retained by the future binding.

verify_main_wave.py passes262144 updates/166134 births against an offline
source-derived state model using raw ROM placements independently of generated
tables. ROM hash and key branch instructions are guarded. Not a source
execution trace. Native build succeeds, but this component is NOT live-bound:
ordinary enemyMain still uses the provisional single-birth fallback. Required
next work: recover its actor lifecycle, allocate/source-order the dedicated16
slots without crowding out C/D/E/F, bind true source23 timing and A4 readiness,
and compare original/native trajectories before replacing that fallback.
No emulator was opened.

## Ordinary enemyMain actor recovered (2026-10-22)

Added ArcadeMainActor and ordinary B9BA dispatcher states0..5. Source entry
ORs active2, writes record+$12=5, camera+248, selects the walk phase by
(16-slot)&3, increments live and falls into the delay phase immediately.
Delay compensates141 fractional units only while scrolling; decrements on
clock&31==0. Entry becomes variant+2 only below relativeX246. Normal variants
use stationary/leftward/bouncing paths; bounce phase0 has no movement that
update, with inclusive48/24 distance thresholds. Death countdown/cleanup,
BB31 pending-hit animation and A92E conversion ordering are retained even
when cleanup deactivates the record. State255 bonus handling remains separate.

gen_arcade_main_actor.py extracts28 animation frames, four walk roots, two
death roots, wounded root and five source vectors. verify_main_actor.py
passes32790 offline source-contract cases for init/delay/animation selection,
bounce boundaries and cleanup ordering. Generated poses are checked against
original ROM bytes. Full ordinary trajectories, bonus-state binding, collision
ownership and live native16-slot pool/rendering remain pending. This actor
and the preceding wave controller are not yet used by gameplay. No emulator
opened.

## EnemyMain five-hit combat and bonus state (2026-10-22)

E3CD proves record+$12 initialized5 is HP, not a contact extent. Renamed
the new typed field hp. Added E373 first-slot shot scan with original
enemyY-shotY+9 byte window and X+7 word window. Accepted hits consume the
shot, queue sound7, decrement HP with byte wrap, set the wounded-animation
request atHP1, and only atHP0 enter state5/phase0, store owner, award042B
(BCD00001000), queue sound6 and add combat0033 viaE407.

Added the enemyMain-specific B9E9 bonus path using the existing verified
B291 poses: phase0 kind rotation, phase2 score pose/owner palette, state5
offscreen cleanup and A92E conversion restart. Sourcekind reuses the typed
hit byte, as source record+$03 does. Valid recovered kinds remain0..10.

verify_main_combat.py passes264704 hit cases,65536 geometry cases and5632
bonus cases; ROM revision, HP instruction sequence and score bytes guarded.
Existing32790 actor checks pass after renaming. These are offline contracts,
not independent source execution or target integration. Ordinary enemyMain
still uses the old fallback in gameplay until dedicated pool, full contacts,
bonus collection and rendering are bound. No emulator opened.

## EnemyMain dedicated pool and player contacts (2026-10-22)

Added ArcadeMainPool with16 typed actors, controller state and exact live
counter. Scheduling applies only F03E/F09A birth fields, retains residual
actor data and performs no live increment until BA2B initialization. Actor
updates dispatch normal and bonus paths in fixed slot order. Warm reset only
clears active/live; producer/controller reset remains caller-owned. Cold reset
clears the complete native owner.

Added ECB7/ECE9 first-slot player-contact target and ED18/E838 converted
bonus award effects. Contact scans nonzero active without bit2, including
bit6 bonus, with exact wrapped Y/X+7 windows; collection sets active2,
increments phase, records owner, queues sound5 and adds combat00C0.

verify_main_pool.py checks256 poisoned pool sequences through12 reservations,
actor initialization/live increments and death cleanup, plus512 contact flag
cases and5632 bonus awards. All pass with real native subsystem functions.
Existing main-combat checks pass. Host integration is not gameplay binding:
art residency/rendering, producer synchronization, source23 input and global
collision/update ordering still need to be connected in entity.c.
No emulator opened.

## EnemyMain resident art fitted without expanding VRAM (2026-10-22)

Audit finds23 normal/wounded/death/score poses:5 exact resident animation
cells,18 missing cells (72tiles). Retired captured-scene tiles61..132 are
unused by gameplay apart from the aliases explicitly audited. All18 protected
scene patterns used by food/barrier/entrance maps lie outside this range.
Added gen_arcade_main_art.py, compiled pose lookup and72tile pixel bank.
Game cold startup loads these after the captured scene. Existing animation,
world, bonus and dynamic upload arenas remain in place.

verify_main_art.py passes23552 indexed pixel comparisons across23poses and
all flip attributes, checks protected patterns and cold-load order. This is
lossless relative to the existing Genesis bake, not original arcade color
fidelity. Food/barrier generators now exclude overwritten scene slots; their
generated C outputs remain byte-identical. ROM builds. Live actor rendering
and target VRAM readback remain pending; no emulator opened.

## EnemyMain live gameplay binding (2026-10-22)

Normal-mode entity.c now owns the dedicated16-slot ArcadeMainPool. Source
controller0 is synchronized before the other enemy controllers, replacing the
single-birth THROWER fallback for this family. Updates run before G/H, shots
scan main before the other ordinary families, and player contacts dispatch
main before K/G/H/I. Five-hit damage, owner scores/sounds, conversion pickups,
warm/cold reset and coordinate rebasing are connected. Survival retains its
existing path. Rendering uses stored source code/attribute poses and resident
normal/bonus art, including both score palettes and byte-edge wrap copies.

Confirmed B9CD/B9D0 call BB31 then A92E after the bonus handler too. The bonus
edge transition to state5 now executes the wounded-pose post-step immediately
when the retained hit/kind byte is nonzero, rather than one update late. Source
instruction guards and bonus edge assertions cover this correction.

verify_main_binding.py extracts the production shot adapter with host game/I/O
shims: five-hit kills for both owners and two-shot progression to the next actor.
51200 real-renderer cases cover all28 normal frames/22 bonus frames, ownership,
all active bytes, wrap and capacity guards with address/undefined sanitizers.
Existing actor32790, combat264704/geometry65536/bonus5632, pool256, wave262144,
art23552pixel and reset-wrapper65536 checks pass. Target compile-time checks
prove the22-byte actor stride/activeoffset9 used by windowless instrumentation.

Initial3000-update runs did not reach wave12 (camera3072), so main peak0 was
not evidence of failed spawning. Removed inactive actor calls and unarmed
controller occupancy preparation; their callees return without effects.
Final10000-update controller-driven runs reach main peak16 in both modes,
large peak3 and K peak2/3. ROM4fe6a520124d690f0d0c813e72f8e26dffabbbc6faf5144cd628d79625fb950b:
1P10545sampledrefreshes/545missed,worst60window39,peak111.43%CPU;
2P11272/1272missed,worst30,peak134.72%CPU. Both complete their requested ticks;
the verification command exits1 because the sustained-presentation gate fails.
These now include additional correct enemy workload, not an equivalent before/
after optimization benchmark. Logs/ROM/symbols/source archived under
out/parity-audit/main-binding/runtime. No MAME or RetroArch frontend opened.

Remaining explicit integration limits: main readiness currently adapts emitted
SAT count, not full source FED8 logical24-cell admission; sprite priority still
follows the partially rebuilt renderer. Native VInt counter supplies source23's
independent clock but source refresh rate/boot phase alignment is unproven.
Other provisional families and their update/contact ordering remain pending.
Full trajectory comparisons, conversion/course-reset target coverage, visual
VRAM capture and60FPS are not established by these tests.

## Logical sprite readiness connected (2026-10-22)

entity.c now computes gameplay readiness from actor flags instead of emitted
Genesis SAT count. It counts special pickup/conversion/carrier, eligible players,
main16 and existing ordinary/shot records without viewport culling, ignores
negative active flags and native popup decorations, reserves4 per active large
actor atomically, then counts shared K once per slot (weapon owner first).
Main's numeric readiness and the boolean used by projectiles/special spawns
now derive from this same logical remainder. Timing remains the previous
render-published readiness consumed by subsequent gameplay.

arcade_sprite_remaining is the count-only form of FED8: order among initial
single-cell actors cannot change its remainder. It matches all512 archived
original CPU traces, including P2 suppression and72 large admissions. The
production mapper is extracted into100003 sanitizer-checked host cases with
minimal actor/game types. Tests cover absent P2, reservation flags, K aliasing,
popup exclusion and four-cell reservation. No new source emulator execution.

The initial implementation adds an expensive full entity scan. Moving active
rejection before slot/type checks and traversing with a pointer retains the
same tested count while reducing measured cost. Initial ROM03c713197e0f823a53e9bcd79aaab46147556e509ed70d204a4269b80e6dbcb0:
1P10000updates1078misses/peak117.87%;2P2579misses/peak141.26%.
Retained ROMa81d9c42120f78dff4311d9f3c465e97b7cafe2069db4dab2f20900321b9215c:
1P10809refreshes/809misses,worst60window39,peak114.65%;
2P12079refreshes/2079misses,worst29,peak138.04%.
Both complete10000updates, mainpeak16/largepeak3, Kpeak2/3. Runtime source23
uses refresh count, so timing changes can alter later selected wave sequences;
these runs do not prove identical full trajectories or universal optimization.
The sustained60FPS assertion still fails. Both versions and logs are archived
under out/parity-audit/sprite-readiness-binding/{initial-runtime,runtime}.

This fixes readiness's physical-sprite-count mismatch, not all FED8 integration.
Actual render admission/priority, large renderer's physical remaining adapter,
and source-equivalent populations for provisional families still need work.
The current renderer can draw actors beyond the source24-cell admitted set.
A future unified admission pass should publish both masks and remainder while
avoiding duplicate pool traversal. PAL/timing/boot phase remain unresolved.
No MAME or RetroArch frontend opened; all new runtime checks were windowless.

## Shared logical admission masks and rendering (2026-10-22)

Added arcade_native_admission: maps native entities/main/large/sharedK to
source priority and emits masks plus remaining count in one logical pass.
Special/player singles reserve at most5; shots are next. Ordinary order is
A,main,C,D,E,F,G,H,I,orb; unrecovered legacy types trail those before large/K.
Provisional families remain provisional populations, but existing typed source
families preserve slot order. Negative single-cell active flags are excluded;
large nonzero active reserves4 atomically; sharedK admits only its current owner.

entity.c publishes readiness from this admission result instead of the separate
count-only scan. Normal main/entity/large/K rendering now honors masks, with
large cell construction given its already-reserved4 cells rather than the
remaining physical SAT length. Main's masked renderer retains a full-mask
wrapper for isolated render contracts. The large-upload fallback may reuse its
old SAT only when the requested cell mask is unchanged, preventing an excluded
actor from reappearing through cached sprites. Survival's path is unchanged.

verify_native_admission.py maps all55 ordinary source records to native slots
and checks33792 admission bits across512 archived original FED8 CPU cases,
including72 large admissions, plus a saturated main/G encounter. The original
trace remains independently checked by verify_sprite_budget.py. Production
mapper100003 sanitizer cases and51200 main-render cases pass; the latter now
also checks zero admission masks produce no SAT writes. ROM builds.

Initial ROMd42bbb886e96fb6c5090b76d978b980d812ebfee0edfd0adeebac238254e77e2
completed10000updates with1P2059/2P4403missed presentations andpeak132.47/150.10%.
Early inactive rejection before mask shifts/family lookups retained identical
host contracts; ROMa138b4c478b9e30904aff9f1ec8680447f09fe364ce9561be3e31f2b74a4526c
completes10000updates with1P11569refreshes/1569misses,worst60window31,peak124.90%;
2P13458/3458misses,worst29,peak142.87%. Both reachmain16/large3/K2or3.
The sustained60FPS assertion fails; current fidelity work adds material CPU
cost. Runtime vsync-driven sequence selection means whole-game trajectories
are not identical across differently paced versions, so no universal timing
gain is claimed. Artifacts archived in native-admission/{initial-runtime,runtime}.

Remaining: actual SAT overlap priority still uses the old drawing order
(players then specials/main/general entities then large/K), despite correct
admission priority. Generic actor viewport/wrap, provisional C/D/E/F populations,
source player suppression/entry ownership and full target visual captures
remain outside this proof. A direct ordered admitted-render stream could remove
extra traversals/mask checks while fixing SAT priority. Full60FPS and arcade
refresh/boot-phase alignment remain pending. No emulator frontend opened.

## Ordered admitted ordinary render stream (2026-10-22)

Admission now emits a bounded24-entry single-cell identity stream alongside
its masks/remainder. Main actors use IDs48..63; other single-cell records use
native entity indices. The renderer consumes only admitted identities in order,
calling a one-actor main renderer in place. This removes the separate16-slot
main render scan and normal-mode48-slot draw scan/mask checks, and restores
A/main/C/D/E/F/G/H/I/orb order among the ordinary families. Survival still
iterates its legacy pool. Each main actor can append its byte-wrap copies
without re-entering admission or charging extra logical cells.

verify_native_admission.py now also compares the complete ordered single-cell
identity stream for all512 archived FED8 source cases; count<=24 is checked.
Main renderer51200 cases and production readiness100003 sanitizer cases pass.
Target build succeeds. Source identity stream excludes the three large actors,
which remain a separate four-cell stage; shared K weapon ordering relative to
large/K siblings and special/player priority are NOT yet fixed.

ROMe645832e352d5bc2dbb8ae80be22b627f69f4a88ccfa26b6e4d69e78eab526db
completes10000controller-driven updates in both modes. 1P11969refreshes,
1969misses,worst60window29,peak135.11%;2P13442refreshes,3442misses,worst30,
peak146.83%. Mainpeak16/large3/K2or3. Performance remains below target and
this change does not establish an overall speed improvement (1P is worse
than the previous admission build). Refresh-driven wave selection also
prevents treating these as identical full-game trajectories. Runtime archive:
out/parity-audit/native-admission/ordered-runtime. No frontend opened.

Next renderer work: source special order (carrier,pickup,conversion), then shots,
then players before ordinary actors, and large followed by the shared K slots
in owner order. Existing viewport/pose/palette and provisional family gaps
remain; ordered admission alone is not whole-game parity.

## Source special/shot/player front-chain priority (2026-10-22)

Normal game_render now asks entity_render_all for the complete SAT chain from
slot0. The special emitter visits carrier0708, pickup0728, conversion0748 in
FF3D order. The admitted shot stream follows, then game_render_players appends
players before the first ordinary identity. An empty/shot-only stream still
inserts players before the large stage. Player source active flags suppress
inactive/negative entries; existing life/hurt blink presentation is retained.
Survival continues reserving its player slots before entity rendering. The
caller terminates the final link and emits an empty SAT sentinel if necessary.

Special uploads now fail per object: a deferred carrier no longer suppresses
resident pickup/conversion objects. There is no upload if that object is inactive.
verify_front_sprite_chain.py compiles the production player, special and entity
emitters with game/admission/upload shims and real cell/main renderers.512 mixed
presence/upload-failure combinations assert exact tile sequence, all chain
links and sentinel bounds;32 player-capacity checks cover inactive normal players
and retained Survival slots. Address/undefined sanitizers pass. This tests the
actual assembled chain, not only the admission list. Target build succeeds.

ROM7900ac06a4ca4817976082e7a6836f2849ca8b06edc160bdf36ad0665316590a
completes10000controller-driven updates in both modes.1P12091refreshes,
2091misses,worst60window30,peak136.13%;2P13687refreshes,3686misses,worst29,
peak150%. Mainpeak16/large3/K2or3. The presentation assertion fails: no60FPS
claim or performance improvement is supported. Archived build/logs/source:
out/parity-audit/front-sprite-chain/runtime. No emulator frontend opened.

Remaining SAT-order gap: shared K weapon sprites still appear before the large
stage while the other K owner is batched with large art. Need a unified large
then K0..3 tail that handles resident weapon art and uploaded projectile art,
including upload fallback owner changes. Player source animation/viewport,
provisional families, full visual source comparison and timing remain pending.

## Unified large/shared-K render tail (2026-10-22)

Normal rendering now skips shared weapon identities12..15 in the ordinary
stream and emits all admitted shared slots after the three large actors,
in K0..3 order. Weapon owners provide source pose code/attr and resident tile;
kind2 projectile owners provide their dynamic-art cells. Mixed presentation
preserves slot ownership for upload caching and never uploads resident weapon
art. Resident-only visible tails do not consult DMA upload admission.

arcade_large_present_mixed retains transactional SAT publication: capacity,
unsupported dynamic art or upload failure leaves caller SAT unchanged. Existing
arcade_large_present wraps the dynamic-only path. Cached-tail fallback now
requires both unchanged visibility and unchanged resident/dynamic owner mask;
a changed owner cannot replay the previous owner's sprite on upload failure.

verify_shared_tail_render.py compiles the production entity tail and actual
mixed/cell renderers under address/undefined sanitizers, with upload/art-lookup
shims. All16 owner combinations assert large-before-K, stable K ordering,
correct resident/dynamic tiles, next links, no duplicate weapon cells and
untouched bounds.32 fallback scenarios cover same-owner reuse and changed-owner
rejection. Resident-only/no-DMA and transactional capacity checks pass. The512
front-chain/32 player-capacity cases remain green. Target build succeeds.

ROMa30623d1a3da53f191c5cc5f17c7619fe08909e2db6e7311c23c0051e866f28a
completes10000updates in both modes:1P12146refreshes/2146misses,worst60window30,
peak136.91%;2P13938refreshes/3937misses,worst29,peak154.00%. Mainpeak16,
large3,K2or3. Presentation assertion still fails; no performance improvement
or sustained60FPS claim. Archive:out/parity-audit/shared-tail-render/runtime.
No emulator frontend opened.

Sprite-family priority now follows specials,shots,players,ordinary,large,K
for the mapped owners. Remaining fidelity work includes provisional family
populations/behavior, source player entry/animation and viewport semantics,
exact full-scene source pixel/audio comparisons, and timing phase/rate policy.
Upload fallback still retains an older pose/position for unchanged owners;
source-independent target captures and full-course regression remain needed.
The recent admission/render changes add substantial CPU cost; reducing that
cost while preserving the now-tested order is a priority.

## Remove unused admission masks (2026-10-22)

After ordered-stream binding, entity/main bitmasks were no longer read by any
production caller. Removed their construction and fields from admission;
ordered identities, remaining capacity and large/K masks remain. The native
admission result shrinks36->28bytes. Emitted68000 no longer performs variable
shifts for those masks. Static instruction count is not a cycle claim (the
compiler changes layout), and no assembly implementation was introduced.

verify_admission_equivalence.py compiles the saved before implementation and
the current implementation together under address/undefined sanitizers.
262144 randomized states compare every ordered identity, membership of each
removed mask, remaining capacity and large/K decisions. Includes sparse/dense
actors, arbitrary active flags, unsupported ranks and exhausted reservations.
All pass.512 original FED8 trace cases,100003 production mapper cases,512
front-chain cases and16 mixed-tail owner combinations also pass. Build succeeds.

Before ROMa30623d1a3da53f191c5cc5f17c7619fe08909e2db6e7311c23c0051e866f28a
vs retained ROMe56b0dd40f99bbe60db010c8dd7eedf4ad38a4a3c3f36c8221557cde4086d6e1,
10000simulation-index input updates each:
1Pmisses2146->1735,refreshes12146->11735,peak136.91->132.03%,worst60window30;
2Pmisses3937->3162,refreshes13938->13162,peak154.00->149.07%,worst29->30.
Mainpeak16/large3/K2or3 in both. Exact encounter trajectories still vary with
refresh-driven source23 selection, so these are runtime workload measurements,
not a universal isolated cycle saving. Admission output equivalence is separately
proven above. Sustained60FPS still fails in both modes. Before/after ROMs,
symbols, source, assembly and logs:out/parity-audit/admission-unused-masks.
No emulator frontend opened. Remaining duplicate flag packing/list construction
and renderer branches require targeted measurement before further optimization.

## Complete-input admission cache (2026-10-22)

Admission is independent of motion/animation: its complete input is entity
flags48, legacy family ranks24, mainflags16, largeflags3, childflags4 and the
reserved single count1. Added an aligned96-byte union and a cache containing
that exact input plus the result. Every word is compared; any changed word
recomputes admission. Renderer reads cache-owned output without mutating it.
No simulation, RNG, animation, coordinates or DMA output is cached. Initialization
uses BSS valid0. A surviving identical input across reset has identical admission;
readiness is republished from the result even on hits.

verify_admission_cache.py exercises every byte/all256 values, repeated hits,
explicit invalidation and exact uncached-output comparison:73730 checks pass
under sanitizers. Existing262144 differential cases,512 original arcade cases,
100003 production mapper cases and front/tail render tests pass. Target ABI
assertions guard96-byte input and counter offsets. Linked cache size134bytes
replaces28-byte result, adding106workRAM bytes. Build succeeds.

Before ROMe56b0dd40f99bbe60db010c8dd7eedf4ad38a4a3c3f36c8221557cde4086d6e1
vs retained ROMf1b948ea4e88bd427e7373fe3ce2f20b6c928778d8f704ce8b3e4bdcfc25f185,
10000simulation-index update runs:
1Pmisses1735->1312,refreshes11735->11312,peak132.03->129.83%,worst60window30;
2Pmisses3162->2891,refreshes13162->12891,peak149.07->147.46%,worst30.
Measured1P9184hits/815misses (91.85%);2P8553/1446 (85.54%). Both reachmain16,
large3,K2or3. Runtime counters count calls between first/last sampled refresh;
sampling may occur within a tick. Refresh-driven wave selection still changes
later trajectories, so whole-game before/after state equality is not claimed.
Admission equivalence is independently tested.60FPS assertion still fails.
Before/after logs,ROM,symbols and source:out/parity-audit/admission-cache.
No emulator frontend opened. Further work must address renderer/update cost
and remaining provisional family/source timing behavior; cache does not prove
full arcade parity or release readiness.

## Recovered C-family formation controller (2026-10-22)

Added typed arcade_c_wave_step forF269..F2F7. Unlike the current live fallback,
source C reserves a dedicated six-slot formation after the previous live count
A7 reaches zero. State0 phase0 copies the full parameter byte (no high-bit mask),
loads30 into its subcounter, increments phase and falls through into the first
decrement on that same update. Each expiry reloads30 and decrements the byte
countdown. Parameter0 therefore wraps and takes7680 updates, not zero delay.
On final expiry it writes reservationB0=6,kindbyte7=0,state1/phase0; births occur
on the next controller invocation. State1 does not recheck live count.

Birth sequence is((clock24>>5)^difficulty96)&7. Six births usecamera+248,
source Y/delay pairs and retainedkind, and reset controller state/phase only.
gen_arcade_c_wave.py extracts8formations/48pairs directly fromF2F8..F357 with
pinned ROM hash. Birth API deliberately omits actor residual fields: future
binding must retain them and write only active128/statephase0 plus birth fields.

verify_c_wave.py guards source branch/countdown/selection bytes and tests all256
parameter countdowns (986880 update ticks), freezing each on a live formation,
196608 clock/difficulty/camera selections, inactive retention and same-update
entry fallthrough under sanitizers. Tables are independently read from raw ROM
in the verifier. All pass and ROM builds. This is an offline source contract,
not original execution traces or live C-family gameplay integration.

Actor inspection confirms BC2A sets vector-rootBE82 and animationBD01, selects
a BDAF script bykind, then B99E sequences direction/duration/animation entries.
Active movement BC86 selects separate scrolling/stopped vector tables before
B99E advances the script; replacing this with generic lane-seeking movement is
not equivalent. Next work is actor/script/animation recovery and six-slot
combat/render binding. The newly recovered controller is intentionally not yet
used by gameplay. No emulator/frontend opened for this work.

## Recovered C-family actor and motion scripts (2026-10-22)

Added arcade_c_actor_step forBBDE..BCFE and deterministic ROM extraction of
43 animation frames,53 timed direction/pose script entries and34 movement
vectors (scrolling/stopped). Only normal birthkind0 is recovered: F2A4 resets
sourcekind7 before the controller births. BC2A initializes animation/script,
increments live and falls into the per-frame delay. Delay decrements each
update and compensates141 X-fraction units only while scrolling. Active order
is movement/animation, script advancement, strict relativeX>248 edge check,
then signed-direction exit. This differs from main's >=248 cutoff.

Death phases0/4 select owner death/score roots, phase1 counts32 updates,
phase2 writes activeFF (one hidden update), phase3 clears active/state/phase
and byte-decrements live. A92E conversion follows handlers even after cleanup.
Bonus entry/score rendering uses the existing bonus poses and strict C edge
cutoff, but its full award/combat binding is not yet recovered/verified.

Source script ends with a singleFF atBE81, so B99E reads adjacentBE/86BE bytes:
direction255,timer190,animation86BE. Source9617 reads code128,attr8,timer8 and
continuation8880; BC7C then selects deathphase2 before a vector255 access can
occur. Native extraction preserves that transient pose; its unconsumed
continuation is represented by index65535. No invented normal pose replaces it.

verify_c_actor.py compares48 complete trajectories and owner death/score paths
against a separately coded ROM-addressed offline reference, reading raw source
animation and vector bytes rather than generated native tables.24334 state
comparisons pass, along with terminal,248/249 boundary and cleanup/purge/live
wrap cases. ROM hash and key source instructions guarded. Build succeeds.
This is NOT original CPU execution evidence. Motion selector is only tested
for source9D0/1; source lifecycle gating at finish/other stop values must be
established before live binding. Art residency, bonus collection/combat, target
trajectory capture and the dedicated six-slot live pool remain pending.
No emulator/frontend opened. Newly recovered C controller/actor remain isolated
until the complete live path can replace the provisional C behavior coherently.

## Corrected main score decoding and recovered C combat (2026-10-22)

Source score handler4CD9..4CF4 predecrements the award pointer for each of four
BCD bytes. Prior main-combat verification incorrectly read forward from the
pointer and accepted the following1000-point entry. Event042B points to4E9A;
its actual amount at4E96..4E99 is00000500. Corrected live main kill award to500
and the production-adapter expected score. Added exact source instruction guards
for all four predecrement additions. Main combat264704 hit cases,65536 geometry
cases,5632 bonus cases and production binding4 hit sequences/51200 renderer
cases pass. This supersedes earlier main1000-point assertions.

Added isolated arcade_c_combat for E424..E4B3 and ECD0..ED2F. Six-slot shots use
active1/state sign clear and wrapped X+8<=16/Y+10<=16 geometry. Accepted shots
set both objects active2/state2/phase0, award50/event0416/sound13, set owner,
apply difficulty0033, and decrement reservationB0 independently of live count.
Final reservation1->0 sets phase4 and queues1000/event0414/sound14 with a second
separately capped0033 increment. Reservation0 wraps255. Contact keeps source
nonzero-active/bit1-clear rules without an AI-state filter. Bonus collection
uses bonus_kind (sourcebyte3), not the C script kind, preserves phase byte wrap,
and applies the shared source bonus event table and difficulty00C0.

verify_c_combat.py guards the pinned ROM, hit instruction sequence, sound entries,
bonus event table and backward BCD decoder. Sanitized host checks cover1082880
hit/counter/difficulty/owner cases,131072 horizontal cases,65536 flag/vertical
cases and22 bonus awards. This is offline contract evidence, not original CPU
execution or integrated C gameplay. C pool/render/finish gating integration
remains pending. ROM builds successfully. No emulator frontend opened; no new
60FPS or full arcade-parity claim is made.

## C dedicated pool lifecycle (2026-10-22)

Added ArcadeCPool owning six typed actors, the recovered producer and live count.
Scheduling writes only F2D6..F2EA birth fields (Y,delay,kind,X,active128,state/phase0)
into all six fixed slots. Fractional coordinates, owner and other residual bytes
survive reservation; live increments only when each actor initializes. Warm reset
API clears active/live only and leaves producer reset to its caller, matching the
existing course-reset ownership convention; cold reset clears the whole pool.

verify_c_pool.py runs512 sanitized controller/actor/combat lifecycles across all
256 residual-byte patterns and both owners. Checks include30-tick parameter1
countdown, following-tick birth, all8 formations, coordinate wrap, preserved
residuals, live6 initialization, producer freeze while live, six kills totaling
1300points/seven separately applied difficulty0033 increments,32-update death
countdown, one hidden cleanup update, next-wave countdown and both reset APIs.
All pass; ROM builds. Pool remains unbound pending rendering/admission integration
and stop-selector investigation. These are host integration tests, not live-game
or original CPU execution evidence.

Additional source inspection: A5E2 calls main atA5FB then C atA5FE, with a9C gate
atA5F4; it does not locally gate on9D. Descriptor25 has stop2; C vector rootBE82
contains only two normal pointers (BE86,BEDB), followed by0000. Therefore simply
claiming source9D2 equals boolean stopped1 is unsupported. Source reachability
of active moving C at the finish must be established, or the third selector's
actual source behavior recovered, before claiming complete binding parity.
No emulator frontend/window opened.

## C finish-gap bound and resident art preparation (2026-10-22)

verify_c_finish_gap.py reads the pinned ROM wave pointers/masks and section
records directly. Last C-enabled screen is67; every subsequent source wave has
bit1 clear. Final stop2 occurs after86 screens (initial section starts one screen
short). Even allowing a complete13-update producer pass after entering screen68,
there are at least8353 actor updates until stop2 at source scroll141/256. Maximum
birth delay120 plus script duration471 and conservative init/terminal allowance3
gives594 updates of possible normal movement; a late hit and cleanup raises this
to629. Thus normal uninterrupted course movement cannot read the absent third
vector entry. Conversion may retain a bonus object but never returns to movement.
This bound assumes startup9C expired, as in ordinary late-course progression;
boot/reentry/reset paths still need separate integration evidence. It is not an
excuse to silently clamp arbitrary source stop values into1.

Added gen_arcade_c_art.py, walking animation/script roots directly from ROM,
including terminal transient86BE. All19 distinct source code/bank combinations
have mappings:10 share existing animation cells and9 new cells occupy36 tiles
in gaps1..12,14..17,20..39. Allocation guards protect blank0, food/entrance aliases,
barrier hints and main61..132. Generated storage1152bytes; compiled mappings and
all flip attributes pass19456 pixel comparisons against the established Genesis
bake. This preserves existing palette adaptation, not a new arcade-color claim.

Assets are not cold-loaded yet; no game-render/SAT wiring has been changed.
Before live loading, food/barrier generators must exclude these newly reserved
scene tiles so later regeneration cannot create stale aliases. Live C actor,
combat, admission and renderer wiring remains the next integration step.
No emulator frontend opened. Build succeeds.

## C resident renderer and generator reservations (2026-10-22)

Added arcade_c_render_actor using the shared cell-to-SAT converter, source
byte-coordinate wrap, viewport0/1/256/224 and resident C/bonus lookups. Active0
and high-bit-hidden actors emit nothing; insufficient SAT capacity writes nothing.
The compiled actor stride22/active offset9 are asserted. verify_c_render.py passes
66560 sanitized cases over all43 C frames,22 bonus frames, both owner palettes,
all256 active flags, ordinary/wrapped X and capacity boundaries.

Initial renderer test expected bonus-bank tile140 for code50/bank4, but C uses
identical resident animation tile598. The expected tile now permits the mapped
shared cell; verify_c_art.py additionally compares each overlapping C/bonus pixel
block directly, including owner2 variants. This is exact pixel reuse, not a
substituted pose. Food/barrier asset generators now exclude all36 C reservations
when looking for scene aliases, as they already exclude main61..132. Regenerated
food76tiles and barrier220tiles retain their established pixel hashes; independent
food10752, barrier46080 and bonus8448 pixel checks pass. Build succeeds.

No cold-load or game admission wiring yet: the renderer and pool remain isolated.
Next integration must add dedicated C identities to admission without shrinking
the unrelated legacy pool, and wire source ordering for update/shot/contact/spawn.
No emulator frontend opened; no new frame-rate or live-parity claim.

## Live C-family integration (2026-10-22)

Normal gameplay now owns ArcadeCPool independently of legacy entities. The source
CLASSC bit is excluded from the provisional spawn path; its recovered controller
runs after main in EF51 order. Actor updates run after main, before G/H; rebasing
and warm/cold course reset cover the pool. Both player shot slots scan main then
C before later ordinary families; C awards/clear bonus/difficulty and sounds use
the recovered combat functions. Player contact/bonus collection follows main
before K. Survival retains its separate provisional behavior intentionally.

Dedicated C identities64..69 enter admission after main and before legacy D/E/F,
without shrinking the48-slot legacy entity pool. Input grows96->104 bytes with
explicitly initialized padding; cache compares26 words, counters move132/136.
Instrumentation offsets updated, target ABI assertions remain. A compatibility
wrapper retains the no-C admission API for existing independent source tests.
Renderer dispatch recognizes C before main's >=48 branch, and cold initialization
loads its nine exact resident cells after captured-scene art. Food/barrier
regeneration already excludes those locations.

Verification passes: production C shot adapter clears six for each owner;
102400 independent C flag/capacity/priority cases;100003 production readiness
cases with C flags;79874 cache checks across all104 input bytes;1024 production
front SAT chains including C between main and G; existing shared-tail ownership
and main adapter checks. Archived512 original admission cases still pass through
the unchanged no-C contract. ROM builds. The first windowless18000-update runs
reach C6/main16/large3 in both modes. InitialROMa8e9f766f5f4a1300437718602360082bac0273ac0aef9b5593f520064344476:
1P21803refreshes/3803missed,2P25420/7419missed;worst60window29 in both.
These longer workloads are not a like-for-like comparison with earlier10000-tick
runs.60FPS assertion fails as expected; no full parity claim. Initial reports
archived under c-binding/runtime-initial. A redundant whole-input clear was then
removed (all fields explicitly assigned), and the retained build is being rerun.
No emulator frontend or window opened. D/E/F, full source timing/boot alignment,
finish/reentry coverage and performance remain outstanding.

RetainedROM00c184671ee1afee06119269016c63c4536aa4f196e765fb3e369a2746819f8b
completes18000updates in both modes:1P21678refreshes/3678misses,worst30,
peak149.12%;2P25083refreshes/7082misses,worst29,peak177.88%. Both reachC6.
Cache16649/1350hits/misses in1P,15290/2709 in2P. Full logs,ROM,symbols and live
source snapshots archived in c-binding/runtime. No tests/processes remain running.

## D formation controller and source player selection (2026-10-22)

Recovered F358..F402 as arcade_d_wave_step with eight four-slot formation tables
extracted directly from F403..F442. State0 freezes while liveA8 is nonzero;
entry copies the full parameter byte and immediately decrements a30-update timer.
After its byte countdown expires it reserves4 (B1), clearskind, enters state1;
births occur on the following controller call without another live check.
Selection is((clock24>>5)^difficulty96)&7. Each birth supplies camera+248,Y,delay,
kind,initial direction9 and a shared target offset derived from AD53's player:
relative X quarters0/1/2/3 map to80/144/208/32. The final quarter wraps the byte
addition to0, substitutes16, then adds16. Direction9 is a movement-vector index,
not HP (BFC8..BFCE uses it to multiply the five-byte vector stride).
verify_d_wave.py passes all256 countdowns/986880ticks,196608 selections across
three camera wrap boundaries/all quarters and256 inactive-retention cases with
source instruction guards and sanitizers. Controller is not yet live-bound;
D actor,combat and rendering recovery remain.

AD53 investigation exposed a live formation-selection mismatch: entity.c used
flow!=GAME_OVER and signed native X, so a dying player could be selected. Added
arcade_tracking_player with exact((~kind)&active&1) eligibility, unsigned worldX,
P1 tie resolution, P1 fallback when neither is eligible and P2 only when tracking
is enabled and P2 qualifies. F3A3/F7-related spawners ignore source carry and still
use the returned player pointer. Live formation_schedule now receives this
selection from game_player_target_flags instead of the legacy flow predicate.
verify_tracking.py guards AD53/AD71 bytes and compares9437184 host branch-reference
cases over all65536 packed flags, both player positions, unsigned boundaries and
ties under sanitizers. No original CPU runtime comparison is claimed. This live
selection correction changes trajectories intentionally; previous performance
records are from the earlier build. No emulator/frontend opened.

## D direction selection and movement tables (2026-10-22)

Recovered C0AD/C0DB as arcade_d_aim. Source subtracts16-bit targetX-actorX,
uses unsigned borrow to choose left/right, takes the absolute difference, then
retains only its low byte as C29C divisor. Y uses byte subtraction/borrow and
absolute magnitude. This intentionally differs from full-width signed aiming.
C29C quotient/remainder feed thresholds>5,>=2 and half-Y versus remainder to
choose angular step1..4, then quadrant maps to direction1..16. Divisor0 gives
quotient255/remainder numerator, preserved explicitly. No runtime CPU interpreter
is added; normal native division implements the recovered arithmetic.

Extracted34 signed fixed-point vectors from C1EE's two rootsC1F2/C247 into native
s16 dx/dy tables, preserving the source24-bit X values (all fit signed16). These
remain limited to established scroll selectors0/1; source stop2 reachability for
D still needs proof. verify_d_motion.py guards C0DB/C29C/threshold instruction
bytes and compares5570560 steering cases against a separately implemented
shift/subtract reference, including all byte distances/quadrants, all Y pairs at
nine unsigned/wrapped worldX boundaries and zero divisors. All34 native vectors
are independently compared against the raw170-byte ROM region. Sanitizers pass.

Actor investigation for next step: state0 initializes vector rootC1EE and pose
via direction9, incrementsliveA8, falls through delay with141 fractional scroll
compensation. State1 moves then tests targetOffset+camera-X+4<=8 to enterstate2.
State2phase0 setsdirection0 and a difficulty-group wait from C007; phase1 keeps
moving/animating and counts delay. Phase2 waits until(clock24+3-slot)&7==0,
selects AD53's player and aims, resets pose, writesdelay24 and entersphase3.
Phase3 moves then testsY>=248 and relativeX lowbyte>=248 to request death4/2.
State3 is a no-op. Death4 phases0..4 parallel the C lifecycle with D-specific
pose roots and liveA8. Shared bonus/purge handling remains to translate.
D is still provisional in live gameplay; this module is preparation for its
complete actor replacement. No emulator/frontend opened.

## D actor state machine (2026-10-22)

Added arcade_d_actor_step for BF30..C128 plus ROM extraction of39 animation
frames and direction/death/score roots. Entry initializes direction pose and
incrementslive, falling through delayed activation; scroll compensation retains
fractional coordinates. Approach moves/animates before checking the target
window. Wait state preserves the current pose while setting direction0, uses
source difficulty waits240/216/192/168, then advances movement/animation during
each countdown update. Aim phase waits for(clock+3-slot)&7==0, stores selected
target, computes the recovered direction, resets its pose, storesdelay24 and
enters aimed movement. Source does not decrement that24 in the movement phase.
Y>=248 or relative-X lowbyte>=248 starts death4/2. State3 remains no-op apart
from the global purge wrapper. Death/score poses preserve the32-update countdown,
hiddenFF update and delayed live decrement. Shared converted bonus handling uses
B291 roots, byte bonus rotation, owner palette and D's >=248 cutoff.

verify_d_actor.py compares144 trajectories/53076 update states against a separate
ROM-addressed Python reference reading source animations/vectors directly. Covers
four slots/four difficulty groups, scrolling/stopped/toggling, three birth delays,
nonzero fractional state, camera wrap, moving targets and complete cleanup.
Both owners' death and formation-score paths are included. Additional5632 bonus
cases read B291 data independently, covering all bonus limits, score-owner palette,
edge death and separate purge boundaries/post-cleanup ordering. Source branch
bytes guard movement/check ordering and slot aim timing. No original CPU execution
claim; scroll selector contract remains0/1. ROM builds successfully.

The D controller, motion and actor are not yet used by gameplay. Remaining work:
accepted-shot/score/contact rules, four-slot pool, exact resident graphics,
admission and live integration, plus finish/reentry and target runtime validation.
No emulator/frontend opened.

## D combat and four-slot pool (2026-10-22)

Recovered E4CA..E55E shot scan/hit and ED30..ED47 shared player contact into
arcade_d_combat. Shot geometry matches C's wrapped X+8<=16,Y+10<=16, with four
stable slots and active1/state sign clear. Accepted shot sets actoractive2/state4/
phase0 and shotactive2/state2/phase0. Event0417 is100points (end-pointer4E4A,
BCD4E46..49),sound13,difficulty0040. ReservationB1 decrements independently of
liveA8;1->0 changes actorphase4,adds event0413=500points (end-pointer4E3A),
sound14 and a second sequential0040 difficulty increment. Four kills total900.
Contact uses shared ECE9 geometry/active rules; bonus awards retain sourcebyte3
semantics. verify_d_combat passes1082880 hit cases,131072 horizontal cases,
65536 flag/vertical cases and22 bonus cases with source bytes and backward BCD
pointer guards under sanitizers.

Added ArcadeDPool: four dedicated actors, D controller/live ownership, stable
slot-index actor updates and minimal source birth writes including target_offset
and direction9. Warm reset preserves residuals while clearing active/live;
caller still owns producer reset. verify_d_pool passes512 complete sequences
across all256 residual patterns/both owners: producer countdown, four births,
retained state, live initialization,900-point clear, five0040 increments, death
and hidden cleanup, next wave and cold/warm reset. ROM builds successfully.

Art audit finds15 distinct D code/bank poses; nine reuse animation/C-resident
cells and six need new exact2x2 cells (24tiles). These are not yet allocated or
loaded. Current lower scene gaps cannot hold all24 without colliding with food,
C or main art; subsequent integration must establish a safe arena or exact reuse.
No art reduction or legacy pool reduction has been introduced. D pool/combat
remain isolated until art/admission/live wiring is complete. No emulator opened.

## D resident graphics, renderer and target VRAM readback (2026-10-22)

Corrected prior pose-count note: D has16 distinct code/bank poses,10 shared and
six new cells. Added deterministic ROM-root art extraction;39 animation frames
and22 bonus frames render through the shared SAT cell converter.16384 pixel
comparisons validate all16 poses/flips;62464 sanitized renderer cases cover
flags/owners/wrap/capacity. C's existing tile32 supplies D's shared death pose.
D's score49 poses do not alias bonus score50, unlike C. Asset generation paths
are separate and C was regenerated/retained after fixing an initial copied path.

Six new cells occupy tiles1921..1944 (F020..F31F). Installed SGDK vdp.c defaults
HScrollF000 and SATF400; vdp_bg.c's whole-plane setter writes exactly one word
atF000 orF002. Game cold init now explicitly sets HSCROLL_PLANE/VSCROLL_PLANE,
HScrollF000 and SATF400 before loading D art. Compile-time bounds protect the
scroll words and SAT; no row/line scrolling is used by this game. This is an
explicit manual arena outside SGDK's normal pattern allocator, like existing
finish/barrier arenas; future line-scroll changes must relocate it first.

Optional SONSON_VRAM_DUMP in the windowless core runner captures exported VRAM
and VDP registers at run completion. After1000 controller-driven1P updates,
verify_d_art_readback.py confirms all768 D pattern bytes exactly, reg11mode0,
reg13=3C(HScrollF000),reg5=7A(SATF400). ROMc02570b193a2230c51d7547739ada964a6d111e0a97ba2ae2ab20ae7f15787a8.
The run completes but the60FPS assertion still fails (407 missed presentations;
worst60window34). This short memory-readback run is not a new performance baseline.
D art is cold-loaded; D actor/combat/admission still await live binding. No
MAME or RetroArch frontend/window opened. Build succeeds.

## Live D-family integration (2026-10-22)

Normal gameplay now owns ArcadeDPool, replacing CLASSD's provisional emitter
spawns. Controller runs after C and receives AD53-selected target X. Actors update
after C using source slot timing, current tracked player coordinates, difficulty,
scroll/purge and shared bonus counters. Warm/cold reset and coordinate rebase
cover all four slots. D shots follow C before later families; D player contact
and bonus collection follow C before shared K. Survival retains separate behavior.

D identities70..73 enter sprite admission after C and before E/F, preserving all
legacy pool slots. Input108bytes/27words includes four D flags; cache ABI counters
now136/140 and runner offsets updated. No-C/no-D wrappers preserve earlier source
comparison contracts. Render dispatch checks D before C/main; D poses use the
already-readback-verified cold-loaded art. Static actor ABI guards22byte stride
and active9 remain in the renderer/runtime instrumentation.

Checks pass: both-owner production four-kill adapters;6400 D capacity/priority
cases;102400 retained C priority cases;100003 readiness cases now including D;
82946 complete-input cache checks;2048 production SAT chain cases with D after C;
shared-tail tests; archived512 original no-C/no-D admission cases; retained main
shot adapter and renderer cases. ROM builds.

Windowless Genesis Plus GX runs complete18000 updates each, reachingD4,C6,main16,
large3 andK3 in both1P/2P. ROM1bc73af94206761348b2c9cf092b97cf0d1403ecfc814db022fec4030ff33114:
1P23154refresh samples/5155 zero-presentation deltas,worst60window29,peak154.49%;
2P27700/9700zero-deltas,worst29,peak171.19%.60FPS assertion FAILS. This is a new
live behavior workload, not an equivalent trajectory to provisional-D builds;
source23 still refresh-driven, so no causal performance comparison claimed.
ROM,symbols,logs,CSVs and live-source snapshots archived in d-binding/runtime.
Source timing/boot/finish/reentry coverage, E/F behavior and performance remain
unresolved. No MAME/RetroArch frontend or window opened; no runtime process left.

## E controller and turning rules (2026-10-22)

Recovered F443..F4BB as arcade_e_wave_step. E owns two dedicated slots; its
countdown freezes whileliveA9 is nonzero, copies the full parameter and falls
through on entry. Unlike C/D, only globalclock24 phase0mod32 decrements wait.
Parameter0 wraps256 decrements. Final decrement selects state1/phase0; next call
births two without rechecking live. Difficulty bits3..5 alone select eight pairs
from F4BC..F4DB; writes are camera+248,Y,delay,direction9,active128/statephase0.
No reservation or kind write is invented. verify_e_wave passes8192 complete
parameter/starting-phase sequences (33558528 ticks),196608 formation selections
and256 inactive retention cases, guarded against pinned ROM bytes and sanitized.

Recovered C44D steering as arcade_e_turn: direction0 becomes1; other directions
turn one step, selecting X or Y comparison from C45B's17-entry dispatch table.
Unsigned comparisons and equality directions are preserved;1/16 wrap circularly.
verify_e_motion compares5570560 cases against an independent ROM-dispatch/branch
reference and checks all34 movement vectors directly against source bytes.

Unlike D, E's stopped vector rootC647 contains24-bit X deltas exceeding signed16
(e.g. direction1=131072 fractional units). Extraction initially rejected this;
ArcadeEVector now stores signed32 X to retain all source24-bit values, with signed16
Y. Do not clamp or substitute D vectors. Original runtime behavior/reachability
of these unusual stopped entries remains an explicit verification obligation.

Actor inspection: E init setsactive1, incrementsliveA9 and decrementsA4, with no
same-update delay fallthrough. Approach/homing alternates slot-specific clock
phase, AD53 carry can select exit when no eligible player exists, and every ninth
steering action introduces a16-update pause with scroll compensation. These and
shared A4 ownership still need complete actor implementation/verification.
The new controller/motion remain isolated; live E is provisional. Build succeeds.
No emulator/frontend opened.

## E actor and shot-combat contracts (2026-10-22)

E actor implementation now covers C2D5..C4C7 entry, homing, ninth-turn pause,
no-eligible-player exit, death and shared bonus entry. Initialization writes
active1, decrements A4 and increments A9, then returns without same-tick delay
fallthrough. Pause entry falls through to the first decrement (16 becomes15).
Cleanup immediately clears the actor and restores A4/decrements A9; there is no
C/D-style hidden FF update. verify_e_actor passes90 trajectories/9694 state
comparisons against a ROM-addressed reference, including the full24-bit stopped
vectors. Bonus/purge verification and original runtime stopped-vector reachability
remain pending. The actor is not yet bound to live gameplay.

Recovered E761/E78D shot combat as arcade_e_shot_targets/arcade_e_hit. Crucially,
E checks shot eligibility once and scans both slots WITHOUT stopping after a hit.
An overlapping pair can both die to one shot. The new API returns a two-bit mask;
callers must apply events in slot order, even though the first hit consumes the
shot. Eligibility is active1 and nonnegative source state; wrapped vertical test
is enemyY-shotY+9 <=16, horizontal enemyX-shotX+8 <=16. Each kill sets E death
state3/phase0, shot state2/phase0, owner, event0418 (300 points), sound12 and a
separately capped004C combat-difficulty increment. No group-clear bonus/counter
is invented. Live counts remain actor lifecycle ownership.

verify_e_combat guards the entire source scan/hit sequence, sound dispatch and
BCD reward data against the pinned ROM. Sanitized host tests pass131072 horizontal
cases,196608 vertical/flag/state cases and131072 double-hit sequences covering
both owners and every incoming16-bit difficulty value. These are offline source
contracts, not original CPU execution or live E integration. No emulator opened.

## E two-slot lifecycle composition (2026-10-22)

Added ArcadeEPool with two dedicated actors, its source controller and live A9
count. Schedule writes only F49A..F4AE's X/Y/delay/direction/active/state/phase;
fractions, animation and other residual fields survive until their owning actor
state updates them. Actor updates run slot0 then1. Shared A4 is passed by pointer,
not synthesized from E's population. Cold reset clears the pool; warm reset clears
active/live only, leaving producer and shared-counter reset to the caller under
the existing course reset API convention.

verify_e_pool passes512 sanitized lifecycle fixtures, covering every initial
residual-byte pattern and both owners. Checks include off-phase global-clock
waiting, next-call birth, no delay decrement on initialization, A4 byte wrap,
live-count producer gating, two overlapping kills from one shot (600 total),
32-update death timer, immediate following cleanup, next wave and both reset APIs.
The overlapping kill pair is explicitly injected; this is component integration,
not controller-driven arcade reachability or a live binding claim.

A4 ownership remains a live integration obligation: FECF resets it to24 for sprite
emission; FF4A/FF6A consume it, while E init/cleanup modify it and C82E/F080 inspect
it for spawning. Do not replace it with a private E reservation counter. Existing
source admission/frame ordering must determine which value each consumer sees.
Graphics, contact/bonus completion and live E binding remain pending. No emulator
window or frontend opened.

## E player contact and converted bonuses (2026-10-22)

Added E contact scan using ED49/ED5B's dedicated two-slot dispatch into ECE9.
Player active must be1; enemy active must be nonzero with bit1 clear. The first
slot satisfying wrapped inclusive +/-7 bounds wins, independently of enemy state.
Caller distinguishes bonus bit6 from lethal contact. This first-contact behavior
is deliberately different from E's both-slot shot scan.

ED18/E838 bonus collection sets active2, increments phase with byte wrap, copies
owner and awards source kinds0..9 as1000..10000, kind10 as20000; event tableE89C,
sound5 and separately capped00C0 difficulty increment are preserved. Invalid
kinds leave actor/output untouched as the existing native boundary contract.
verify_e_combat now guards the contact dispatch/body, bonus branch and award
routine plus ROM BCD entries. Sanitized checks pass131072 contact axis cases and
1082880 valid/invalid bonus/owner/difficulty cases, alongside all prior shot tests.

verify_e_actor now covers5632 bonus kind/limit/owner combinations against the
source B291 animation roots, next-kind wrapping, owner palette, x248 cutoff and
immediate following E cleanup/counter restoration. Purge boundaries are checked
after no-eligible-target exit and after cleanup, including A4 wrap. Existing90
trajectories/9694 state comparisons and512 pool lifecycle fixtures still pass.
These are offline component checks; original stopped-vector runtime behavior,
shared A4 frame ownership, graphics and live E integration remain pending.
No emulator frontend or window opened.

## E graphics inventory and exact-sharing audit (2026-10-22)

Added audit_arcade_e_art.py: independently walks C4C8/C5C8's ROM animation
roots and compares all74 frames'21 distinct code/bank poses against the currently
resident animation, C, D and main pixel arrays. Nine poses share exact existing
cells: code0/3/26/27/28 bank7 use558/566/570/574/578, code29 bank7 uses C tile32,
and code110/111/112 bank4 use502/506/510. Twelve poses, code113..124 bank4, need
new cells (48tiles/1536bytes if fully resident).

The audit also reconstructs each full16x16 cell from column-major Genesis tiles
and compares every horizontal/vertical flip against resident cells and other new
cells. All transforms round-trip exactly; none of the12 missing cells can be
eliminated by exact whole-cell flip sharing. Pixel equivalence here is relative
to the established indexed Genesis bake, not an arcade palette-fidelity claim.
Manifest: out/parity-audit/e-art/audit.json. No runtime VRAM allocation or renderer
change has been made. Next graphics work must reserve safe space or implement a
bounded pose cache with upload-before-publication checks; existing D high-VRAM,
SAT, scrolling and other resident resources must remain protected. No emulator
opened; this audit does not change or validate the ROM's current performance.

## E graphics data and bounded upload cache (2026-10-22)

Added gen_arcade_e_art.py and generated arcade_e_art data. Nine exact resident
aliases return tile indices; twelve missing poses return tagged cell IDs distinct
from VRAM indices. All twelve128-byte cells are immutable ROM data. Unsupported
code/bank combinations return65535. Attribute flip bits do not change cell lookup.

Added arcade_e_upload with two dedicated four-tile cache slots:1945..1948
(F320..F39F) and1972..1975(F680..F6FF). These are after D's F020..F31F arena and
after the full80-entry SAT F400..F67F, respectively, and before finishF700.
Compile-time guards protect D/SAT/finish boundaries. Like D, these manual arenas
require whole-plane scrolling atF000 and SAT atF400; live binding must retain
those settings. No existing resident allocation has been moved or removed.

Each slot uploads only a changed requested cell, up to256bytes total per update.
Preflight accounts for queue descriptors,128KiB source-boundary splits, bytes and
caller-reserved later transfers. Uses ordinary checked SGDK DMA queue calls with
immutable ROM sources; no Z80 bypass, staging reuse or early flush. Cache reset
is required after scene load; calls require the previous frame queue to have
flushed and successful admission before publishing the new pose.

verify_e_upload passes144 simultaneous pose pairs under ASan/UBSan, byte and
operation capacity refusal/retry, dirty reuse, unused-slot retention, reset,
deferred transfer and byte-for-byte preservation outside both arenas. Compiled
new art passes3072 pixel comparisons against the established source-to-Genesis
bake. ROM builds655360bytes. Target queue split behavior, VRAM readback, renderer
publication, live E integration and current performance remain unverified.
No emulator frontend/window opened.

## E renderer with explicit pose readiness (2026-10-22)

Added arcade_e_render_request and arcade_e_render_actor. Requests name a missing
ROM cell only for visible active actors; resident/bonus/unknown poses need no E
cache upload. Rendering resolves resident aliases and shared bonus poses, or
requires the exact requested cell ID for the dedicated E slot before emitting
its cache tile. Unknown, stale, unavailable or invalid-slot requests write no
SAT entries. Caller must obtain readiness from accepted upload work and preserve
same-frame transfer-before-presentation ordering; a matching ID alone is not
proof of hardware completion. Inactive and bit7-hidden actors do not render.

Source8-bit camera-relative X and negated Y feed the existing cell converter,
including wrap copies and capacity-transactional writes. Target compile assertions
pin E actor20bytes and active offset7. verify_e_render passes98304 sanitized cases
covering all74 normal animation frames,22 bonus poses, both palettes/cache slots,
all256 active flag values, interior/right-wrap placement and no-capacity writes.
It additionally rejects missing/wrong ready-cell IDs and invalid slot2 without
writes. ROM builds655360bytes. Live binding and target VRAM readback pending.

Source A5E2 actor order reconfirmed: mainB9BA, C BBDE, D BF30, E C2D5, then
later families including C69B (whose C82E reads A4). E must receive shared counter
state at this point; it cannot own a private availability counter. Current native
admission supplies readiness separately, so its timing must be reconciled during
binding rather than silently replacing this source dependency. No emulator opened.

## E admission identities and unchanged cache ABI (2026-10-22)

Added arcade_native_admission_cde, with E identities74/75 after D/legacy-D and
before legacy-E/F. Both E slots use the normal one-cell source budget and active
visibility rules, in slot order. Old cd/c/no-family entry points remain wrappers
with absent later families, preserving archived comparison contracts.

The108-byte admission input now stores two E flag bytes where its two padding
bytes previously lived. Cache size/27-word equality and profiling offsets136/140
are unchanged; all inputs are still explicitly initialized. The production mapper
currently sets E flags zero until the complete live pool/upload binding is ready,
so this does not activate E gameplay or claim original source A4 timing parity.

verify_e_admission passes31824 independent explicit-order fixtures: variable prior
occupancy, both E flags/slot order, reserved capacity0..25, later families, atomic
large bodies and shared K tail. Cache verifier now uses full C/D/E input and passes
82946 checks including all108 input bytes/all256 values. Existing100003 production
readiness fixtures,6400 D priorities,512 archived original admission comparisons,
2048 SAT-chain cases and32 player-capacity cases pass. ROM builds655360bytes.
Live E binding, shared A4 actor/controller timing and target performance remain
pending. No emulator frontend/window opened.

## Live E-family binding (2026-10-22)

Normal gameplay now owns ArcadeEPool and excludes CLASSE from provisional wave
spawning. Controller follows D, actors update after D with AD53-selected target
and explicit eligibility from active/kind flags. E's init/cleanup operate on the
shared arcade_main_ready byte; subsequent shot-budget consumers see its updated
nonzero status. This preserves shared ownership instead of adding a private E
counter, but the full original A4 render/actor/controller phase contract still
requires runtime comparison. Survival retains its separate spawner/behavior.

E shot scanning follows D; each selected player's two shots apply both E-slot
hits in order even after the first consumes the shot. Contact and converted bonus
collection follow D. Warm/cold resets, producer state and world-coordinate rebase
cover both E slots. Admission maps real E active flags to identities74/75, and
render dispatch recognizes them before D. Only admitted E actors request cache
uploads, reserving704bytes/two descriptors for later work. Successful upload
requests publish the corresponding ready IDs; refusal invalidates readiness and
suppresses dynamic E poses until retry (an explicit remaining visual/performance
risk, not a claim of source-equivalent behavior under overload).

Tests pass: four production E shot sequences (both owners, overlapping double
kill and separated hits),31824 E admission fixtures,82946 cache checks,100003
production readiness cases now including E,4096 full front-SAT chain cases with
E between D and G, retained D combat/admission, and shared-tail ownership/fallback
checks. Strict host compilation caught an ambiguous else in initial readiness
publication; explicit braces fixed it before the final passing runs. ROM rebuild
succeeds655360bytes. No runtime emulator run performed: E population reachability,
VRAM uploads, stopped vectors, shared A4 timing and performance remain unverified.
No emulator frontend/window opened.

## Integrated E runtime population and pacing baseline (2026-10-22)

Extended the windowless Genesis Plus GX harness with E population tracking,
using the target-asserted20-byte actor stride/active offset7. Both1P and2P complete
18000 controller-driven simulation updates and reach E2,D4,C6,main16 and large3.
Lives are refilled by the harness; neither run completes a scenery ribbon wrap,
so this is not a full-course playthrough or arcade-state parity proof.

ROM655ce1c957246572dabfad504743c06a4ab411f0739a120744c621e2558b3ec8,
core88314cd8554a3cc5eebf00a4139e6f0dee9ea579dcb2321a0959579f1902fa09.
1P:23298 refresh samples,5299 zero-presentation deltas,worst60window29,
peak159.23%,cache16590hits/1409misses.2P:29500samples,11500zero-deltas,
worst29,peak180.52%,cache15169/2830. Both60FPS assertions FAIL. New E behavior
and refresh-clock-dependent controllers change trajectories; do not attribute
baseline differences to E upload cost without matched-state measurements.

ROM,symbols,build manifest, relevant live code/harness snapshots, reports,logs and
CSVs archived under out/parity-audit/e-binding/runtime, with source hashes.
Refresh-boundary PC samples during zero-presentation deltas most often resolve
to entity_render_all (2274 of5299 in1P;4307 of11500 in2P). These samples identify
where to investigate, not exclusive cycle cost or root cause. Targeted renderer
profiling remains necessary. E VRAM/presentation matching, stopped-vector source
behavior, shared A4 timing, F and final campaign flows remain unresolved.
No MAME/RetroArch frontend/window opened; both runner processes are finished.

## Readiness-loop profiling experiment, reverted (2026-10-22)

Mapped archived missed-refresh PCs onto the actual68000 disassembly. Many samples
attributed to entity_render_all lie in its inlined readiness mapper, especially
inactive legacy-slot handling. Tested splitting fixed slots0..23 and ranked
legacy slots24..47 to remove per-slot range checks. Differential tests compare
all108 produced input bytes across100003 production-style fixtures; inputs match.
Existing4096 SAT-chain and shared-tail checks pass. Candidate ROM
cff134197b31effea492749309d652f2c0952213a00130a48a6fd58ae2d61dca built successfully.

Candidate18000-update runs:1P23593samples/5594zero-deltas,worst28,peak155.22%;
2P28077samples/10076zero-deltas,worst29,peak180.96%. Both60FPS checks FAIL.
Against the integrated E baseline,1P missed presentations increased while2P
 decreased; source23 remains refresh-driven and populations/cache workloads differ.
No causal or general speedup is established. Reverted the production loop rather
than retaining an inconclusive optimization. Both old and candidate mappers remain
as test fixtures; the verifier compares them and current production input bytes.
Candidate ROM/ELF/source/disassembly/traces/reports remain in
out/parity-audit/readiness-split. No emulator frontend/window opened.

Disassembly also shows repeated volatile g_survivalRun reads within renderer
branches. A future frame-local mode snapshot may be useful, but requires checking
all writes/callbacks and measured equivalent-work evidence before acceptance.

## Frame-local renderer mode snapshot (2026-10-22)

Checked mode ownership: g_survivalRun is assigned in game_init; no address is
passed to writers, no increment/compound mutation exists, and the registered
sound VInt callback does not change it. entity_render_all now reads this volatile
mode once into a local constant. No gameplay state, admission ordering, animation
rate, enemy capacity or transfer policy is altered. Actual68000 disassembly has
one g_survivalRun reference in the renderer instead of22 separate reference sites.

Stored the previous renderer as a semantic fixture. Full4096 front-chain cases
compare old/new SAT bytes and counts, plus256 Survival direct-pool/popup scenes.
Existing shared-tail tests pass. ROM builds655360bytes, hash
3d4f096f35dcd6c70e9d806e89fe442e06984f1d531abd0625974f60360ee25d.

Windowless18000-update runs complete both modes and reach E2/D4/C6/main16/large3.
1P23152refresh samples/5152zero-presentation deltas,worst60window29,peak161.62%;
2P29380samples/11380zero-deltas,worst29,peak178.66%. Both60FPS assertions still
FAIL. Missed counts are modestly lower than integrated-E baseline5299/11500,
but refresh-driven source23 changes trajectories, particularly1P. Do not claim
an exact causal whole-game speedup from these totals. Retain the mode snapshot
because its immutable-in-frame ownership, byte-identical render fixtures and
elimination of repeated volatile reads are established; final60FPS remains open.
ROM/ELF/symbols/source/disassembly/reports/logs/CSVs archived under
out/parity-audit/render-mode-snapshot. No emulator frontend/window opened; both
runtime processes have finished. Original arcade comparison and physical hardware
claims are unchanged.

## F controller and four-slot formation data (2026-10-22)

Recovered F4DC..F579 as arcade_f_wave_step. Source liveAE gates state0 only;
entry copies the full byte parameter and falls through to phase1. Clock24 phase0
mod32 decrements wait, parameter0 wraps256 decrements. Final decrement writes
reservationB6=4 and advances state1/phase0 without spawning. The following call
births four dedicated slots without rechecking live or rewriting reservation.
Birth writes X,Y,originY,delay,direction,active128 and state/phase0; the eventual
pool binding must preserve other actor residuals.

F57A selects four64-byte placement banks via difficulty bits4..5, then one of
eight four-pair rows via bits1..3. Bit5 also selects side: rightcamera+248 with
direction5 or leftcamera-8 withdirection1. Thus32 formations are retained exactly,
including the correlated side/bank selection. gen_arcade_f_wave.py reads the
original pointer rootsF582/F5C2/F602/F642 and generates immutable native data.

verify_f_wave guards the pinned ROM countdown, selection and birth instructions;
its expected placements independently follow source pointer/offset arithmetic.
ASan/UBSan checks pass8192 countdown sequences/33558528 ticks,196608 selections
(all difficulty/clock values and camera wrap cases),256 inactive-retention cases.
Delayed birth, live freeze, full-byte wrap and reservation timing are checked.
F actor lives at DE95 and uses sourceAE; its combat EC0B consumes B6. Actor,
combat, art and live binding still await implementation. The new controller is
isolated; live F remains provisional. No emulator frontend/window opened.

## F entry vectors and table-driven cruise motion (2026-10-22)

Recovered DE95 actor dispatch: state0 entry/delay, state1 death/score, state2
sixteen-update straight entry and state3 oscillating cruise; bonus stateFF uses
shared B351/B36A. Entry animates before delay handling and increments liveAE;
zero delay sets active1/state2 immediately. Death retains the hiddenFF cleanup
phase used by C/D. Full actor implementation remains pending.

Added arcade_f_motion and deterministic ROM extraction. E09A supplies nine
24-bit X/16-bit Y entry vectors. E0C7 supplies nine24-bit cruise X vectors, with
normal direction1=282 and direction5=-141 fractional units. No invented symmetric
speed or generic sine is used. E0E2's complete128-byte table is preserved, including
index9=40 (not the smoother expected56) and final entry3. Phase advances028D,
wraps as16bits, toggles the half selector when the result's high bit is set, then
clears that bit. Wave Y replaces the integer coordinate with origin plus/minus
the upper product byte of amplitude times the indexed sample. Normal amplitude
is48; horizontal movement and Y fraction ownership await the full actor binding.

verify_f_motion passes524288 phase/half/amplitude cases and65536 amplitude/table
cases, all18 raw vectors and128 table entries, with ASan/UBSan. Expected phase
arithmetic uses separate byte carry and expected scaling uses repeated addition.
This is an offline ROM contract, not independently executed original E162 CPU
multiply or live F movement validation. No emulator frontend/window opened.

Follow-up: an independent byte-operation reference of E162's temporary stack and
carry chain passes all65536 amplitude/sample pairs in F's specialized domain
(amplitude<<8 times sample), confirming the reduction to amplitude*sample/256.
This strengthens arithmetic evidence without claiming original CPU execution.
ROM builds655360bytes; movement helpers remain isolated pending the F actor.

## F actor state machine and animation graph (2026-10-22)

Added ArcadeFActor and DE95..E02F native state transitions. Extracted28 animation
frames from flightE032/E040, deathE04E and scoreE074 roots. State0 initialization
selects the direction-dependent flight pose, clears half-cycle, increments AE
and falls through delay handling. Zero delay sets active1/state2; nonzero delay
decrements and adds141fractionalX only while scrolling. Initial hidden active128
is retained during the delay. Direction and all unrelated residuals are retained.

State2 entry sets16 then immediately performs the first straight movement and
decrement. State3 entry sets amplitude48 and wave_phase0 then immediately moves,
replaces integerY from the source table and animates. Unlike the waiting delay,
these movement handlers do not test stopped; preserve source behavior. Cruise
keeps Y fraction untouched and checks byte-relative X>=248 after movement.
Death state1 supports owner death/score roots,32-update origin_y countdown, an
activeFF hidden update then cleanup/decrementAE. source+8 serves both originY
and death timer, represented by one field. Shared bonus/purge paths are present
but their dedicated verification remains pending.

verify_f_actor compares every typed field against a separately written,
ROM-addressed animation/vector/phase reference.40 trajectories/12616 comparisons
pass across both entry directions, camera wrap, scrolling/stopped/toggled delay,
three delays and both owners' death/score paths. No generated native data is used
as expected vectors or animation bytes. This is offline native equivalence,
not original CPU execution or target/live F gameplay. Bonus/purge boundaries,
sanitzers, combat, pool, art and live binding remain open. No emulator opened.

## F combat, bonus verification and pool lifecycle (2026-10-22)

Recovered EB97/EBC5 first-shot overlap across four dedicated F slots. Wrapped
X+8<=16 and Y+11<=16 retain F's vertical offset. EBE1 changes actor to death
state1, consumes B6 separately from AE, awards50/event0419/sound12 and adds0033
difficulty. Last reservation awards500/event0413/sound14 and0040 additional
difficulty; four kills total700. ED79 contact uses the shared inclusive7 box.
Converted bonuses award1000..10000 or20000, sound5 and00C0 difficulty.
verify_f_combat passes1082880 hit,131072 horizontal,65536 flag/vertical and22
bonus cases with ASan/UBSan. Original CPU execution and live binding are pending.

verify_f_actor additionally passes5632 bonus combinations with source-addressed
animation graph, rotation/limit wrapping, owner palette, edge and hidden cleanup,
and post-handler purge boundaries including purge after cleanup. Its40 movement
and death trajectories/12616 state comparisons still pass. Actor-specific target
validation remains pending.

Added arcade_f_pool with four stable slots, AE live ownership and separate B6
reservation in the wave controller. Birth writes only the fields written by
F4DC..F579, including source+8 originY; fractions and other residuals survive.
Warm reset clears active/live only; caller must reset producer/reservation at the
appropriate source transition. Cold reset clears the entire pool. Sequential
actor updates preserve bonus selector order and death's hiddenFF update.
verify_f_pool passes512 sanitized host lifecycles covering all byte fill patterns
and both owners: retained birth fields, delayed spawn, initialization fallthrough,
live freeze, first-hit ordering,700-point clear/268 difficulty,32-tick death,
hidden cleanup, next wave and resets. Expected formation values reuse the
separately verified formation table: this test proves integration, not independent
source extraction. F art/render/admission and live gameplay binding remain open.
No emulator frontend or window opened.

## F resident art and renderer (2026-10-22)

Traversed the original E032..E099 animation graph from both flight roots and the
owner death/score pointers. All28 frames collapse to11 distinct code/bank poses.
The established Genesis indexed bake exactly matches resident animation tiles:
flight183..185/bank15 at398/402/406, death0/3/26..28/bank7 at558/566/570/574/578,
C's resident death29/bank7 at32, and score49/banks3/4 at586/590. No new patterns,
VRAM allocations or per-frame uploads are needed. audit_arcade_f_art inventories
raw source graph poses; gen_arcade_f_art rejects any unmatched resident pose.
verify_f_art checks11264 pixels across all11 poses and four flip attributes
against the established arcade-to-Genesis bake. This preserves indexed output;
it does not independently establish palette conversion fidelity.

Added arcade_f_render_actor with source byte-coordinate wrapping and shared
arcade_cell_sat presentation, including bonus art/owner palette lookup. Inactive
and bit7-hidden actors emit no sprites. Unknown poses and insufficient sprite
capacity leave the destination untouched. Host and 68000 compilation assert
22-byte actor stride and active offset9 for future runtime instrumentation.
verify_f_render covers51200 combinations of28 normal and22 bonus frames, both
owners, all active flags and interior/wrapped X, with ASan/UBSan. Explicit
checks cover flip bits, transactional two-slot refusal and unknown poses.
F admission, scheduling/combat binding and live target readback remain pending.
No emulator frontend/window was opened.

## F sprite admission contract (2026-10-22)

Extended native admission with arcade_native_admission_cdef and four stable F
identities76..79. F slots follow E/legacyE and precede legacyF and G/H. Every
visible F consumes one logical source slot; inactive/bit7-hidden records consume
none. The existing24-slot budget, stable slot order, atomic four-slot large
admission and shared-K tail are preserved. Existing cde/cd/c/base callers supply
zero F flags, preserving their behavior while the live F binding is pending.
The108-byte production cache remains unchanged in this step: its eventual live
F extension must cover all four flags and update target instrumentation offsets.

verify_f_admission passes1246752 ASan/UBSan host cases against an explicit,
independently listed family sequence: all prefix occupancies, reserved0..25,
and6^4 F visibility combinations. E admission still passes31824 cases and the
existing cache passes82946 checks/27648 input mutations. These establish bounded
logical sprite selection, not live pool ownership or target rendering. Full F
scheduler/contact/shot/update/render binding remains the next integration step.
No emulator frontend/window opened.

## F live pool binding (2026-10-22)

Replaced ordinary CLASSF provisional spawning with the native four-slot pool.
Controller runs after E and before G/H; actor update follows E; per-owner shot
scan follows E and retains first-hit behavior; contact/converted-bonus handling
precedes shared K. Warm/cold resets and camera rebasing include F. Survival's
existing direct-spawn path remains distinct. Renderer handles IDs76..79 before
E dispatch and uses only resident F/bonus tiles, without new DMA ownership.

Expanded admission identity from108 to112bytes to include all four F flags.
The cache compares/copies28 words and calls cdef. Target compile assertions now
require hit/miss offsets140/144; the libretro observer uses the same offsets.
verify_admission_cache passes86018 checks/28672 mutations. Production readiness
mapping passes100003 cases with randomized F flags and independent total-budget
checks; historical mapper fixtures are adapted only in the harness to provide
new F fields. Original fixture sources are preserved.

verify_f_binding executes the actual production shot adapter for both players:
four first-hit kills across two updates,700 total score,268 combat difficulty,
12/12/14 final sound sequence, no early live release. Static wiring checks
supplement these host fixtures. D binding still passes6400 priority cases and
both owner clears; E adapter passes4 sequences. Front renderer passes8192 chains
including F between E and G, plus256 Survival differential scenes and32 player
capacity cases. Historical no-F scenes remain byte-identical. Shared tail tests
pass16 owner combinations,32 fallbacks and capacity/resident cases.

ROM builds786432bytes, SHA256
0227ec7ffad40e35f8a3ecf83b783d86b64283b669acba248bbb48ec2065c3a8.
Windowless Genesis Plus GX18000-tick validation is running separately; full
arcade parity and60FPS are not established by this binding or these host checks.

Runtime completed both modes and reached F4/E2/D4/C6/main16/large3. 1P has
23792 refresh samples,5793 zero-presentation deltas,worst60window30,peak152.49%.
2P has30735 samples,12734 zero-deltas,worst29,peak180.32%. Both60FPS assertions
FAIL. Neither run wrapped the course ribbon; lives were refilled by the harness.
These prove live F population reachability, not original-arcade trajectory or
visual equality. Source23 remains refresh-driven, so these different workloads
must not be presented as a controlled performance comparison. Results, CSVs,
logs, ROM/symbols/entity source and harness scripts archived under
out/parity-audit/f-binding/runtime with hashes. Both processes have finished;
no emulator frontend/window opened. Remaining work includes matched source
clock policy, target F visual/state comparisons and whole-game performance.

## Resident E art must survive no-op queue pressure (2026-10-22)

F-binding missed-presentation PC samples concentrate in entity_render_all
(1P2058/5793;2P4266/12734), including its inlined admission mapper. The cache
refresh accounts for261/1034 and E uploader158/329 samples. Added an archived
sample attribution tool; these biased refresh-boundary samples are not cycle
shares. Baseline report is f-binding/runtime/missed-pc-samples.json.

Found a correctness issue in arcade_e_upload: even when both requested cells
were resident (or absent), it required spare DMA capacity for caller reserves.
Queue pressure could therefore report failure and invalidate E readiness despite
needing zero transfers. Added success return after cell validity/cache comparison
when mask==0. Dirty uploads still enforce all operation, byte, reserve and bank
split checks. This also avoids unnecessary SDK queue calls on resident frames.

verify_e_upload reproduces the old failure with archived implementation
fixtures/e_upload_before_noop.c, then passes with the fix under ASan/UBSan.
Tests cover saturated queue/resident and absent requests, invalid ID rejection,
dirty refusal without disturbing resident cache, all144 cell pairs,3072 indexed
pixel checks and protected VRAM boundaries. ROM builds786432bytes. Windowless
18000-tick runs are pending; no performance improvement is claimed yet.

No-op fix runtime completed18000ticks in both modes. ROM SHA256
81020b25c8e93d84954febb031b2cec6bd02421840a3ed5fde72fcc7d56d1eae.
1P23511samples/5512zero-presentation deltas,worst60window29,peak152.20%;
2P31403samples/13403zero-deltas,worst29,peak178.86%. Both60FPS assertions fail.
F4/E2/D4/C6/main16/large3 reached; no course wrap. Counts are mixed versus the
previous run and populations differ, so no causal overall speedup is claimed.
Retain as a reproduced correctness fix: already-resident art does not consume
DMA capacity. Artifacts archived in e-upload-noop/runtime. Both processes ended.

Timing investigation found original bytes9112..9118 = CC0001 D323 DD23:
LDD #1; ADDD <$23; STD <$23, i.e. $23 is the high byte of a16-bit counter
whose low byte is$24. This contradicts the live adapter's assumption that source23
is an independent low-byte refresh counter. Caller, reset and halt semantics must
be traced before replacement; native behavior is not yet corrected. This is a
concrete next parity blocker and likely explains divergent formation selections.

## Correct source23 byte at main formation selection (2026-10-22)

Pinned ROM IRQ vectorFFF8 points910D. Instructions9112..9118 load0001, add the
word at0023 and store it back: source23 is the HIGH byte, source24 the LOW byte.
Increment precedes mode dispatch and the3B-bit7 gate at9130. The vector ends RTI
at913E. Boot406A..4074 clears RAM; new-game8F5B initializer does not explicitly
reset23:24. The prior description of23 as an independent low-byte refresh clock
was incorrect. F029 loads23 for initial formation selection; F06F loads23 then
XORs24 for replenishment selection.

Corrected the live main-pool argument from(u8)vtimer to(u8)(vtimer>>8). This fixes
the established byte-selection error without claiming boot-phase or regional
alignment. verify_main_refresh_byte guards original vector/increment/load bytes
and compiles the actual call-site expression;262144 counter cases check all16-bit
states across four32-bit epochs, carry and wrap. The old expression selects the
wrong byte in261120 cases. verify_main_wave still passes262144 updates/166134
births against independently read ROM placements. Build succeeds786432bytes.

Remaining timing limitations are explicit: native wave_clock still advances per
simulation update and starts at0, while the original23:24 is one IRQ-owned word
that also runs outside gameplay. Source/NTSC rate and boot/menu phase alignment
are not established. Runtime benchmarks after this correction necessarily select
different formations; they are new correctness workloads, not optimization A/B.

Corrected-byte runtime completed18000updates in each mode, ROM SHA256
 d5e506f5a841cc7a0a55a89480f10f8c2233e634264c2a48c4cdbbd23a02d936.
1P23301refresh samples/5302zero-presentation deltas,worst60window29,peak155.52%;
2P31035samples/13034zero-deltas,worst29,peak180.71%. Both60FPS gates FAIL.
Both reach F4/E2/D4/C6/main16/large3; neither wraps the course ribbon. Harness
refills lives. New formations invalidate direct causal performance comparison.
ROM/symbols/source/CSV/logs/harness archived in main-refresh-byte/runtime with
hashes. Both processes finished. No emulator frontend/window opened.

## Grouped admission cache comparison (2026-10-22)

Changed112-byte cache comparison from28 single-word iterations to7 groups of4,
retaining short-circuit mismatch exit, complete identity, copy and rebuild rules.
Previous implementation retained as fixtures/admission_before_compare_groups.c.
verify_admission_cache passes86018 checks/28672 byte mutations under ASan/UBSan.
Emitted68000 comparison loop executes224 instructions on identical input before
and119 after (28x8 versus7x17). Shared prologue/return excluded; this is instruction
count, not measured cycles, because displacement addressing/branch costs differ.
Disassembly and fixed-input count report are archived in admission-compare-groups.

ROM95096462d0d7d19b2ed2a70929a5c2a40d427c1021a9cb88047cfd07305d97c0
builds786432bytes. Windowless18000-update runs complete both modes:1P23169samples,
5170zero-presentation deltas,worst60window29,peak155.03%;2P30917samples,
12916zero-deltas,worst29,peak180.86%. Previous missed counts5302/13034 decrease
by132/118, but these are whole-game measurements with timing-sensitive phases,
not proof of exact causal speedup. Cache hit/miss totals match previous runs
in both modes; raw coarse workload comparison is archived with explicit limits.
Both60FPS gates still FAIL. F4/E2/D4/C6/main16/large3 reached, no course wrap.
Retained for equivalent semantics, reduced common-path loop overhead and modest
miss reduction in both runs. Both runtime processes finished; no emulator
frontend/window opened. ROM, symbols, CSVs, logs and source archived under
out/parity-audit/admission-compare-groups/runtime.

## Skip provably unused legacy range in readiness mapping (2026-10-22)

Ordinary recovered populations use dedicated main/C/D/E/F pools and fixed legacy
slots0..23 for G/H/K/shots/I. Generic direct allocation remains valid at24..47,
including Survival. Added a monotonic legacy_pool_used latch: allocation sets it
before activating a slot>=24; reset clears it only after clearing every active
slot and before Survival ambient spawning. No other source file writes entities.
When clear, readiness scans only0..23 and fills the unused24 entity bytes with0
and24 rank bytes with255 in aligned word stores. When set, the original full
scan remains in force until reset, even after direct actors die. No live actor
count, behavior, priority or allocation limit is reduced.

verify_sprite_readiness_binding passes100003 cases with alternating used/unused
legacy ranges, random retained fields and F pools; every input byte matches the
historical mapper. verify_legacy_pool_lifetime executes actual allocator/reset
prefixes for120050 range/free-slot/latch cases under ASan/UBSan. Full per-type
initializers are outside this fixture; source audit establishes they cannot
clear the monotonic latch. Front sprite chain8192 cases,32 player-capacity and
256 Survival differential cases pass. Build786432bytes.

ROM8ee77fbcffa753f6a5f9316c724a385d6da469854a1c85edc968c9b3ec6a4bc2
completes18000updates in each mode, F4/E2/D4/C6/main16/large3 reached.
1P23091samples/5092zero-presentation deltas,worst60window29,peak140.82%;
2P30313samples/12312zero-deltas,worst29,peak179.39%. Prior missed counts5170/12916
are lower by78/604, but1P workload/cache signatures differ due timing-sensitive
formation selection. Both60FPS assertions still FAIL; no causal whole-game
percentage speedup claimed. No course wraps; harness refills lives. Artifacts
archived under legacy-pool-lifetime/runtime. Both processes finished, no emulator
frontend/window opened. Legacy update/contact scans remain candidates only after
separate ownership and ordering validation.

## Empty legacy collision scans (2026-10-22)

Applied the proven legacy_pool_used lifetime to the reverse legacy-family list
builder in collision_pass. With no direct allocation since reset, slots24..47
are empty and the builder now does zero iterations; its six heads remain empty.
The subsequent native/G/H/I/large collision handlers keep their existing order.
Separately start ordinary generic player-contact scanning at16 rather than0:
slots0..15 were unconditionally excluded by the next predicate. Survival still
starts at0 and all generic upper bounds remain48, preserving later allocations.

verify_legacy_collision_scans compiles actual pre/post list-builder and contact
prefixes and compares full candidate sequences in100000 randomized valid-latch
cases under ASan/UBSan. This verifies scan equivalence, not every collision effect.
Build succeeds786432bytes. Updated direct-RAM target fixtures for generic shot
order and collision pages to set the new lifetime latch when bypassing allocator;
those MAME fixtures were not executed this turn. Large-damage fixture already
calls the actual allocator and needs no latch injection. No MAME/frontend opened.
Windowless18000-update gameplay measurements are being recorded separately.

Collision-scan runtime completed both18000-update runs. ROM SHA256
059bd0bf692bfe5b424d85aa8b8c0a8bc3934613a990d428bdfb09aa37986b0c.
1P23069samples/5070zero-presentation deltas,worst60window30,peak136.13%;
2P30217samples/12216zero-deltas,worst29,peak177.39%. Both60FPS gates still FAIL.
Compared with previous5092/12312 misses, reductions are22/96; no exact causal
percentage is claimed. All native pool peaks reached, no course wrap, lives
refilled. Archive includes ROM/symbols/source/logs/CSV and coarse per-update
workload comparisons at legacy-collision-scans/runtime. Both processes ended;
no emulator frontend/window opened. Performance remains substantially below
release requirements despite these bounded improvements.

## Pending-work gate for food/barrier rendering (2026-10-22)

Added render_pending to the food world owner. remember() sets it for food writes;
barrier pose changes set it when a map write remains queued. Begin clears it
before initial redraw collection. Renderer returns immediately when no work
exists, replacing two20-slot scans. On entry with work it clears the latch, then
retains it for queue capacity refusal, barrier upload refusal or partial map
transfer. Completed writes clear their original masks. Pool recycling can leave
a conservative stale pending flag, causing one harmless scan, but cannot lose
pending work. Immutable DMA source lifetimes and transfer order are unchanged.

Archived original owner as fixtures/food_world_before_dirty_gate.c and changed
the food-world oracle to use that independent pre-change source.80000 updates
match state and final nametable; native snapshots additionally assert that every
pending food/barrier mask implies the latch. Barrier upload is rejected in that
fixture. Dedicated verify_food_render_pending executes actual old/new renderers
for20000 sequences with barrier success/refusal, capacity refusal, partial DMA
failure, retry and idle. Write order/values, residual masks and eventual drain
match under ASan/UBSan. These are host transfer contracts, not target VRAM proof.
Build786432bytes. Windowless gameplay measurement is in progress separately.

Pending-render runtime completed18000updates in both modes, ROM SHA256
77c2c79bcc994084d33ffcf213e0fecb6f346137cdfb3a5950883a217acca5fe.
1P22601samples/4602zero-presentation deltas,worst60window29,peak137.70%;
2P29410samples/11409zero-deltas,worst30,peak175.39%. Previous missed counts
5070/12216 decrease by468/807. Both60FPS gates still FAIL. Runtime formations
remain timing-sensitive; no exact causal percentage claimed. F4/E2/D4/C6/main16/
large3 reached, no course wraps; lives refilled. ROM/symbols/source/reports/CSVs
archived under food-render-pending/runtime with hashes. Both processes ended;
no emulator frontend/window opened. Target-specific DMA/VRAM regression remains
pending despite host equivalence and successful runtime completion.

## Single flow snapshot for player target flags (2026-10-22)

Missed-refresh sampling repeatedly identifies game_player_target_flags. Its
old68000 fallback path computes the42-byte player address twice, once per flag
byte. Rewrote it to read flow once and construct the same active/kind flags;
owned death/reentry state still returns the original active/kind bytes directly.
No cached state survives the call, so player transitions cannot leave stale flags.
The emitted fallback contains one flow read/address calculation; saved before/
after disassembly is under player-target-flags. This is not a cycle measurement.

verify_player_target_flags compiles actual old/new function bodies and compares
33554432 fallback cases (both valid player indices, all player-count/flow/started
bytes) and262144 owned active/kind cases with ASan/UBSan. The fixture initially
needed its Player typedef fixed; the corrected test passes. ROM builds786432bytes.
Windowless18000-update1P/2P measurements are running separately. Full arcade
entry/life correctness is not established by equivalence to this prior adapter.

Player-flag runtime completed18000updates in both modes, ROM SHA256
2160fc143576c02bb50bdfd9f0ead3e2f8e1dc4f912a0561b4a26339367adcba.
1P22527samples/4528zero-presentation deltas,worst60window29,peak133.50%;
2P28973samples/10972zero-deltas,worst29,peak173.14%. Previous misses4602/11409
reduce by74/437.1P last-sample coarse signatures match all18000 indices;2P
population/cache signatures differ as refresh-dependent formations shift. No
causal overall percentage claim. Both60FPS gates still FAIL. All native pool
peaks reached, no course wrap, lives refilled. Archive player-target-flags/runtime
contains ROM/symbols/source/CSV/logs with hashes. Both processes finished; no
emulator frontend/window opened. Full timing alignment and substantial CPU
reduction remain required for release.

## Shared player flags within native enemy update (2026-10-22)

entity_update_all now snapshots both player target flags once at the start of its
ordinary native-pool block. Main/C/D/E/F updates only receive pool/counter pointers
and player coordinates/flags by value, and cannot mutate Player or life owners.
Tracking, E eligibility and G/H world-player context reuse these same two words.
Snapshot lifetime ends before player contacts, scheduler or future player updates;
Survival does not query these flags. This removes three of five repeated queries.

verify_enemy_target_snapshot compiles actual old/new production blocks with pool
call shims, checking target index/coordinates/eligibility and both world-player
contexts for200000 flag/position cases. Query counts are5 before and2 after;
Survival queries0. Pool mutation ownership was inspected separately in C source;
the shim test is not an independent arcade behavior oracle. Build786432bytes.
Windowless18000-update runs are pending. Previous interrupted turn only inspected
source; no orphaned runtime process remained when this work resumed.

Snapshot runtime completed18000updates per mode; ROM SHA256
231c2627bd9b2bbfbdbfa253d536b5d5deba6fc4608c4ee25cb17027d2ebd2d4.
1P22362samples/4363zero-presentation deltas,worst60window29,peak137.89%;
2P28600samples/10599zero-deltas,worst29,peak172.36%. Previous4528/10972 misses
reduce by165/373. Both60FPS gates still FAIL. All native pool peaks reached,
no course wrap, lives refilled. Cache hit/miss totals match prior runs, but full
state/refresh phase equality is not established. Archive enemy-target-snapshot/
runtime contains hashed ROM/symbols/source/reports/CSV/logs. Both processes ended;
no emulator frontend/window opened. Inspection found C/D/E/F pool wrappers still
call inactive actors whose first action is returning; moving those guards to the
caller is the next bounded CPU optimization candidate, not yet implemented.

## Skip zero-active native pool calls (2026-10-22)

C/D/E/F wrappers previously called every actor with full argument setup even
though each actor handler immediately returns for active0. Moved the same guard
to each wrapper while retaining the callee guard for other callers. Any nonzero
value, including128 hidden birth and255 hidden cleanup, still executes in slot
order. Live counts are not used as a shortcut because hidden births can precede
their live-count increment. No simulation update is skipped for an active actor.

All four real lifecycle fixtures pass512 cases each. verify_pool_idle_dispatch
compares actual old/new wrappers with actor observers across every active byte
and slot (C1536,D1024,E512,F1024), preserving ordered effects and confirming
only zero-active calls disappear. ASan/UBSan checks pass. Original wrappers are
retained as fixtures. ROM builds786432bytes, SHA256
71c3cc7f8467a67bcf747cfe1276b710c5c64537b4da025716600ee6884272c9.

Windowless18000-update runs complete:1P21571samples/3571zero-presentation deltas,
worst60window31,peak132.91%;2P27662samples/9662zero-deltas,worst30,peak170.26%.
Previous misses4363/10599 decrease by792/937;2P formations/cache statistics differ,
so no exact causal whole-game speedup claimed. Both60FPS gates still FAIL. All
native pool peaks reached, no course wrap; lives refilled. ROM/symbols/pool source/
reports/CSVs/logs archived under pool-idle-dispatch/runtime with hashes. Both
processes finished, no emulator frontend/window opened. Larger renderer costs,
source timing alignment and whole-game fidelity validation remain outstanding.

## E preparation visibility gate (2026-10-22)

entity_render_all skips E admission scanning/upload preparation when both actors
are inactive or have bit7 set. The real render request returns65535 for all such
actors; the previously corrected uploader returns success for an empty request
without changing residency. Therefore ready identities remain unchanged. Actor
updates are untouched, including hidden entry/cleanup work.

verify_e_prepare_visibility compiles the archived original and current production
blocks with real request/art lookups. All1572864 combinations pass: every pair
of active bytes, four E admission masks, dynamic/resident/invalid poses, upload
success/refusal, populated non-E admission lists and varied prior readiness.
Readiness and nonempty requests match under ASan/UBSan. Upload transport is a
shim; verify_e_upload separately passes no-op saturation and dirty refusal tests.
Front sprite chain and shared tail checks pass. Build786432bytes.

Windowless18000-update tests complete for both modes, ROM SHA256
 db71c6040b32bc3a3ee63ebeabf08002e8586979e5682ead06d1bd386ee08c03.
1P21413samples/3413zero-presentation deltas,worst60window33,peak129.39%;
2P27489samples/9489zero-deltas,worst29,peak168.80%. Previous misses3571/9662
fall by158/173, but worst2P window changes30 to29. Both60FPS gates still FAIL.
No causal percentage speedup claimed; timing-dependent formations remain a
confounder. All native pool peaks reached, no course wraps; lives refilled.
Artifacts/hashes saved under e-prepare-visibility/runtime. Processes finished;
no MAME or RetroArch frontend/window launched. Full-course fidelity, shared IRQ
clock alignment, target VRAM checks and substantial performance work remain.

## Fixed admission gather experiment — reverted (2026-10-22)

Missed-refresh PCs identify the inlined readiness mapper within entity_render_all.
Tested replacing main16/C6/D4 active-byte gather loops with fixed assignments.
Emitted68000 eliminates the loop branches, but uses long absolute operands and
some byte packing. Readiness differential coverage extended to every active byte
at each of26 slots:106659 total cases pass, as do front/shared-tail SAT tests.

Candidate ROM dbe3979d8bb67191719bb355d2c068f0b31123034d835a1752ee583abbcccecb
completed18000updates:1P3326misses,worst60window30,peak143.12%;2P9368misses,
worst30,peak168.36%. Baseline3413/9489misses,worst33/29,peak129.39/168.80%.
1P formation/cache statistics differ and first-to-last presentation deltas total
17998 (sampling boundary), not17999; requested simulation updates still18000.
Both modes fail60FPS. Improvement is mixed; reverted production mapper to its
exact pre-experiment source. Retained broader test coverage and candidate archive
fixed-admission-gather/runtime, including source, ROM, symbols, CSVs, reports,
assembly and hashes. No frontend/window launched; both processes finished.
Timing-dependent workloads prevent clean causal profiling. Source IRQ clock
alignment remains a priority alongside renderer cost reduction.

## Clock ownership dispatch audit (2026-10-22)

New audit_arcade_clock guards raw ROM vectors, instructions and indirect tables
instead of trusting linear disassembly of table bytes. IRQ mode02=2 dispatches
9636; state05=1 selects9646; state08=2 selects979D; state0B=0 selects97AB.
97AB calls9A2C,A5E2,E2B3,EF51,FED8 in that order. Therefore gameplay and the
wave scheduler descend from the same IRQ path after9112 increments23:24 and
after the3B-bit7 gate. This extends the earlier byte-selection finding to its
actual normal-gameplay caller chain.

A deliberately narrow interpreter executes guarded CC/D3/DD arithmetic bytes
for all65536 initial words, including carry and wrap. This is static opcode
semantics, NOT a full6809 or board execution trace. Counterexamples explicitly
assume additional delivered source IRQs while the native simulation advances
once; they establish the mixed-clock defect under that condition, not whether
masked source IRQs accumulate on real hardware. Native coherence is reported
FAIL, not hidden behind the audit script's successful report generation.

Current native reset/advance/callsite wiring is checked and source-file hashes
are attached to out/parity-audit/clock-ownership/results.json. A blanket vtimer
replacement would also jump Home gameplay phases after pause: game_run bypasses
updates during pause while VBlank continues. Clock unification must preserve that
intentional Home extension. No production timing change was made without the
remaining IRQ nesting/unmasking, boot/menu phase and cadence evidence. Existing
source traces inspected here are isolated subsystem fixtures and cannot supply
that missing full-clock lifecycle evidence. No emulator launched.

## Reference IRQ cadence and release gates (2026-10-22)

Pinned MAME0.288 sonson.cpp, base6x09.lst and m6809inl.h are archived with URLs
and SHA256s under clock-ownership/reference. Driver main IRQ is VBlank HOLD;
screen is55.40Hz. IRQ entry in base6x09.lst sets CC_I before vector dispatch.
This is emulator implementation evidence, not an oscilloscope measurement.
The55.4Hz rate was already noted near the start of this ledger but has not been
integrated into native pacing. Nominal60/55.4 gives about8.3% speedup if every
Genesis display refresh runs an arcade update. User was asked about this tradeoff;
default remains original gameplay speed with separate NTSC presentation timing.
No cadence change has yet been applied or claimed verified.

ROM guards extend the audit:412D reads DSW2, complements it and stores3B;
9130 bit7 is therefore the Freeze DIP, not the Service DIP. The bulk loader's
54CA ORCC10 and54E0 ANDCCEF are real instructions. Pointer41D4=54B0 sits in the
foreground command table41BE, dispatched by41B3 outside the IRQ. This is evidence
for foreground IRQ interleaving during loading, not proof of nested gameplay
IRQs. Linear disassembly ANDCC matches in map data do not establish executable
unmask paths. Full reachable-code/observed IRQ behavior remains open.

README now reports the retained ROM's actual failed performance gates rather
than obsolete near60FPS claims. ROADMAP begins with six explicit public-v1.0
milestones and exit evidence, with historical phases labeled as historical.
Timing, Home pause, source/input/audio cadence, interpolation and PAL are explicit
requirements; no documentation change substitutes for implementing them.
Audit script passes its source guards and reports native clock coherence FAIL.
No emulator process or frontend launched; production ROM remains unchanged.

## Unified main-wave counter carry (2026-10-22)

Removed the main-wave adapter's vtimer-derived high byte. Source23 and24 are
now wave_clock_high/wave_clock, advanced only by entity_arcade_clock_advance;
low-byte wrap carries into the high byte. Scheduler and results use that same
advance owner. Rendering and host refresh waits cannot independently alter the
formation selector high byte. The two byte symbols remain convenient for source
state probes. Removed schedule-reset's low-only reset; both bytes persist across
new sessions, and the course wrapper explicitly preserves both around reset.

This is a byte-ownership correction, not full source-clock integration. Current
counter still advances at the end of logical gameplay/results updates; original
IRQ increments before dispatch, including non-game scenes. Boot/menu elapsed
phase,55.4Hz source cadence and regional presentation pacing remain pending.
Home pause continues to bypass game_update; no wall-clock catch-up was added.

verify_main_refresh_byte now compiles the actual owner and callsite arguments:
all65536 states plus200000 consecutive advances pass carry/wrap/read-stability
under sanitizers. The initial fixture needed an unsigned comparison correction.
Course wrapper65536 cases now also check high-byte retention. The stale actor
retention fixture lacked the recently integrated native pools/renderer fields;
added explicitly stubbed declarations and checked that neither cold nor course
schedule reset changes the low clock. Its256 actor cases pass; stubs do not
prove pool internals. Results integration2048 cases and main-wave262144updates/
166134births also pass. Build786432bytes. Clock audit structural ownership now
passes while full lifecycle/cadence gate remains FAIL. Windowless runtime pending.

Shared-clock runtime completed18000updates each, ROM SHA256
3322724774a30ef688a0993ed005bc15c99cfd93762ee8c7e15f8b24d0a989ef.
1P21408samples/3408zero-presentation deltas,worst60window33,peak136.23%;
2P27173samples/9172zero-deltas,worst29,peak167.14%. Both60FPS gates fail.
2P population/cache signatures differ from the prior build, so this is a new
correctness workload, not an optimization comparison.2P sample-boundary deltas
sum18000 while requested simulation elapsed18000; no skipped updates inferred.
F4/E2/D4/C6/main16/large3 peaks reached, no course wraps; lives refilled.
ROM/symbols/source/scripts/CSV/logs archived with hashes under shared-clock/runtime.
Both processes finished; no emulator frontend/window opened. README now identifies
this retained baseline. Full clock timing and full-game parity remain incomplete.

## Advance the shared clock before gameplay dispatch (2026-10-22)

Moved the single logical counter advance to game_update entry, before results
or normal gameplay dispatch, matching the guarded source IRQ9112 ->9136 order.
Removed both scheduler-end and results-end advances. All clock consumers within
an update now observe the same value, including post-entity bookkeeping. Home
pause bypasses game_update; Survival retains its separate timer and does not
advance this arcade counter. Source boot/menu IRQ phase and55.4Hz cadence remain
unimplemented. This change does not claim full time-domain parity.

verify_clock_dispatch compiles the actual update prefix and real byte owner with
a results observer. All65536 words x8 normal/Survival/results combinations pass
(524288), including pre-dispatch observation and exactly-one advance. The initial
C fixture needed line formatting corrected to satisfy misleading-indentation
warnings. Pause bypass is guarded structurally, not claimed as a new target test.
Shared-clock65536-state/200000-step, results2048-case, reset65536-case and actor
retention256-case checks pass. Results fixture now advances at caller entry and
checks that the dispatcher does not advance again, including the restart edge.
Build786432bytes. Windowless1P/2P runtime measurements are pending separately.

The same dispatch observer rejects the archived pre-change game_update entry,
which has no pre-dispatch increment. Current524288 cases still pass.
Windowless18000-update runs complete, ROM SHA256
15807ab6f69cb5870d875cc9ebf427834514d8e9872ddf3b5a8db78146979b37.
1P21661samples/3661zero-presentation deltas,worst60window30,peak142.53%;
2P27356samples/9355zero-deltas,worst29,peak153.86%. Both60FPS gates fail.
Timing/order changes affect formations and sampled populations; these are new
correctness baselines, not speed comparisons. All native pool peaks reached,
no course wraps, lives refilled.2P presentation delta sum18000 reflects sample
boundaries with18000 requested simulation updates. Archive clock-dispatch/runtime
binds ROM, symbols, source, scripts, reports and CSVs with hashes. Processes ended;
no MAME or RetroArch frontend opened. Full cadence/menu-clock work remains.

## Cadence kernel and actual core timing (2026-10-22)

Windowless Genesis Plus GX AV metadata reports59.922743404312Hz and44100Hz
audio. This matches53693175/(3420*262). Probe archive core-timing-probe binds
core hash,61-update startup trace and metadata; its missed-presentation gate
fails and it is not a replacement for the18000-update baseline. Restored the
completed baseline reports after archiving the probe. verify_libretro now prints
AV metadata; verify_second_core parses it and reports elapsed emulated seconds,
observed simulation rate and presentation rate instead of assuming exact60Hz.
Clock constants checked against Genesis Plus GX core/system.h:
https://github.com/ekeeke/Genesis-Plus-GX/blob/master/core/system.h .

Added arcade_pacing kernel (not yet connected to game_run). It uses a rational
55.4Hz source accumulator over regional Genesis refresh periods, preserves
fractional time and pending simulation debt, and advances only the host anchor
during pause. A caller can consume a bounded number of updates without dropping
remaining debt. Modulo16-bit refresh differences require observing at least once
per65535 refreshes, including pause. Pending count is32-bit; no unbounded-duration
or unbounded-debt claim. NTSC/PAL math does not establish physical PAL support.

verify_arcade_pacing compares actual C with an independent64-bit total-time
calculation for320000 observations: both regions, varied host phases, wrap,
1/7/257-refresh gaps, pause, zero/two-update work budgets and eventual debt drain.
Sanitizers pass. Integration must preserve pending input edges, results/scene
boundaries, Home pause, separately serviced audio and honest repeated/interpolated
presentation accounting. Neither gameplay pacing nor60FPS is fixed by this
standalone kernel. No emulator frontend/window opened.

Kernel build succeeds786432bytes; unused kernel is removed by linking, and ROM
and symbols exactly match the clock-dispatch baseline. Updated metadata parser
verified in a second61-update probe:59.922743404312Hz,1.2182352785sampled seconds,
49.25156992observed simulation/presentation Hz for this short window.13misses;
acceptance still FAIL. Archived probe and restored full baseline reports. Both
probe processes ended. No current long-run performance improvement claimed.

## Initial gameplay cadence binding (2026-10-22)

Bound arcade_pacing to game_run for normal Arcade/Home play and results. Observe
actual vtimer refresh differences, preserve fractional55.4Hz time/debt, and
prepare a display even when no source update is due. Survival retains its prior
per-display timer/update path. Home pause advances only the refresh anchor and
discards queued button edges; it does not accrue new simulation debt.

Before enabling multiple updates per display, inspection found render-owned
entity_sprite_admission publishes next-update gameplay readiness. Therefore this
first binding consumes at most one update per prepared display and retains excess
debt. It cannot guarantee PAL source cadence or recover substantial overload.
Separating logical admission from render scheduling is required before bounded
multi-update catch-up is safe. This limitation is explicit, not a60FPS solution.
Display commits can repeat the same simulation state; no interpolation yet.

Input helpers preserve newly pressed bits between display sampling and source
consumption, with independent pad state and pause discard. game_update can still
read hardware directly when called outside game_run for isolated fixtures.
Multiple transitions between source polls coalesce; no claim of exact original
board input-phase matching. Attract duration counts source updates; immediate
button abort still checks each display. Audio callback is unchanged.

verify_gameplay_input_latch8192 cases pass transient/held/release/pause/controller
contracts. verify_game_pacing_loop compiles actual loop, cadence and helpers with
platform/gameplay shims:8scenarios x2000updates cover both regions, Survival,
pause/resume, host wrap and simulated stalls. Independent64-bit elapsed-time
oracle verifies all source time remains either consumed or pending. PAL/stall
cases deliberately retain backlog. Cadence320000 observations, clock dispatch
524288cases, shared counter checks and results2048cases pass. Build786432bytes.
Windowless runtime now records pending simulation debt as well as actual core
refresh and observed simulation/presentation rates;1P/2P measurement pending.

Cadence binding runtime completed18000updates each, ROM SHA256
8937c37f5a13cd5af0a33f03821476e60165d828d4671f360674e4f7209ed36c.
1P21834samples/3829zero-presentation deltas,worst60window29,peak143.55%;
observed simulation49.39997Hz,presentation49.41369Hz,pending2186(final/peak).
2P27746samples/9744zero-deltas,worst30,peak154.93%;simulation38.87365Hz,
presentation38.87797Hz,pending7651final/7655peak. Core59.922743404312Hz.
Both60FPS and sustained55.4Hz service are unachieved. Display-only iterations
explain presentations exceeding simulation count; they are not interpolation.
Pending work is retained, so overload is visible rather than silently dropped.
F4/E2/D4/C6/main16/large3 peaks reached; no course wraps; lives refilled. Full
source/native timing comparison remains pending. Archive game-pacing-loop/runtime
contains ROM/symbols/source/scripts/reports/CSV/logs with hashes. Processes ended;
no MAME or RetroArch frontend/window opened. Logical admission must be separated
from render ownership before multi-update catch-up, alongside substantial CPU
work reduction. README updated with current rates and growing debt.

## Bounded catch-up with logical admission handoff (2026-10-22)

Exposed entity_arcade_prepare_admission as a logic-only wrapper around the exact
mapper used by entity_render_all. game_run now consumes at most two due updates
per prepared display. Before a second normal step it publishes admission so the
next step sees the same spawn/shot/food readiness that a render between updates
would provide. The final render still uses the same mapper; no SAT/DMA operation
is needed for the intermediate handoff. Excess source-time debt is retained.

Do not batch through results/scene transitions: stop after any update that began
or ended with results active, or ended the game. This gives display/background
transfer work a boundary before further simulation. Survival still executes one
update per display. Attract duration now adds the actual number of source steps,
not just a boolean indicating whether this display updated. Input latch consumed
per step: first gets pending presses, later steps get held state.

Production game-loop fixture now checks the two-step cap, admission publication
before subsequent normal steps, and no further update after a results boundary.
Eight2000-update cases cover NTSC/PAL, pause, Survival and extreme100-refresh
stalls with independent elapsed-time/debt accounting. Readiness106659 cases now
exercise the public wrapper; front/shared-tail SAT regressions, clock dispatch
and input latch tests pass. No original full-game trajectory or target scene-
transition comparison is claimed by these host fixtures.

Benchmark reports sampling_limit_overshoot explicitly. With up to two updates
between refresh observations a fixed-tick request may end one update beyond its
limit; the raw trace is retained and acceptance permits only that bounded extra
update. Build786432bytes; windowless1P/2P measurement in progress.

### Bounded catch-up runtime result (2026-10-22)

Archived the completed run in `out/parity-audit/bounded-catchup/runtime`, including
ROM/symbols, sources, harness, reports, traces and SHA-256 artifact manifest.
ROM `627fbaebf718c92680d5cf6c670728c3674670ff37ac35510715c22317d5d1e9`:
18,001 observed simulation updates per mode (one-update sampling overshoot).
1P: 55.3985 simulation Hz, 43.4201 presentation Hz, 5,362 refresh intervals
without a presentation; worst 60-refresh window 20; pending source ticks peak
28/final 0. 2P: 51.7368 simulation Hz, 25.9230 presentation Hz, 11,829 intervals
without a presentation; worst window 18; pending ticks peak/final 1,274.
Both presentation gates fail; 2P also fails sustained source cadence. Average
1P source cadence does not prove even pacing. Lives were replenished and neither
workload completed a course. Scheduling changes affect the trajectory; these
figures are not a matched-state optimization A/B. No emulator frontend opened.

The next diagnostic builds in `out/subsystem-profile`, leaving production sources
and ROM untouched. Its SGDK getSubTick wrappers measure inclusive elapsed time
for simulation, rendering, entity updates, sprite preparation, catch-up admission
and VBlank service/wait. The timer is scanline-quantized and has a documented
VBlank rollback limitation. Nested probes and interrupt time are included;
VBlank includes intentional waiting. These measurements locate broad costs and
cannot establish exact CPU cycles, release performance or arcade fidelity.

### Isolated subsystem timing diagnosis (2026-10-22)

`tools/build_subsystem_profile.py` copies source/headers into an isolated build
and wraps complete calls (including early returns). The production ROM remains
`627fbaebf718c92680d5cf6c670728c3674670ff37ac35510715c22317d5d1e9`.
Two 3,000-requested-update opening workloads completed with expected presentation
gate failures. Final diagnostic ROM:
`220490efb2771fdcc8fa18309ac1ef975eedd4edd279fc40ae9e8ce3869da815`.
Evidence: `out/parity-audit/subsystem-profile-split`, with hash manifest, builder,
ROM/symbols/build manifest, raw logs/traces and scoped summary. Initial five-probe
plus VBlank run is retained separately in `subsystem-profile-first`.

Mean SGDK subticks per completed call (inclusive, quantized, NOT exact cycles):

| Probe | 1P | 2P |
|---|---:|---:|
| Simulation | 794.7 | 994.3 |
| Entity update | 638.3 | 734.1 |
| Collision | 154.0 | 306.2 |
| Scheduling | 83.5 | 89.3 |
| Render | 262.1 | 257.1 |
| Sprite preparation | 195.3 | 186.2 |
| Catch-up admission | 82.8 | 97.5 |
| VBlank service/wait | 557.8 | 713.6 |

Collision is approximately 31% of inclusive 2P simulation elapsed time in this
limited workload; entity updates are approximately 74%. Simulation is the larger
measured cost, so collision/player-contact and actor-update profiling takes
priority over further speculative renderer changes. Nested measurements must
not be summed with their parents. Counts cover calls since boot, not just sampled
gameplay, and refresh-boundary stopping can leave an outer call incomplete while
its inner call has completed. No E/D/C/main/large actors appeared in these short
runs; broader scene coverage is required before generalizing. Instrumentation
changes timing/trajectories and is not a performance improvement. Both processes
finished; no emulator frontend was opened.

Validation: isolated target build passed; actual game-loop host fixture passed
8 scenarios x 2,000 updates (cadence conservation, bounded catch-up, input latch,
pause, results boundary, intermediate admission, wrap and NTSC/PAL math).
Production ROM checksum unchanged after both diagnostic builds/runs.

### Contact fallback bound experiment — reverted (2026-10-22)

Profile-guided candidate limited the ordinary fallback player-contact scan to
slots16..23 while `legacy_pool_used` was false. The existing allocator sets that
latch before activating a slot24..47; reset clears it after clearing all actors.
The loop re-read the latch so an in-loop pickup allocation remained observable.
No source collision order, overlap or award rule was intentionally changed.

Validation passed: 100,000 actual scan-prefix candidate-order cases; 200,000
additional cases with in-loop allocation and final-state comparison; 106,659
readiness cases. The two existing target integration scripts passed 300 updates
and 1,500/900 checks respectively. Those scripts used MAME with `-video none`,
`-window -nomaximize -resolution 512x448 -noreadconfig`; no window opened and both
processes exited. Their scope is existing ordinary/K contact fixtures, not proof
of the new bound across every game state.

Candidate ROM `9b31e614127bfa1144d5f368c7fdf4ffe7af6468a3e1674c76c3fc30b8e887ac`.
Short 3,000-update same-input-policy runs: 1P presentations43.35→44.26Hz,
worst60-window23→24, peakCPU208.06→206.69%; 2P26.85→27.81Hz,
worst20→20, peak241.94→236.77%, final source debt135→42.
Long 18,000-requested-update runs: 1P43.42→43.19Hz, worst20→20,
peak242.24→238.38%; 2P25.92→27.03Hz, worst18→20,
peak287.60→258.15%, final debt1274→691. Long baseline completed18,001
updates; candidate18,000. Both gates still fail. Refresh-boundary execution and
catch-up phase can alter trajectories, so these are not matched-state cycle
benchmarks. Neither long run completed a course.

Decision: revert the candidate because the longer workload did not preserve
improvement in both modes. Retain fixtures, reproducible candidate verifier and
artifacts under `out/parity-audit/contact-scan-bound/{baseline,candidate,long-candidate}`.
`verify_contact_scan_bound.py` reconstructs the reverted bound explicitly.
Next diagnosis should separate collision's food/background, shot-family and
player-contact costs before further optimization. Do not repeat this loop-bound
candidate as an established win.

### Collision section diagnosis (2026-10-22)

Extended the isolated profiler with complete shot-family, player food/background,
shot background/large, and remaining player body/special/legacy sections.
Production source and ROM unchanged. The new reporter validates ROM identity
against build and runtime manifests, verifies probe completeness and retains
call counts alongside inclusive totals. Diagnostic ROM
`d8e6e7c3de08add986bebfe929fdd76847e353e04567134e774bae83a370897f`;
evidence and exact instrumented source archived in
`out/parity-audit/subsystem-profile-collision-sections` with SHA-256 manifest.

| Section | 1P calls / mean subticks | 2P calls / mean subticks |
|---|---:|---:|
| All collision | 3011 / 185.1 | 3011 / 352.9 |
| Shot families | 3011 / 46.0 | 3011 / 75.9 |
| Player food/background | 1505 / 45.1 | 3011 / 52.2 |
| Shot background/large | 1505 / 32.8 | 3011 / 31.3 |
| Player body/special/legacy | 1505 / 150.2 | 3011 / 157.8 |

The largest measured child section is player body/special/legacy. Much of the
2P collision increase is call frequency: the native source-clock dispatcher
alternates owners; 1P skips absent-P2 contact ticks while 2P has a player on both
phases. This must not be optimized away. Per-call costs are similar. The next
candidate is sharing immutable player-state reads across the five native contact
families, after food/background displacement, while retaining live death-request
checks, bonus effects and family ordering. Audit of the five bonus collectors
shows actor/award mutations rather than player lifecycle mutations; a production
change still requires differential tests and target timing evidence.

Scope: 3,000-requested-update opening runs, replenished lives, no full course,
no E/D/C/main/large populations. Both presentation gates fail as expected. Timer
probes perturb execution and include nested probe overhead/interrupts; SGDK's
VBlank timer limitation remains. These are broad diagnosis, not exact cycles,
release measurements, or full arcade parity. Both windowless Genesis Plus GX
harness processes exited; no MAME or RetroArch frontend launched in this step.

### Shared native-contact player state — retained (2026-10-22)

The five main/C/D/E/F contact families now share one immutable eligibility and
player-active snapshot taken after food/background displacement. Each family
still rechecks `deadReq`, preserving lethal-contact suppression and bonus ordering.
Contact targets and bonus collectors modify actors, score/difficulty awards and
hurt/death requests, not flow/invulnerability/lifecycle ownership. `snd_command`
only updates audio replay state. K/special/G/H/I/large and fallback contacts remain
outside this snapshot scope. No enemies, scans or source contact beats removed.

`verify_contact_player_snapshot.py` passed200,000 differential cases using the
actual before/after dispatch and all five real contact/bonus implementations.
It checks actor bytes, wrapped coordinates, score, combat difficulty, sound
sequence, player death/hurt and at most one target-state query. Lifecycle flags
are supplied by a stable query shim; this is not a full original-game oracle.
Target build passed. The emitted entity_update_all size fell from0x2A22 to0x28F8
(298 bytes); padded ROM remains786,432 bytes. The isolated profiler's section
start now precedes the outer eligibility guard, avoiding an uninitialized timer
on skipped contacts if profiling this production version.

ROM `bc91a1002c87dbadf80a6bf45355e6b7aa02a680007fcbf253b446df5c40654e`.
Both windowless Genesis Plus GX runs completed18,001 source updates:

| Metric | Before1P | After1P | Before2P | After2P |
|---|---:|---:|---:|---:|
| Presentation Hz | 43.4201 | 44.0511 | 25.9230 | 26.3389 |
| Simulation Hz | 55.3985 | 55.3985 | 51.7368 | 52.5203 |
| Worst60-refresh presentations | 20 | 20 | 18 | 19 |
| Peak CPU % | 242.24 | 240.72 | 287.60 | 282.62 |
| Final pending source ticks | 0 | 0 | 1274 | 986 |

Both presentation gates still FAIL;2P sustained source cadence still FAILS.
Lives were replenished, no full course completed, but all native main/C/D/E/F
and large-family population peaks appeared. These are identical simulation-input
policy workloads, not matched-state cycle traces; changing execution timing can
change trajectories. Retained because both measured modes improve without a
regression in the reported worst-window or peak-CPU metrics, with differential
behavior checks passing. Evidence, source and hashes archived in
`out/parity-audit/contact-player-snapshot/runtime`. No emulator windows opened;
both harness processes exited. Public-release gates remain open.

### Actor-update cost diagnosis (2026-10-22)

Extended isolated profiling to native-pool/context setup, the shared actor loop,
large actors, and food-carrier update. First diagnostic
`222efc54acb5659434444087c5e33f7429c22df57406b713f41f19e1669b81f8`
measured shared actor-loop averages306.5/239.0 subticks (1P/2P), versus
70.0/71.7 for native-pool context. Initial data retained under
`out/parity-audit/subsystem-profile-actor-sections`.

A second diagnostic split active entity_update_one calls by fixed slot family.
ROM `e2d8d4c405bd4b6a9b4379331ab5c560dcc720b9f8cb96eb63a389fc200ed7a8`:

| Adapter | 1P calls / mean subticks | 2P calls / mean subticks |
|---|---:|---:|
| G | 7210 / 38.3 | 2577 / 37.0 |
| H | 7078 / 35.8 | 5696 / 34.2 |
| Player shots | 1970 / 21.4 | 4051 / 21.2 |
| I | 4726 / 29.4 | 4251 / 28.6 |
| Other slots | 0 / not sampled | 0 / not sampled |

G/H combined dominate measured actor-call totals in these openings. Player-shot
adapter copying is not the primary target. Next experiment: compile fixed-family
G/H entry points from the existing shared state-machine body, inspect whether
constant propagation removes repeated family decisions, and require behavioral
comparison plus target runtime evidence before retaining any change. Do not
remove movement, floor probes, animation or contact phases to reduce cost.

All probes are diagnostic and include interrupts and timer quantization. Added
per-actor probes increased measured outer-loop times substantially (392.9/311.3),
so they must not be compared directly as release costs. Each run requested3000
updates with lives replenished and no full course; later native families are
not covered by these openings. Zero-call probes are now reported explicitly as
not sampled rather than implying zero cost. Exact instrumented sources, logs,
ROM, symbols, manifests and summaries archived with hashes in
`out/parity-audit/subsystem-profile-actor-families`.

Production ROM remains `bc91a1002c87dbadf80a6bf45355e6b7aa02a680007fcbf253b446df5c40654e`.
Both diagnostic builds passed and both player workloads completed; expected
presentation gates still fail. All harness processes exited; no emulator windows
or MAME/RetroArch frontends were opened. No gameplay change this diagnosis.

### Fixed-family G/H entry points — retained (2026-10-22)

Added G and H entry points calling a single always-inlined dispatch body with
constant family values. The original generic APIs remain available in source;
the gameplay adapter selects G for slots0..5 (including shared-K reservation)
and H for slots6..11. Bonus state255 ownership and the following entity mirror
remain unchanged. The compiler folds family branches in the entry points and
changes helper inlining; measured target execution, rather than assumed inline
benefit, determines retention. No movements, animation steps, floor queries,
weapon occupancy checks or update-order phases were removed.

Validation:
- Independently compiled pre-change source versus new G/H functions:200,000
  initial states x16 updates =3,200,000 updates, under ASan/UBSan. Actor/weapon
  bytes and floor callback order/arguments match. Inputs constrain pointer/index
  fields to valid ranges; this is not a claim about arbitrary corrupt states.
- Archived original-6809 oracle host comparison:19,200 cases,1,293,960 field
  checks,zero differences; traceSHA256
  `2483b5e43835ca0f67d49d71032eda42a803f7885d91bbf35ecbff2c2efaa83b`.
- Actual compiled G/H and weapon routines:19,200 cases,1,656,776 checks,zero
  errors. Target verifier now selects fixed-family entry points when present,
  retaining fallback support for old ROMs. Original floor-query arguments and
  collision pushes are replayed through a RAM callback. This is isolated source
  behavior coverage, not a complete-game comparison.
- Target build passed; linked text658,016,data108,BSS10,724 bytes are unchanged;
  padded cartridge remains786,432 bytes. Generic unused ROM entry points are
  removed by section GC; source API still builds for existing host oracles.

ROM `2ac963fb615becb48e49e0f573054dd3c257c20cdd98dce0b01abf27dfff2d0a`.
Both gameplay workloads completed18,001 updates with unchanged simulation-index
input policy and core. Compared with retained contact-snapshot baseline:

| Metric | Before1P | After1P | Before2P | After2P |
|---|---:|---:|---:|---:|
| Presentation Hz | 44.0511 | 45.1898 | 26.3389 | 26.4432 |
| Simulation Hz | 55.3985 | 55.3985 | 52.5203 | 52.7283 |
| Worst60-refresh presentations | 20 | 23 | 19 | 19 |
| Peak CPU % | 240.72 | 212.21 | 282.62 | 280.76 |
| Final pending source ticks | 0 | 0 | 986 | 912 |

Retained because both measured modes improve with no worsening in the reported
worst window/peak load, and source-derived target checks pass. Both presentation
gates still FAIL;2P still fails sustained source speed. Workloads refill lives
and do not complete a course; runtime timing can affect trajectories, so these
are not matched-state cycle benchmarks. Evidence, sources, old fixtures, scripts,
traces, ROM/symbols and target results archived with SHA-256 manifest under
`out/parity-audit/foe-family-specialization/runtime`.

The target fixture used MAME with video disabled, configuration loading disabled,
window/nomaximize flags and512x448 fallback resolution. It exited successfully;
no window opened. Both Genesis Plus GX workloads also exited. Release gates
remain open, including full-stage source comparisons and60FPS qualification.

### VBlank-arrival diagnosis — wait policy unchanged (2026-10-22)

Inspected installed SGDK sys.c: SYS_doVBlankProcess delegates to
ON_VBLANK_START; VDP_waitVBlank(TRUE) waits through an already-active blank and
then the next active display before processing. Replacing this with ON_VBLANK
without transfer-completion evidence would risk flushing DMA outside blank.

`build_vblank_entry_profile.py` creates an isolated build with only a boundary
histogram: raw V counter, sequential blank-flag read and queued DMA byte size
before the existing wait. No wait-policy change; production ROM is unchanged.
Each mode completed18,001 source updates under the usual replenished-life
workload. Counts since game_run entry (not just the harness sampling interval):

| Metric | 1P | 2P |
|---|---:|---:|
| Presentation boundaries | 14002 | 9039 |
| Blank-flag arrivals | 476 | 727 |
| Unambiguous early blank (raw224..228) | 5 | 9 |
| Maximum queued bytes, all boundaries | 1856 | 1552 |
| Maximum queued bytes, early blank | 304 | 504 |

Only3.4%/8.0% of instrumented boundaries arrived in blank. Early arrivals were
rare. Raw NTSC V counter229..234 occurs before and after rollback, so it cannot
be treated as an unambiguous remaining-transfer budget. These results do not
establish DMA completion time, tearing safety, callback cost or PAL behavior.
Counter overhead perturbs timing; no release-performance claim. Keep default
VBlank-start processing. Both diagnostic processes exited; no windows opened.
Exact source, ROM/symbols, raw histograms, workload traces and hash manifest are
archived under `out/parity-audit/vblank-entry-profile`.

Sound inspection also confirms snd_vblank runs in the V-Int callback, separately
from SYS_doVBlankProcess's user VBlank callback. ay1_to_ym and ay_1up_to_ym each
compute1845281/period and block shifts for every enabled channel each interrupt.
A next bounded experiment can replace this deterministic12-bit period conversion
with an exact ROM lookup while preserving every register value and write order.
It must be exhaustively checked over valid periods, compared at the register-call
boundary, and benchmarked with full audio; this observation alone is not a
measured audio bottleneck or permission to alter sound fidelity.

### Exact sound pitch lookup — retained with limited benefit (2026-10-22)

Moved the existing1845281/period, block-shift and saturation calculation offline
for all12-bit AY periods. Both AY-to-YM banks use the same4096-word ROM table;
period0 remains gated silent. Register addresses, values, order, volumes,
envelope handling, key state, replay commands, V-Int cadence and Z80 ownership
are unchanged. This preserves the existing conversion, not proof that the
existing AY-to-YM timbre is source-identical.

Validation: generator --check passed; all4095 nonzero conversions match the
original arithmetic. Actual old/new mapping routines passed2,297,152 ordered
register-write/final-key-state cases, including zero period, all volume/envelope
values, key combinations and mixed register patterns, under ASan/UBSan.
Linked-ROM check validates all4096 big-endian values, even alignment, source and
symbol identity, and flat cartridge bounds. Target disassembly replaces the
former __udivsi3 call sites inside snd_vblank with table references. These checks
do not measure bus-write timestamps or prove audible/hardware equivalence.

ROM `afafca48eb2af3d02866cf7d597bbd1316f13c21b3e4d00d8fc275554521db6b`.
Linked text grows8192 bytes (658016→666208); data108 and BSS10724 unchanged;
padded ROM remains786432 bytes. Full-audio workloads completed18001updates each:

| Metric | Before1P | After1P | Before2P | After2P |
|---|---:|---:|---:|---:|
| Presentation Hz | 45.1898 | 45.2729 | 26.4432 | 26.4562 |
| Simulation Hz | 55.3985 | 55.3985 | 52.7283 | 52.7541 |
| Worst60-refresh presentations | 23 | 23 | 19 | 19 |
| Peak CPU % | 212.21 | 211.62 | 280.76 | 281.20 |
| Final pending source ticks | 0 | 0 | 912 | 902 |

Explicit tradeoff: retain an exact constant-time pitch conversion at8KiB ROM
cost despite the0.44percentage-point 2P peak-load increase in this workload.
The means barely improve; no broad frame-time success claim is justified. The
removed divisions provide a local interrupt-work reduction, with ample flat-ROM
capacity and no added static RAM. Timing changes can change trajectories; the
workload is not a matched-state cycle benchmark. Both60FPS gates still fail,
2P still falls behind source cadence, lives were replenished, and no full course
was completed. Sound listening/bus timing and whole-game parity remain open.

Evidence, original mapping fixture, generated table, sources, scripts, disassembly,
ROM/symbols and traces archived with hashes under
`out/parity-audit/sound-pitch-lookup/runtime`. All windowless harness processes
exited. No MAME or RetroArch windows/frontends opened for this experiment.


## 2026-10-23 — Original attract input sequencer (integration pending)

The native demo still forces one player and supplies a synthetic autopilot.
Source signatures at9492/94B6/9A2C/9A4D/9A5F establish two scripted inputs,
delay+1 entry durations, previous-input copying before dispatch, and persistent
fire OR every eighth source-clock tick. The fixed native2400-update cap also
differs from the original phased30-count timeout.

A read-only natural MAME trace was collected previously using the verified local
arcade set, video disabled and explicit window/no-maximize flags. No RAM writes,
CPU redirects or injected inputs were used. The trace has9000 frame snapshots;
SHA256 e6857e84b47ff6a1c8b1c1897102fe42b48e5eb6373432c9bf13c556873af1cf.
Two complete main demos reach pointers9B34/9BC2. Thus9B14/9BB6 are table starts,
not stream terminators. The contiguous356-byte span9A6E..9BD1 includes the
adjacent prelude scripts; the next routine starts9BD2.

Added reproducible generator tools/gen_attract_data.py, native arcade_attract
state/step code and tools/verify_attract_natural.py. Persistent native state
matches5179 updates /41432 fields across two full demos and one partial,
including exit ticks. Tests compare input, previous input, delay and cursor for
both players against the independently captured original CPU. ASan/UBSan pass;
invalid reads are rejected atomically instead of inventing a loop. Generator
--check passes. Both C files compile with the actual68000 compiler/SGDK headers
and project-required -fno-builtin/resource includes. Initial manual compilation
omitted those SDK flags and failed; corrected commands passed. Target execution
has not been tested.

Evidence: out/parity-audit/attract-natural/trace.csv and
out/parity-audit/attract-sequencer/report.json. No emulator was launched for this
sequencer verification. The production game loop and retained ROM are unchanged.
Next integration must own prelude/setup, source clock phase, two-player world
initialization, timeout, coin/start interruptions and demo-sound policy. This
component pass does not establish full attract or whole-game parity.


## 2026-10-23 — Attract input adapter and phased timeout connected

Production game_init now selects two demo players and initializes the verified
shared sequencer. game_update steps it once after the source clock advances,
before either player consumes input. The adapter maps original fire/directions
into the existing pad boundary; the old synthetic autopilot is removed.
Production game_run now decrements a30-count timeout on source-clock multiples
of64 after each simulation update, replacing the fixed2400-update cap.

verify_attract_pad.py tests the extracted production adapter for both players
and all256 input bytes (512 checks), with installed SGDK button values and
ASan/UBSan. verify_game_pacing_loop.py passes the prior8 normal/Survival timing
scenarios plus512 attract timeout cases (all256 clock phases with/without
stalls). Those are platform/gameplay-shim tests, not target gameplay evidence.
The source byte audit now reports the adapter/timeout as connected while keeping
the full integration gate FAIL. bash build.sh passes:786432-byte ROM SHA256
5712dbcc707f5cda639d1a17c0280f42e90843612d4a45b7d52d0129d8f220de.
Sources, ROM, symbols and reports archived under
out/parity-audit/attract-integration with a hash manifest.

Remaining: the source's prelude and setup ticks are not yet represented in the
native lifecycle, so first-input/timeout phase still differs. Initial world state,
all-out handling, demo sound, and coin/start interruptions require source-backed
integration and target execution. Existing interruption behavior is unchanged.
No emulator was launched in this integration turn. Prior performance numbers
belong to the previous ROM; no performance improvement is claimed for this build.


## 2026-10-23 — Windowless target attract execution

verify_attract_runtime.py boots the current ROM through a standalone Genesis
Plus GX libretro harness, sends only controller Start presses through the initial
menus, then idles. It performs no RAM writes, life refills or CPU redirection.
ROM5712dbcc707f5cda639d1a17c0280f42e90843612d4a45b7d52d0129d8f220de
completed three attract entries/exits in10000 target refreshes, with both players.
All5760 checked script updates matched the original ROM bytes: both cursors,
inputs, previous inputs and delays. Results, generated harness, core/ROM/symbol
hashes and trace are in out/parity-audit/attract-runtime.

The initial observer compared arbitrary refresh-boundary samples and failed at
refresh2441: the tick counter had reached678 while the clock/script still
represented677. This is a mid-update snapshot, not evidence of a script fault.
The corrected observer accepts only PCs within linked VDP_waitVBlank/VDP_waitVSync
ranges;2556 snapshots outside those ranges were skipped. It still advances the
reference across every intervening update rather than dropping intervening work.
The coherent comparisons passed. Display-wait PC gating is part of this test's
contract and must remain explicit; arbitrary RAM snapshots are not atomic.

This proves target script execution and repeated return to title on this core,
not source lifecycle parity, graphics/audio matching, smooth60Hz or hardware
compatibility. No MAME or RetroArch frontend/window was opened. The standalone
harness unloaded the core and exited normally. Boot/prelude phase and demo
interruption/sound behavior remain open.


## 2026-10-23 — Demo-sound DIP command gate and target PCM

Source415D derives52 from inverted DSW1 bit20; source6347 admits ordinary
commands only if52 (demo sound enabled) or22 (live game) is nonzero. The1-up
wrapper6410 goes through this gate. Forced system commands6339 bypass it;
coin command6400 has its own live-game restriction. The native ordinary sound
path previously ignored g_config.demoSound entirely.

snd_command and snd_play_1up now suppress ordinary events during muted attract.
Shutdown still invokes replay_start directly and silences chips unconditionally.
No replay pitch, timing, register mapping or live-game command behavior changed.
verify_demo_sound_gate.py pins the source instruction bytes and tests extracted
production functions:1024 command cases and4 layered1-up cases, ASan/UBSan.
The build passes; ROM5380ef37e8355a24143120d43a31cfede751c2f2b04e09401e03b0cd2756c0c6,
786432bytes. Build/source manifest archived in out/parity-audit/demo-sound/build.

Windowless Genesis Plus GX attract regression passes5760 sequencer updates and
three complete entries/exits on this ROM. verify_demo_sound_runtime.py then
injects raw and decoded demo-DIP settings before attract starts, with controller
boot otherwise unchanged. After120 settling refreshes per demo, each run
measured8663576 PCM samples: OFF had0 nonzero samples/peak0; ON had8661470
nonzero samples/peak4995. Reports and harness in out/parity-audit/demo-sound/runtime.
This establishes the target output gate, not menu/DIP persistence, source audio
quality, physical sound output or the still-unimplemented forced coin/system
sound path. No emulator frontend/window opened; all harness processes exited.
Full arcade lifecycle and performance release gates remain open.


## 2026-10-23 — Preserve Arcade coin/Start across attract exit

The old game_run could exit on raw C before the next mode_input_poll counted
the coin. A Start edge observed in the demo was also discarded when the title
rearmed input, forcing another press. Arcade now checks demo exits immediately
after polling coins/Start, carries controller Start bits in g_attract_startPlayers,
and routes them through the same title purchase helper. Credit spending occurs
only there. Coin-only returns to title after crediting; Home retains button-to-menu
behavior. This implements the owner's per-controller console control contract;
it does not assert the original cabinet's complete start lifecycle is identical.

Production pacing-loop regression passes8 normal/Survival scenarios and512
attract timeout phases. Build passes. Windowless Genesis Plus GX passes5760
script updates across3 unattended demos. Controller-only verify_demo_start_runtime.py
passes4 scenarios: each controller sends Start with/without simultaneous C during
an active demo. Paid starts consume one coin and enable only the requested player;
unpaid starts leave the title idle (tick count constant for the final300 refreshes).
No RAM injection or life refill is used. ROM SHA256
2ce8c57c6b2f71fa03122b9ceb03b7785492f59454256ec99ae72272a673771a,
786432bytes. Evidence/build manifest:out/parity-audit/demo-start.

All harness processes exited; no emulator frontend/window opened. Simultaneous
both-controller DIP variants, source boot/prelude/clock ownership, visual/audio
fidelity and performance release gates remain open. No new performance claim.


## 2026-10-23 — Arcade Freeze dispatch gate

Original9112 increments23:24;412D reads/inverts DSW2 into3B;9130 skips mode
dispatch when inverted bit80 is set. Native game_update now checks the raw
DSW2 Freeze bit after the source clock increment and before script/gameplay/
results dispatch. Arcade Start handling and attract timeout also skip while
frozen. Coin polling remains before the gate. Home pause behavior is unchanged;
Home ignores the Arcade Freeze setting. The sound VInt callback is untouched.

verify_freeze_dispatch.py pins source bytes and exercises the real update prefix
for4096 DIP/mode/Survival/clock-carry cases with ASan/UBSan. Existing pacing tests
pass8 scenarios plus512 timeout phases. Build passes,786432bytes, ROM
9709fdac05a743d23cf402487e5e6d3eb90dabb5137aa24edfb1e80566e31611.
Windowless target attract regression passes5760 updates/three cycles.
verify_freeze_runtime.py injects only the Freeze DIP for400 target refreshes;
380 settled samples hold both players' coordinates, camera and all script state
constant while source clock traverses all256 byte values. Clearing Freeze
resumes script execution; the first demo exits at refresh4189 (ordinary3704).
Report and build manifest:out/parity-audit/freeze-runtime.

These establish gameplay-dispatch Freeze behavior, not complete source IRQ
ownership through boot/menu, menu accessibility while frozen, visual/audio
fidelity, physical hardware or performance. Runtime fixture uses explicit DIP
injection; it does not establish DIP UI/SRAM behavior. All processes exited,
with no emulator frontend/window opened. Full source lifecycle remains open.


## 2026-10-23 — G/H walking family specialization retained

Prior actor profiling identified G/H adapters as substantial actor-loop costs.
Current linked code still called a shared walking routine: actual symbol
size0x464 (1124 bytes), with runtime family branches despite fixed-family public
entrypoints. Marking only walking always_inline permits those constants to
propagate. No state ordering, active counts, animation frames or callback
semantics were edited. Generic host entrypoints remain available.

Fresh pre-change baseline ROM9709fdac05a743d23cf402487e5e6d3eb90dabb5137aa24edfb1e80566e31611
measured43.1339/26.4084 presentations/s for1P/2P (18001 source updates each),
worst60-refresh windows20/19, peakCPU237.5/280.615%, finaldebt0/937. This is the
post-lifecycle-fixes baseline; historical45.3/26.5 numbers belonged to an older
ROM and must not substitute for this comparison.

Candidate ROM68760145a5ba5c3829ef7d2212925d2505a4fefbc1cbb41dbbfe7ef97300d76f
measures45.4606/26.4652 presentations/s, worstwindows24/20,
peakCPU209.521/279.834%, finaldebt0/896. Simulation55.3985/52.7721Hz.
The2P change is small and still fails source cadence; both60FPS gates fail.
Lives are refilled and neither workload completes a course. Runtime trajectories
can change with pacing, so these are workload results, not matched-state cycle
measurements. Retained for improved means/minima/peakload in both modes.

Independent old-TU comparison passes3.2million updates with ASan/UBSan.
Archived originalMC6809 comparison passes19200 frames/1293960 fields,0differences
(traceSHA2483b5e43835ca0f67d49d71032eda42a803f7885d91bbf35ecbff2c2efaa83b).
Compiled target workload completes; no isolated68000 state fixture was rerun.
Shared walking symbol disappears. G/H entries become0x426/0x334bytes;
linked text667488,data108,BSS10732 unchanged and paddedROM remains786432bytes.
Baseline/candidate ROMs,symbols,sources,manifests and runtime traces archived in
out/parity-audit/foe-walking-inline. All windowless harness processes exited;
no MAME/RetroArch frontend/window opened. Further substantial2P work remains.


## 2026-10-23 — H/G/I contact pointer table rejected

Tested replacing repeated H/G/I slot-index arithmetic and Entity addressing with
a64-byte const pointer table, computing the numeric slot only for bonus awards.
Actor order and family early-exit rules stayed unchanged.100000 differential
cases passed with real body-overlap logic, deterministic bonus stubs and
ASan/UBSan. Candidate source is retained as a fixture; the verifier explicitly
uses that reverted candidate, not a self-comparison of production.

Candidate122da7cb4bf593bcf2ee02e0993902ac5043eaf0a2005e9053d4c259c04a1c00
completed18001 source updates per mode:1P44.1896 presentations/s,worstwindow19,
peak234.326%;2P26.3809,worst19,peak270.947%,finaldebt1047 (peak1048).
Baseline68760145... gave45.4606/26.4652,worst24/20,finaldebt0/896.
Despite256bytes less linked text and unchanged staticRAM, overall means and
worst windows regress and2P debt grows. Reverted the isolated loop edit;
rebuild restored exact baselineROM SHA256
68760145a5ba5c3829ef7d2212925d2505a4fefbc1cbb41dbbfe7ef97300d76f.
Restored its bound benchmark reports after archiving all candidate outputs.
Evidence:out/parity-audit/contact-pointer-order/{baseline,candidate}.

No performance improvement is retained from this experiment. Next measurement
should split player_body_special_legacy into its individual contact families,
special-object pass and legacy scan before choosing another structural change.
Source and target whole-game parity remain unproven; life-refill workload is not
a completed course. All windowless processes exited; no emulator window opened.


## 2026-10-23 — Detailed contact profiling, diagnostic only

Extended isolated subsystem profiler from21 to27 probes, splitting body contact
into main/C/D/E/F, K, special, H/G/I, large and legacy fallback regions. Original
statement order and early exits are preserved. Production sources/ROM unchanged.
Diagnostic4fe08d3b307f8b7e8840805d54ef0ce9b848cab3a568f2aa1f85cbf4d11c9a4d
ran both3000-update and18000-update workloads (sampling may overshoot by one).
Short profiles archived under out/parity-audit/contact-detail-profile/short;
long profiles, exact diagnostic source/ROM/symbols/harness results and hashes
under the sibling long directory.

Long workload mean subticks per contacted player,1P/2P:
main/C/D/E/F31.0/33.6;K10.2/10.1;special19.7/21.4;H/G/I37.3/35.0;
large8.9/9.4;legacy55.2/57.4. Calls9005/18010 reflect source alternating-player
cadence, not duplicate same-player work. Inclusive collision means211.1/410.4
per simulation update. Fallback traversal remains the largest individual body
section even with larger enemies/families reached in the long workload.

Timer probes include interrupts and nested measurement overhead, quantize to
scanlines and perturb timing/trajectories significantly. They are neither exact
cycles nor release FPS. No completed full course is claimed; lives refilled.
No legacy actor adapter was sampled, suggesting empty fallback work, but that is
not a proof that every fallback slot was always empty (e.g. transient objects).
Next candidate: a whole-loop fast path justified by allocation latch ownership
and reserved16..23 slot types, with dynamic allocation/Suvival regression tests.
The earlier bound-only candidate was rejected and should not be relabeled a win.
All windowless processes exited. Production ROM remains68760145...; no MAME or
RetroArch frontend/window opened.


## 2026-10-23 — Empty fallback contact traversal skipped (retained)

Detailed profiling showed empty legacy traversal was the largest individual body
contact section. Native general allocations start at24 and set legacy_pool_used
before activating the slot; the latch is monotonic until all actors are cleared.
Slots16..19 are reserved player shots, with no ET_SHOT case in the fallback
contact switch;20..23 are already excluded. Added one gate around the entire
fallback loop: run it for Survival or when legacy_pool_used is set. When set,
the exact old loop remains, including allocation/collection during traversal.
Gate is evaluated per contacted player, after preceding native contact effects.
This differs from the previously rejected bound-only experiment, which still
traversed16..23 and repeatedly tested its bound.

verify_contact_empty_skip.py passes200000 prefix/effect-order cases with
ASan/UBSan, valid reserved-shot/latch states, both modes and modeled in-loop
allocation. Original fallback switch has no player-shot case (asserted), and
allocation latch ordering is checked structurally. Test does not execute every
collision effect or establish complete source parity. Initial test compilation
caught misleading indentation in the generated harness; braced the test guard,
then reran successfully. Production build and target workloads passed execution;
60FPS assertions continue to fail as expected.

Baseline68760145... -> retained candidate
c8cc054ef73925770a6c23c6263d329fc0054ee2cff53a80db28a313122da9ed:
1P45.4606->45.5930 presentations/s, worst60window24->23,
peakCPU209.521->208.447%, peakdebt16->25, finaldebt0.
2P26.4652->29.7883 presentations/s,worstwindow20->20,
peakCPU279.834->263.574%,finaldebt896->241 (peak253).
Simulation1P55.3985Hz/2P54.6655Hz. Candidate18001/18000 updates; baseline18001
in both. Small overshoot difference is preserved, not trimmed. Lives refilled;
no full course. Whole-workload trajectories change with timing, so this is not
a matched-state cycle comparison. Explicitly retain the1P worst-window/backlog
tradeoff for substantial2P mean/debt improvement and slightly better1P mean;
neither source cadence nor60Hz presentation release gate is closed.

Linked text667488,data108,BSS10732 and paddedROM786432bytes unchanged.
Baseline/candidate hashes, source, ROM/symbols, manifests and runtime outputs are
in out/parity-audit/contact-empty-skip. All standalone windowless processes
exited; no MAME or RetroArch window/frontend opened.


## 2026-10-23 — Formation scheduler idle gate rejected

Measured scheduler cost motivated moving the G/H controller inactive check
before six-slot counting, and omitting live_count computation in birth states
where the real controller never reads it. No RNG calls or controller ordering
were changed.200000 actual old/new scheduler-adapter comparisons passed with
the real formation controller/RNG and an allocation shim (all actor/controller/
reservation/RNG bytes and allocation counts, ASan/UBSan).

Candidate4652a09b955c446e29012f3db605dc34a7df4c4fd970ca3fa0a4cc8eb9c8a114
improved1P45.5930->48.2546 presentations/s, but worsened2P29.7883->29.1119.
1P worst60window23->22,peakCPU208.447->211.719%,peakdebt25->15.
2P worstwindow20 unchanged,peak263.574->262.891%,finaldebt241->325.
Requested18000updates yielded18000/18001; life-refill workloads, no full course.
Timing changes trajectories; this is not a local matched-state cycle result.
Linked text/data/BSS and paddedROM size unchanged. Rejected for2P regression
and mixed pacing results, despite substantial1P mean improvement.

Archived baseline/candidate sources, ROM/symbols, manifests and runtime outputs
under out/parity-audit/formation-idle-gate. Reverted the isolated adapter edit;
the candidate remains an explicit fixture so the differential test cannot become
a self-comparison after reversion. No emulator frontend/window opened.

Rebuild restored exact production ROMc8cc054e... and its bound benchmark reports. All harness/build sessions exited.


## 2026-10-23 — Controller join regression and guarded contact profiling

verify_join_runtime.py passes controller-only cold boot, paid first start and
paid other-player join in both orders on production c8cc054e... . Held Start on
both controllers preserves the extra credit, does not pause, and advances game
ticks. Each new entrant responds left/right in coherent display-wait snapshots
while its flow is alive/respawning. The earlier first-player direction assertion
ran during FLOW_DYING, when movement is intentionally locked; it did not isolate
join responsiveness. Corrected misleading diagnostic column labels and added
both flow fields. No gameplay code changed, RAM writes, life refills or CPU
redirection. Report binds harness, both traces, ROM, symbols and core hashes in
out/parity-audit/join-runtime. This validates the requested console extension,
not source mid-game joining, first-player death input, or complete continue flow.

Fixed build_subsystem_profile.py probe25 insertion: inserting after the unbraced
fallback guard would guard the timing statement instead of the collision loop.
Probe now precedes the guard, and generation asserts guard/comment/loop remain
adjacent. Isolated diagnostic build passed; 3001/3000-update workloads completed,
with expected failing presentation assertions. Diagnostic ROM9d815cb5... .
2P inclusive means: collision365.3, actor loop303.4, scheduling90.4 subticks;
fallback contact6.0 per call. 1P fallback5.3 per call. Quantized inclusive probes
perturb timing and are not release FPS or exact cycle measurements. Short early
course workloads refill lives, sample no large actors, and complete no course.
Archive: out/parity-audit/contact-guard-profile. Next profiling/optimization focus
is remaining collision work and actor adaptation, with matched behavior checks.
Production ROM and release benchmark unchanged. All harness sessions finished;
no MAME or RetroArch frontend/window opened.


## 2026-10-23 — Specialized H/G/I overlap experiment rejected

After eligibility checks in H/G/I contact traversal, replaced two general
arcade_body_contact calls with their exact unsigned byte-Y/word-X bounds.
Upward-Y conversion cancels in the wrapped difference. Kept slot order, bonus
family skipping and all effects unchanged. verify_contact_inline_overlap.py
passes100000 actual loop comparisons (bonus award stub) and524288 exhaustive
axis-displacement/boundary checks under ASan/UBSan. Compiled target successfully;
no isolated target overlap fixture was run. Candidate remains a separate fixture
following reversion, so the test is not a self-comparison.

Candidatec7901ffcceb19495b02f8742d875c7e071637f5fbfd409413f6d8504e7cf68dc:
1P46.7064 presentations/s vs45.5930 baseline; worst60window24 vs23,
peakCPU207.227 vs208.447%,peakdebt19 vs25,final0.
2P29.6704 vs29.7883,worst19 vs20,peakCPU265.479 vs263.574%,
finaldebt372 vs241. Both candidate runs18000updates; original1P18001.
Simulation55.3983/54.2776Hz. Lives refilled; no completed course. Timings alter
trajectories; these are integrated workload results, not matched-state cycles.
Rejected for worse2P pacing/backlog despite1P improvement. Baseline/candidate
ROMs, symbols, manifests and sources are archived in
out/parity-audit/contact-inline-overlap, candidate runtime outputs alongside.
Rebuilt exact productionc8cc054e... and restored its bound reports. No frontend
opened. Next optimization should address a larger actor/collision data-flow cost,
not repeat this call-elimination experiment as a demonstrated speedup.


## 2026-10-23 — Empty actor-tail traversal experiment rejected

Added a slot24 boundary check to actor traversal: break only outside Survival
when the monotonic legacy allocation latch is still clear. This evaluates after
earlier actors, preserving same-update fallback births.9408 prefix/observer
cases pass under ASan/UBSan, checking modeled active update order, all birth
positions and slots, both modes and latch states. Existing120050 actual allocator/
reset-prefix checks also pass. Initial generated test compilation failed for
unbraced nested-loop indentation; braced those loops and reran. These tests
prove dispatch/latch behavior in their scope, not all actor effects.

Candidate8af4f40ff77f1aeec0c211f930559b6ad52de8b7b09e5487bf6702a3852f575b
built and completed18001/18000updates. Versus c8cc054e...:
1P mean45.5930->47.9720 presentations/s but worst60window23->20,
peakCPU208.447->228.760%,peakdebt25->23,final0.
2P mean29.7883->31.7507 but worst20->19,peak263.574->286.328%,
finaldebt241->299. Simulation55.3985/54.4942Hz. Lives refilled; no course
completion. Integrated trajectories differ with timing; no exact cycle claim.
Rejected because improved means came with worse worst-case pacing and2P debt.
Archived candidate/baseline manifests, ROMs, symbols, source and candidate runs
under out/parity-audit/actor-empty-tail. Explicit prefix fixtures preserve the
rejected differential test after reversion. Rebuilt exactc8cc054e... and restored
its bound performance reports. No MAME/RetroArch frontend opened.

The recent micro-optimizations repeatedly trade average presentation for timing
debt/stalls. Next step should establish matched-state whole-update timing and
check the scheduler/input relationship before selecting another local rewrite;
whole-run trajectory changes currently confound local cost attribution.


## 2026-10-23 — Consumed-input divergence measured; exact diagnostic replay

Investigated why local CPU changes alter integrated benchmark trajectories.
verify_libretro chooses script buttons using the current game counter, but the
frontend polls on refresh boundaries and game_run samples once before up to two
catch-up updates. Simulation-indexed script selection is not per-tick replay.

Added audit_consumed_input.py: builds isolated baseline/current-source and
archived actor-empty-tail candidate, records actual gameplay_pad at each live
game_update into a64-entry ring, drains it read-only after each core refresh,
and checks ring overrun plus contiguous ticks. Controller input and life-refill
behavior otherwise use existing harness. Both2P diagnostics recorded3011ticks;
91common ticks consumed different pads, first tick44 (baseline10/10,candidate
26/26). Both clock43. This is direct evidence of a workload confound, not proof
that a particular production gameplay divergence is caused solely by input.
Diagnostic timing itself is perturbed by recording.

The --tick-replay mode overrides pads only in diagnostic source copies, indexed
by exact source tick. Both builds again recorded3011ticks, with ZERO differing
pad ticks. Reports bind diagnostic ROM/symbol/harness/trace/core hashes under
out/parity-audit/consumed-input and consumed-input-replay. Does not yet prove
whole-state equivalence or matched-state CPU costs: render admission, lifetimes,
life-refill timing and other refresh-dependent effects still need comparison.

Production ROM remains exactc8cc054e... . Added honest input-delivery scope to
future verify_second_core reports and README. Integrated presentation results
remain valid for their workload but cannot alone attribute local CPU savings.
Next: use this replay to compare canonical per-tick gameplay state and locate
any residual divergence before using it for matched-state timing decisions.
All isolated runs completed; no MAME/RetroArch frontend/window opened.


## 2026-10-23 — Per-tick state replay and deterministic refill

Added audit_state_replay.py using isolated baseline/archived actor-tail candidate
builds, exact per-source-tick pads, and a34-word ring snapshot. Records clock,
camera, both players'XY/flow/lives/flowTimer/score and all12G/H actors'X/Xfraction/
Y/Yfraction/active/state/phase. Capture precedes current update after preceding
render/admission work. No ring loss; contiguous ticks checked. This deliberately
omits other enemy pools, RNG, animation cursors and many lifecycle globals;
selected-field equivalence is not full-state or arcade parity proof.

Short replay:3011common ticks, zero field differences.18000-update workload with
frontend life refills:18011common ticks, one difference at16115, P2lives99vs98
(flow and timer equal). Archived under state-replay/short and refresh-refill.
Moved life refill to diagnostic source-tick entry, disabling frontend refill:
18011common ticks now have ZERO pad or recorded-state differences. Baseline
captured18015records,candidate18011 because startup detection/sampling phase
varies; four unmatched baseline tail records are explicitly outside comparison.
Diagnostic ROMs63c0d76a.../837c6394... and all bound reports/traces live under
out/parity-audit/state-replay. Diagnostic inputs and refill are test mutations,
not normal playthroughs. Recording affects CPU load; no production FPS claim.

This establishes a repeatable selected-state comparison for the rejected tail
experiment, and identifies refresh-timed life refills as another confound.
Next extend canonical state coverage and measure equivalent workloads before
reevaluating optimization decisions. Do not restore a rejected production edit
solely from this partial equivalence result. Production remainsc8cc054e... .
All windowless harness sessions completed; no MAME/RetroArch frontend opened.


## 2026-10-23 — Broader pool replay and matched-interval diagnostic timing

Added audit_pool_state_replay.py. Extends exact-input/source-tick-refill replay
with1488-byte snapshots every64ticks, in a checked two-buffer mailbox. Copies
waves,RNG,G/H full records,weapons,world context,large,K,I,Iwave,main,C,D,E,F
pools,bonus cursors/limits,I population/reservation,shot budget and readiness.
Static size check enforces4096-byte per-buffer capacity; runtime checks overrun
and fixed snapshot lengths. Raw byte comparison includes padding. It does not
cover every global, food/background state, complete renderer or audio state.

2P18000-update diagnostic:281pool snapshots (418128bytes) match byte-for-byte,
and18011common per-tick selected player/G/H records match. Baseline has four
extra tail records, explicitly not compared. Both pools.csv hashes are
 e1e06c340fc54bb2715bd1499c327a1b8a99f47b451d38ccdc39a43a51c7bc6f.
Diagnostic ROMsbf4fe58e.../e207cedf...; full hashes/manifests/traces under
out/parity-audit/pool-state-replay. Source copies only; production unchanged.

For equal tick64->17984 (17920source intervals), baseline elapsed20704refresh
intervals; actor-tail candidate20114,590fewer (~2.85%). This is integrated
instrumented timing with refresh-quantized endpoints, including recording,
render/audio/waits. It is evidence of a benefit under the controlled workload,
NOT production FPS or exact CPU-cycle savings. Do not promote candidate solely
on this result: ordinary controller-driven worst-case/debt regressions remain
recorded, and broader deterministic/hardware/fidelity acceptance is open.

Production remains exactc8cc054e... . All harnesses finished; no emulator
frontend/window opened. Next useful decision: measure a minimally instrumented
exact replay in both player modes, while preserving independent normal-input
stress tests and the unresolved source lifecycle parity work.


## 2026-10-23 — Actor-tail skip retained after controlled replay measurements

Added benchmark_tick_replay.py: isolated baseline/candidate builds with exact
source-tick pads and life refill, no state ring/bulk snapshot overhead. Full
rendering/audio/pacing remain. Controller boot/start still drives startup;
recorded sampled tick ranges may overshoot by one. No complete course claim.
Diagnostic replay costs remain, so these are not production-controller FPS.

18000-update controlled results, baseline2af26bfb... -> candidate82af5861...:
1P47.3824->49.6555presentations/s,worst60window20->24,
peakCPU224.414->205.176%,peakdebt28->13,finaldebt2->0.
2P26.8413->28.9334,worst19->19,peak275.928->270.850%,
peakdebt602->258,final591->241. State-recording runs from preceding entries
matched18011selected ticks and281native-pool snapshots; this low-overhead build
omits that recorder, so do not claim full-state identity was observed in it.

Retain the actor-tail boundary guard on this combined evidence: exact traversal
observer/allocator invariants, broad sampled-state agreement, and better controlled
1P/2Pmeans/peaks/debt with no controlled worst-window regression. Explicit tradeoff:
normal frontend-sampled trajectories still produce worse worst windows23/20->20/19,
CPU peaks208.447/263.574->228.760/286.328%,2Pfinaldebt241->299. Their averages
improve45.5930/29.7883->47.9720/31.7507. These normal-input stalls remain release
blockers; input sampling confounds attribution but does not make stalls acceptable.

Production restored exact8af4f40ff77f1aeec0c211f930559b6ad52de8b7b09e5487bf6702a3852f575b.
Its archived normal-controller reports restored with hash checks. Re-ran9408
traversal cases and120050allocation/reset cases, and5760target attract updates
across3demos. Current traversal verifier now checks actual production prefix
against the tested fixture. README/ROADMAP updated to normal-input production
metrics48.0/31.8FPS. Benchmark/replay scripts explicitly select BOTH archived
entity.c variants now, preventing accidental self-comparison after adoption.
No claim of60FPS or full arcade parity; no emulator frontend/window opened.


## 2026-10-23 — Correct independent coin slots and coinage selector DIP

Source4134 initializes both slot ratios to1C/1C;4149..415D applies the selected
ratio only to A(rawDSW1bit4set) or B(clear).921C..9230 dispatches coin edges to
separate records30/35;9242 increments independent fractions31/36. Native had
one shared fraction and applied the configured ratio to both pads, contradicting
its COIN SLOT DIP UI. Fixed mode_controls.c to use coin_fraction[2] and apply
ratio to selected controller-slot only. Other slot stays1C/1C. Free-play table
threshold now255 as in source423A table (global free-play purchasing unchanged).

verify_coin_slots.py guards source instructions and original ratio table, then
runs8388608actual native poll cases against an independent ROM-table reference:
all rawDIPbytes,mixed/held C edges,Start edges,fraction state,and Home exclusion,
with ASan/UBSan. Existing99-credit saturation explicitly retained/modelled; source
uses a16-bit credit word, so this is NOT complete cabinet accounting parity.

Windowless target verify_coin_slots_runtime.py injects only2C/1C DIP/selector
on arcade title, then alternates four C pulses across pads. SelectedA credits
0,1,2,3;selectedB1,1,2,3. No direct credit/fraction writes. This tests both slot
selection directions, held-edge handling and independent fractions; UI DIP
interaction and other target ratios are outside coverage. Both join orders and
5760attract updates across3demos also pass on newROM85ab3fed... .

Re-ran18000-update normal-controller1P/2P workloads: unchanged47.9720/31.7507
presentations/s,worst20/19,CPUpeaks228.760/286.328%,finaldebt0/299. Expected60FPS
assertions fail; no complete course. Bound build/runtime archived under
out/parity-audit/coin-slots/build; target reports under coin-slots-runtime.

Coin audio remains open: source9208calls6400 once for any accepted slot edge,
before credit conversion;6400only emits01when live flag22iszero and bypasses
demo-sound gating via6339. Native title currently has no active replay engine,
and demo exit shuts it down, so wiring snd_command alone would be insufficient.
Future work must handle sound ownership across title/demo transitions and check
source credit width/overflow, rather than claiming the full coin path is fixed.
No MAME/RetroArch frontend/window opened; all test sessions completed.


## 2026-10-23 — Word credit counter and source decimal display semantics

Source924F..9255 adds the slot award to16-bit25:26 and skips the write on carry;
it resets the fractional count even when overflow rejects the award. Source
92CA..92CD purchases via signed word subtraction. Changed g_arcadeCredits to
volatileu16, preserving Home credits asu8; coin awards now reject word overflow
instead of clamping99. Updated title change detection, game-loop coin-change
snapshot, HUD cache and buffers to retain the full word. Explicit cache-valid
flags avoid65535colliding with an initial sentinel. Free-play purchasing still
bypasses debit, and owner-requested per-controller purchase behavior remains.

Source505C..506D passes the word to501F; that routine clears six cells, skips
leading zeroes and emits one zero for zero. New arcade_credit_text produces
left-aligned decimal text padded to six cells; title and gameplay HUD share it.
Native font/layout adaptation remains; this turn has no screenshot-based claim
of pixel-exact original credit placement. The title retains its existing
centering helper, now with a fixed full-width credit string.

verify_coin_slots.py:8388608poll cases,2097152word addition/purchase cases and
all65536format inputs pass under ASan/UBSan. Source instruction bytes/table are
pinned. Target8case test injects initial credit/DIP only, then uses controller C
edges: crosses99and255, reaches65535, and rejects seven-credit overflow awards
at65530rather than saturating. Both selected slots covered. Updated diagnostic
credit reads toword width. Both join orders,4demo paid/unpaid Start scenarios,
5760attract sequencer updates and pacing-loop tests pass. No claim of complete
coin debounce/sound lifecycle parity. Coin audio remains open.

ROM6e8ef9250d026d8cfc912a6c4cb0cb45d877fc2a4f14747e699a031622f0d271:
18001/18000normal-controller updates,47.9474/31.7507presentations/s,worst20/19,
CPUpeaks228.760/286.377%,finaldebt0/299.60FPSassertions still fail; lives refilled,
no complete course. Build/runtime/source archiveout/parity-audit/credit-word/build.
README/ROADMAP current identity updated. All standalone windowless sessions
completed; no MAME/RetroArch frontend/window opened.


## 2026-10-23 — Coin cue and arcade title audio ownership

Added per-poll coin-edge mask (cleared on poll/rearm), independent of credit
conversion. Both simultaneous slots cause one cue via the combined mask, matching
source9208..921A's single6400call before per-slot credit processing. Native title
now owns an initialized replay engine, shuts it down before entering demo/live
play, and reinitializes on demo return. snd_play_coin bypasses demo-sound gating
via direct replay command01; callers are arcade title or g_attract arcade game,
not live gameplay. Source6400suppresses when22isnonzero;6339bypasses52gate.

During demo, partial-credit coins play immediately without forcing title exit.
Credited coins follow the existing demo-to-title path; cleanup stops audio, so
title ownership restarts the cue. This is an explicit transition adaptation;
exact original queue timing and waveform are not claimed. Initially considered
exiting demo on any coin edge, but source review did not establish that rule;
kept the prior credit-change/Start exit condition before final build.

Host coin tests now verify edge masks alongside counts/fractions; sound test pins
6400/6339bytes and verifies coin bypass across all demo/live-DIP combinations
(the sound API requires nonlive caller ownership). Ordinary command/1up gates
and production pacing-loop tests pass. Windowless target PCM fixtures:
no-coin title silent; title cue DemoSoundOFF/ON39602nonzero samples,peak869;
credited-demo and partial-demo cues OFF39594nonzero,peak869. Before-cue windows
silent. Partial coin stays in demo; credited coin exits. Ordinary demoOFF still
zero across8663574samples; ONproduces output. These are output/gate tests, not
listening or original waveform fidelity. Attract5760updates, both joins, and
four paid/unpaid demoStart scenarios pass on current build.

ROM47a89d7bb3cc855475596fa78d4ed69b37091f574a838349fa34b3217d28152b:
18001/18000normal-controller updates;47.8982/31.7265presentations/s,worst20/19,
CPUpeaks228.857/286.377%,finaldebt0/299.60FPSassertions stillfail; no completed
course,livesrefilled. Bound artifacts undercoin-audio/build, PCM report under
coin-audio; README/ROADMAP identity/rounded rates updated. All sessions ended;
no MAME/RetroArch frontend/window opened.


## 2026-10-23 — Freeze DIP gates arcade title mode dispatch

Pinned source911E..913D: input,DIP,sound,coin processing precedes the Freeze
mode-dispatch gate; invertedDSW2bit7 freezes the mode handler, not coin handling.
Native arcade title lacked that gate. New windowless regression first failed
on47a89d7b...: paid Start spent the coin while frozen, and idle title entered a
demo during Freeze. Failing report/traces/ROM/symbols preserved in
out/parity-audit/title-freeze/before.

Added the gate after title coin polling/cue and before credit redraw,settle,
purchase,and idle progression. Onb148fe55... two target scenarios now pass:
1400refreshes frozen with a credited coin and Start pulse preserves credit and
idle title; a fresh Start after release buys the game. Without inputs, idle demo
waits during Freeze and resumes afterward. DIP injected; no UI/navigation claim.
Pinned source signature guards included in verifier. Normal target attract
5760updates/3demos, gameplay freeze380settled snapshots with clock advancing,
and title/demo/partial coin PCM cases also pass.

This does not close all frontend clock/input fidelity. The native source clock
still lacks complete title/boot lifecycle ownership. Source92B8 reads held Start
levels, whereas native title uses edge polling; held Start across freeze release
or before a later coin remains a follow-up audit. Current test deliberately
releases the frozen Start and uses a new edge afterward. No held-level parity
claim or full source lifecycle claim.

CurrentROMb148fe552d9c1f2af30fea90c197f799934c67c49c0d549eff365d9d5f88fc98.
Normal18001/18000update runs remain47.8982/31.7265presentations/s,worst20/19,
finaldebt0/299;60FPSassertionsfail. These life-refilled runs complete no course.
Bound build/reports under title-freeze/after. README/ROADMAP identity updated.
No MAME/RetroArch frontend/window opened; standalone sessions completed.


## 2026-10-23 — Arcade title uses held Start levels

Pinned source92B8..92BF reads current3Cstart levels; the no-credit gate92B0
returns before this and does not reset the frontend progression handler that
9296calls first. Native title consumed only new Start edges and reset its idle
timer on unpaid Start. Added mode_input_starts_held using the SAME sampled pads
saved by mode_input_poll; title now uses held levels. Coin edges, gameplay joins
and Home pause remain edge-driven. Removed unpaid-Start idle reset so a held
unpaid button cannot indefinitely suppress attract playback.

Before buildb148fe55... failed all4new controller cases: Start held before a
later coin, and Start held across Freeze release, on each pad. Failing evidence
preserved underheld-title-start/before. Final5db35ba0... passes all4 plus2unpaid
held-Start cases that enter demo with zero credits. Freeze still preserves credit
until release. This follows source held-level semantics while retaining requested
per-controller start selection, not original cabinet2P-start selection policy.

Host mixed-input8,388,608poll checks include the sampled held mask; word-credit
and formatting suites pass. On finalROM:5760attract updates/3cycles,6held-Start
cases,2titleFreeze scenarios,both join orders and4demoStart scenarios pass.
Boot clock/source lifecycle and original debounce remain open. Normal18001/
18000update performance unchanged47.8982/31.7265presentations/s,worst20/19,
finaldebt0/299;60FPSassertionsfail. No full course; livesrefilled.

FinalROM5db35ba0f444de90d3481170abe5a89886bbabcd651233763db6426d819c7bd9.
Bound build/source/reports underheld-title-start/after; README/ROADMAP identity
updated. All harness sessions completed; no emulator frontend/window opened.


## 2026-10-23 — Credited title lifecycle source contract

`tools/audit_title_lifecycle.py` verifies 16 signatures against the pinned
original ROM and writes `out/parity-audit/title-lifecycle/report.json`. This is
byte-pinned control-flow evidence, not original-CPU execution or native parity.

The 9296 dispatcher runs the current handler, then credit-change detection, then
held Start purchase. A credit change selects state 5. After queue setup, that
state counts down 240 handler calls and returns unconditionally to state 0.
Unchanged credits do not keep retriggering it. State 0 reseeds RNG 78:79 to 87CD,
then enters the prelude and demo. After the demo, state 4 runs two presentation
sequences with 240-call countdowns, then enters state 5 for credits/free play or
state 0 otherwise. Prelude and main demo timers use the source clock modulo 64.

Therefore suppressing demos whenever credits are nonzero would be incorrect.
The current 900-display-refresh title loop is also not equivalent. The next
implementation must cover the complete sequencer, source clock/RNG ownership,
prelude and queued presentations; merely replacing 900 with 240 is insufficient.
Queue visual meanings/completion timing and natural interrupted lifecycle traces
remain open. No production code or ROM changed and no emulator was launched.


## 2026-10-23 — Recover original front-end labels

The command dispatcher at 41BE maps 09 to 50E1 (individual ranking) and 0A
to 514F (total ranking); those handlers use text commands 08 and 0D before
reading dynamic score/name data. These are the two post-demo presentations.
`gen_arcade_title_art.py` now extracts commands 06/07/08/0A/0B/0C/0D directly
from the pinned source 47DA tables: credits/free play, both rankings, copyright,
deposit coin and press start. Original cells and palette attributes are retained.

Generated `arcade_title_art.c/.h`: 74 deduplicated tiles, 206 sparse cells,
7 command groups, 3234 raw data bytes. `verify_title_art.py` compiles the arrays
and checks all 206 cells against separately transcribed strings, addresses and
attributes using the established graphics decoder as pixel oracle. Host checks,
strict 68000/SGDK compilation and deterministic regeneration pass. This does
not independently revalidate the graphics decoder or establish live VDP output.

Assets are ready for the front-end rewrite but not yet referenced by title.c.
Dynamic ranking data, logos, sprites, clear/queue transitions, source clock and
lifecycle integration remain unfinished. No production ROM rebuild or emulator
launch occurred; previous runtime/performance results retain their prior scope.


## 2026-10-23 — Native six-state title sequencer

Added `arcade_title_flow.c/.h`, translating 933E..95D9 into native state updates
and ordered command events. State0 emits RNG reseed; prelude setup/player/decor
and ticking are explicit adapter actions. Demo initialization/ticking preserve
the source modulo64 timer. Both ranking screens retain their byte countdowns
(including preservation of the high timer byte); credited state uses a word
countdown and returns to state0 even when credits remain. Credit-change handling
follows state dispatch and appends its commands, as 9296/931E do. Maximum output
is eight commands. Start purchase and Freeze gating remain caller responsibilities.

`verify_title_flow.py` passes 197893 host calls: exhaustive 16-bit countdown
values, both byte countdown phases, all source clock phases, queue ordering,
busy/credit changes and credited/freeplay cycles. The source contract audit
passes and the module compiles with strict warnings for 68000/SGDK. Tests are
source-contract checks, not original-CPU differential or integrated target tests.

The production title loop is not switched yet. Integration still requires
continuous source clock and RNG ownership, actual prelude/world/player/decor
setup, dynamic ranking data and screen-command execution/acknowledgment. The
module must not be treated as a full parity pass until those effects are wired
and natural controller-driven cycles are compared. Existing ROM unchanged; no
emulator launched.


## 2026-10-23 — Title flow versus natural original-CPU trace

`verify_title_flow_natural.py` runs the persistent native sequencer against the
archived 9000-frame natural arcade capture (SHA256
e6857e84b47ff6a1c8b1c1897102fe42b48e5eb6373432c9bf13c556873af1cf).
It starts at observed mode1/state0 frame979 and advances once per source clock
update, without resetting native state from expected rows. All8021updates and
18859 state/front-end-phase/demo-timer comparisons agree, with ASan/UBSan.
Coverage includes two full uncredited cycles and a partial third: prelude, demo,
both ranking intervals and restart. The source prelude length changes with
clock phase; the native sequence matches without a fixed elapsed-time override.

The test supplies busy=0: archived rows do not record queue-busy, so this proves
the resulting sequence under immediate command completion, not queue execution
timing. Source08 during demo belongs to9646 and is deliberately excluded from
phase comparisons. Credits, actor/prelude state, graphics, target integration
and startup before frame979 remain outside this comparison. Original execution
was previously archived; this turn launched no emulator and changed no ROM.


## 2026-10-23 — Dynamic ranking map renderer

Added `arcade_title_map.c/.h` for original individual/total score boards.
Generator now extracts both five-row descriptor tables512C/51B5 and emits
palette-specific lookup tables for all256 raw name glyphs in each row. Source
4EBE/4EEA expands four packed score bytes into eight nibbles and blanks leading
zeroes except the final digit; native map preserves this even for non-decimal
nibbles. Individual names use four glyphs; total names use two four-glyph groups
with the original one-cell gap. No runtime font conversion is needed.

Static labels plus expanded ranking glyphs require1153 unique tiles and40362
raw bytes including lookup/descriptors/maps (previous74-tile report was static
labels only). This is a cold front-end asset set; final VRAM placement/load
ownership must be checked during integration. `verify_title_ranking.py` passes
512cases/524288cell checks, including all byte-valued names per row, positions,
palette pixels, score formatting and preservation of untouched cells. Static
label checks pass; art and map compile strictly for68000/SGDK; regeneration is
deterministic. Existing graphics decoder is reused, not independently proven.

Current save interface exposes only best scores, not the original five-entry
individual and total ranking tables with names. Native ranking data ownership,
qualification/insertion/name entry and compatible SRAM persistence remain to
implement. The map renderer is not connected to live title.c/VDP yet; production
ROM unchanged. No emulator launched.


## 2026-10-23 — Ranking qualification and insertion

Added `arcade_ranking.c/.h` with the original100-byte individual/total ranking
layout. Boot entries repeat original41E6/41EE records. Source9CBF qualifies
equal scores ahead of existing entries;9BFF inserts P1 before9C32 inserts P2.
Source9C6D adjusts the recorded P1 row ONLY when it equals the new P2 row,
clearing P1 qualification if that row is4. This can differ from the usual
"increment any displaced P1 row" policy and is preserved deliberately. It also
compares row metadata without first checking P1 qualification. Single-player
9C63 skips P2 and total-table insertion. New names become blank tile16.

`verify_arcade_ranking.py` pins the original compare/metadata/single-player
branches, checks boot bytes against ROM, and compares table changes with an
independent stable descending-sort oracle. All22326cases/2232600table-byte
checks pass for ascending input pairs across descending/equal/zero tables in
both player counts. Metadata checks retain the explicit source equality rule.
Strict68000/SGDK compilation passes. This is source-contract validation, not
original-CPU differential execution or live gameplay integration.

Transient name-entry records, input/timeout logic, live game-over ownership,
full table display binding and SRAM schema migration remain open. The native
module is not called by game.c yet. ROM unchanged; no emulator launched.


## 2026-10-23 — Name selection and idle timeout

Added `arcade_name_entry.c/.h`: original9F37 initialization (including retained
repeat-timer bytes),9F75 fresh-fire selection, BACK/END behavior, A056/A0B1/
A108/A161 independent directional repeats, and9FF9's selector-hole correction.
The latter compares remainder15 directly, but remainder1 after DECB; treating
it as remainder0 would corrupt horizontal wrapping. Four name glyphs start
at3F, not ordinary space16. The original A214 glyph table is byte-checked.

Idle state follows9E37: when directional inputs cease, set a16-bit deadline
clock+0D48. Direction bits3C reset the wait; fire alone does not. Deadline
equality advances to phase2; the following call requests completion. Both
controller activity is used only for a two-player session.

239736 source-contract checks pass for initialization retention, choices and
name slots, BACK/END, held fire, repeats/wrap, all16-bit deadline phases and
controller masks. Added9EDC name commit to ranking tables;1000 qualification/
row combinations preserve score bytes and unrelated rows, with P2 writing
after P1 and total names concatenated. Existing22326ranking cases still pass.
Both modules compile strictly for68000/SGDK.

These are host/source-contract results, not original-CPU differential tests.
Name-entry screen/sprites, two-player participation orchestration, game-over
integration and SRAM migration remain open. The live ROM is unchanged and no
emulator was launched.


## 2026-10-23 — Version2 ranking SRAM payload

Production save codec now stores100 ranking bytes at337..436 within existing
512-byte slots; version2 payload length425. Header offsets, CRC507..510,
commit511 and generation comparison are unchanged. Version1 length325 loads
all prior settings/records and initializes the absent rankings to original
defaults. Loading performs no write; a later successful commit migrates the
alternate slot. Unknown versions above2 remain locked against writes.

Added save_get_rankings/save_set_rankings for finalized ranking state; setters
mark dirty, leaving commit timing to the scene owner. Reset options preserves
rankings; clear scores restores ranking defaults. Host ASan/UBSan:1036codec
checks and1034transaction checks pass, including every interrupted write on
both slots, old-save loading/migration, absent/read-only SRAM, reset isolation
and newer schema preservation. The transaction harness now verifies ranking
bytes alongside settings at every interruption boundary.

Built ROM3cc32eeab771cbf90e62bf1641e30eef0abfb22112b2ce5593beddc2636279bc,
786432bytes. Windowless attract test passes5760source updates/3cycles. Normal
18001/18000update performance remains47.8982/31.7265presentations/s, worst20/19,
finaldebt0/299: both60FPSassertionsfail. Life-refilled workloads complete no
course. Bound artifacts and host summary under ranking-save/build and report.

New ranking/name-entry gameplay callers and front-end flow are still unbound;
this is persistence infrastructure, not a completed ranking user flow. Target
cross-process ranking-save checks and physical SRAM tests remain open. No
emulator frontend/window opened; standalone harness sessions completed.


## 2026-10-23 — Arcade returns to attract after game over

The outer main loop incorrectly showed the opening ARCADE/HOME selector and
DIP screen after every Arcade game. Source97ED resets arcade mode dispatch,
and9258 re-enters attract without consuming credits or asking for setup. Added
an Arcade title/game inner loop retaining current DIP settings and credits.
Home's existing return path stays outside that inner loop; reset returns to
the opening selector. This fixes the return destination only, not the missing
post-game ranking/name sequence or the existing continue-delay semantics.

Windowless `verify_arcade_return.py` fails on prior3cc32eea... and passes on
830e709fab0dd7057b873444030e0d2c8542bdb8b36c28b2dc0ee96241bac7f1.
It starts via controller, inserts a spare coin, injects both player flows/lives
and one remaining game-over beat at refresh2000, then presses Start2200.
At2100credit1 remains; at2400new run has3lives,credit0 and advancing ticks.
Old ROM remains at0lives/credit1 in configuration. This injected exit fixture
is not natural final-death or complete lifecycle proof. Before/after reports
are archived; previous ROM is also retained under ranking-save/build.

Current attract smoke passes5760source updates/3cycles. Full18001/18000update
benchmarks remain47.8982/31.7265presentations/s,worst20/19,finaldebt0/299.
Both60FPSgates stillfail; no full-course coverage, lives refilled. New build
manifest/ROM/symbols/performance/source bound under arcade-return/after/build.
No emulator frontend/window opened; all standalone sessions completed.


## 2026-10-23 — Name-entry grid and player cursor graphics

`gen_name_art.py` extracts A1F0's five17-cell grid rows and text commands
0217/0218/0219 (instruction and player labels). It bakes fixed sprite codes
56/57 in source palettesC3/C0 from9FF9, including the inverted source flip-bit
convention via existing arcade_cell_sat. The complete set is89tiles, fitting
the existing132-tile bonus scene arena;116static cells plus64dynamic glyph
lookups support all names the source input table can produce. No ASCII-only
substitution or new cursor artwork is used.

`arcade_name_map.c/.h` composes the screen while preserving HUD rows0..4 and
row31, places new letters at original two-cell-spaced positions and emits
cursor sprites from corrected selectors. The caller retains viewport and tile
base ownership. `verify_name_art.py` passes5058checks covering grid/labels,
all accepted glyph values and name slots, both player sprites, every valid
selector across three viewports, completed-player cursor suppression and
untouched cells. Existing decoder reused; no independent graphics-decoder
claim. Strict68000compilation and deterministic regeneration pass.

VDP upload ownership and live post-game session integration remain pending.
The production ROM was not rebuilt in this step. No emulator launched.


## 2026-10-23 — Live post-game ranking and save validation

Build f5129807d16b96e5adeaed410ac6ae15ed8aca1fb9972910009dc53cd86c873f
connects qualification, original name grid/cursors, individual/total boards
and SRAM ranking writes to Arcade game-over. Start hands off to the existing
controller-specific purchase owner. Home remains outside this scene.

`verify_postgame_runtime.py` passes four target scenarios: 1P AAAA entry,
paid Start interruption, 2P AAAA/AAAA with scores 50000/60000 and total 110000,
and a new process loading the 2P battery image before another ranked run.
It checks v2 headers, committed-slot CRC, exact serialized score/name bytes,
loaded status and generation advancing to 2 while retaining prior rankings.
The fixture injects final-death state and scores; it does not prove natural
reachability or complete original-CPU scene parity. No frontend was launched.

Name-entry, ranking, name-art and ranking-map host checks pass. Current-ROM
18001/18000-update benchmark still yields 47.8982/31.7265 presentations/s,
worst 60-refresh counts 20/19 and final simulation debt 0/299. Both performance
assertions fail; lives are refilled and neither course is completed. Build,
sources and reports are archived under postgame-runtime/build.

Remaining scene limitations: original qualification/setup substates are
collapsed; drawing-queue timing and the existing 600-tick continue delay are
not source-verified; total-score accumulator equivalence needs independent
confirmation; simultaneous completion/idle-timeout behavior and Freeze still
need integrated comparison. Full title lifecycle is still unbound. These
checks do not close the arcade-parity or release gates.


## 2026-10-23 — Post-game timeout and original timing contracts

`audit_postgame_lifecycle.py` pins ten original ROM signatures: handler/Start
ordering, independent completion and idle increments, both 180-call ranking
holds and countdowns, and the packed-decimal combined-score routine at4D5A
feeding9CD5. This confirms arithmetic intent, not total recomputation timing
across continues or binary score wrapping throughout the native game.

Extended `verify_postgame_runtime.py` passes five scenarios on unchanged
f5129807... ROM. The new unattended path enters names at refresh2003,
individual ranking5687, total5882 and returns6076; saved names are four3F
bytes, matching untouched initialized name slots, and CRC passes. The original
3400-source-tick idle timeout completes on target without controller activity.
Score/death are still injected. Exact setup timing, simultaneous completion
and timeout, natural reachability and Freeze coverage remain open.

No production code changed in this verification step; the retained benchmark
still applies to the identical ROM. No frontend/window was launched.


## 2026-10-23 — Prelude input initialization recovered

Added `arcade_attract_prelude_init`, matching93EB..93F7: cursors9BB6/9BC4,
zero delays, retained current/previous input bytes. The existing shared source
stream decoder handles the prelude without invented repeat/end rules.
`verify_prelude_natural.py` compares persistent native state with the archived
natural original-CPU trace: three cycles,1834source updates,14696field checks,
including initialization and exit ticks. State is seeded from the observed
incoming state once per cycle, not overwritten per update. ASan/UBSan pass.
Main-demo regression remains5179updates/41432fields. Strict68000compile passes
with SGDK headers and production -fno-builtin; initial standalone invocations
omitted required target header/build flags and were corrected.

This initializer is not yet bound to production title flow. Prelude actors,
world/render setup and full clock/RNG ownership remain integration work. ROM
was not rebuilt; its archived performance measurements remain historical to
that unchanged binary. No emulator frontend was opened.


## 2026-10-23 — Prelude-specific player motion record

Recovered940A differs from gameplayAA16: title left/right magnitudes are0100,
while vertical values remain02C0/0240. Added arcade_player_step_prelude with
zero camera and stopped-section semantics, sharing native movement logic with
the gameplay wrapper. Prelude setup/call order and rendering remain unbound.

`verify_prelude_motion.py` pins the16original motion-record bytes and passes
768horizontal/fraction/no-idle-drift cases plus20000deterministic gameplay
comparisons against the archived pre-change implementation. This is a host
regression, not original-CPU prelude actor-state evidence. Strict68000compile
passes. Shared function signatures/code generation changed; no performance
claim transfers to the next rebuilt ROM until remeasured. The current ROM was
not rebuilt in this step. No emulator frontend opened.

B19A inspection also identifies the two title decorations as shared bonus
kinds8/9, initialized through B1DA then held at state2, x005C/0090,yB4.
Their generated B291 poses already exist; live prelude placement remains work.


## 2026-10-23 — Rebuilt prelude movement support and target regression

Current ROM a840997ad353b02ef0b685ea42931a53e732821c120c8600d0dbc315119b27f9
builds at786432bytes. Native prelude entry remains unbound to the title loop.
Converted verify_player_stopped_target.py from a MAME launch to a standalone
Genesis Plus GX diagnostic harness (`genplus_probe.py`); the actual68000
cartridge passes all1280original stopped two-player motion/animation observations.
The diagnostic ROM is separate and the production ROM hash is checked unchanged.
Attract target5760updates/3cycles and all5post-game scenarios pass on this build.

Current18001/18000-update workloads:47.9597/33.2329presentations/s,
simulation55.3985/54.8016Hz,worst60-refresh counts20/20,finaldebt0/196.
Both60FPSassertions fail. The previous build measured47.8982/31.7265; this is
not a controlled speedup claim because consumed inputs/trajectories may diverge.
Lives refilled, no completed courses. Source/build/results archived under
prelude-motion/build. No MAME or RetroArch frontend/window was launched.


## 2026-10-23 — Prelude actor ownership

Added arcade_prelude.c/.h to own the two players, animations, input streams and
two fixed decorations. Source93BA setup sets active/state/phase, x001C/00D0,
y88 and retained input history, executes player initialization once, then uses
the940A prelude motion entry for subsequent updates. P1 runs before P2 and
separation follows both. Source941A decorations use existing B291 kind8/9
first poses at x005C/0090,yB4; positive-kind B23D does not animate them.
World/session initialization remains a caller prerequisite; this module does
not invent a reset for retained fractional/animation fields.

verify_prelude_setup.py pins four source signatures and passes all256fraction
cases,10240scripted actor updates and an atomic input-boundary failure case.
Checks include source placements, initialized animation codes, fixed decor,
first40inward-motion ticks and preserved fractions. Synthetic supporting
floor is explicit; no real-terrain, shot/collision, complete source actor trace
or live title integration claim. ASan/UBSan and strict68000compile pass.
Production ROM unchanged; full scene presentation and title-loop binding remain.
No emulator frontend/window opened.


## 2026-10-23 — Prelude SAT composition

Added arcade_prelude_render.c/.h: source order P1/P2 then both decorations,
existing resident life/bonus art, original inverted flip attributes and FF4A
coordinate conversion (X low byte at camera0, Y=-worldY-16). Source FF4A also
skips zero or negative active flags; this is pinned and tested. Four cells use
up to12wrap entries staged in96stack bytes. Invalid art/capacity returnsFFFF
without partial writes; caller owns VRAM loads and final link termination.

verify_prelude_render.py passes42624scene combinations across positions,
activity masks and three viewports, capacity boundaries/canaries, invalid poses
and all128negative active values. ASan/UBSan and strict68000compile pass.
Existing artwork/converter are reused; no new pixel oracle or live VDP claim.
Scene binding, background/terrain, interactions and original captures remain.
Production ROM unchanged. No emulator frontend/window opened.


## 2026-10-23 — Prelude terrain callback

Added gen_prelude_terrain.py and arcade_prelude_terrain.c/.h/data.c. Source935F
selects first6485descriptor;907D copies128bytes from825B into the collision
ring. Native immutable data reproduces that map. A847 probe preserves low-byte
X wrapping, ring offset, upward Y bit lookup and signed7-(Y&15) push-out.
The source copy does not reset8A, so the callback requires an explicit ring
context rather than assuming0 on every attract cycle. Ring lifecycle is still
owned by pending world/title integration.

verify_prelude_terrain.py passes4194304probe comparisons across all32rings,
allX/Ybytes and high-word-X aliases against the original bitmap. A further
20480native prelude updates exercise640ticks per phase with the real floor
callback. ASan/UBSan and strict68000compile pass. This is bitmap/operation
verification and smoke coverage, not a full original actor trace or live scene
comparison. Shots/collisions/render ownership remain unfinished. Production
ROM unchanged; no emulator frontend/window opened.


## 2026-10-23 — Prelude collection and score poses

Prelude decorations now use the existing ArcadePowerup state and B19A pickup
step rather than a fixed-pose-only record. Initialization executes the shared
first-pose routine before selecting state2 and source positions. Subsequent
scene updates run pickups after both players/separation. A separate collect
pass matches E7D6/E7DF P1/P2 then slot order, accepting nonzero player flags
but only active1 pickups. It returns ordered reward events and carries the
shared combat/count/FB95 request state explicitly; caller must gate title
score writes via live-game semantics. SAT composition resolves current code
and owner palette, including collected score poses.

verify_prelude_collect.py passes418geometry/owner cases,255nonzero player-active
priority cases, shared count wrap/request and both owners'96-update score
lifetimes, including next-tick pose change and owner-specific SAT tiles.
ASan/UBSan pass. Setup, terrain and42624scene render regressions plus strict
68000compilation pass after adapting to shared pickup fields. No live scene
or complete original actor-state comparison claimed. Source ordering still
requires caller to collect after the scene update; shots and full frontend
binding remain open. Production ROM unchanged; no frontend/window launched.


## 2026-10-23 — Prelude firing contract and isolated target display

Source93CF..93D8 clears both shot availability bytes; AD10 returns immediately
on zero. 9463 calls no shot update/render owner. No prelude projectile pipeline
should be added despite auto-fire bits in scripted input. These three additional
signatures now pass audit_title_lifecycle.py (19total). Earlier references to
pending prelude shots are superseded by this recovered disabled-fire contract.

Added arcade_prelude_display.c/.h with cold VRAM/palette ownership and synchronous
SAT publication. Existing captured title tiles relocate to768..1002, keeping
bonus136..267 and animation290..729 disjoint. Source map rows are retained;
plane and sprite viewport both use Y1. Display is disabled during cold loading,
prior DMA is flushed, and local transfer buffers are consumed synchronously.
The captured background is an interim asset; source-command composition and
image parity remain unverified.

verify_prelude_display_target.py builds an isolated cartridge and runs it in the
windowless Genesis Plus GX harness. It initializes terrain ring0 and cold player
palette state, runs setup, source scripts, pickup/contact and640SAT updates;
completion/error counters pass and production ROM remains a840997a... unchanged.
This verifies target execution, not live VDP pixels or full arcade scene parity.
Production title binding, sound/events and lifecycle continuity remain open.
No MAME/RetroArch frontend/window launched.


## 2026-10-23 — Prelude captures and224-line layout correction

Added optional PPM frame capture to genplus_probe.py without creating a window.
The isolated prelude diagnostic captures refresh40/120/360/600 and hashes each
image. Visual inspection found clipped copyright/credits with the previous
Y1 viewport. Archived arcade snap_title/sonson/0000.png measures240x240;
Genesis NTSC scene is256x224. The display now maps source row1 to target0,
uses sprite viewportY8, and places copyright source28 at target26 and credits
source30 at target27. This is an explicit display adaptation; actor/world
coordinates and simulation are unchanged. Horizontal content remains centered
with the extra Genesis width. The corrected frame600was inspected and both
bottom UI lines are visible. Captured background scores/credits remain static
until lifecycle-driven UI updates are connected.

Isolated target640-update smoke passes with diagnostic ROM c69f9354cca816a33cb55a0a7f3155c7d6e44871bc91b4eec4c2512d1152d365.
Production ROM remainsa840997a... unchanged. Four diagnostic captures are not
the ten final-release screenshots. No source-aligned motion/pixel-difference
claim or production title integration yet. No frontend/window launched.


## 2026-10-23 — Live prelude HUD inputs

Added generated glyph banks for source attributes44/56/08/0C/10:211tiles at
1003..1213, disjoint from title/animation/bonus/font arenas with compile-time
guards. arcade_prelude_hud formats eight-digit high scores (source leading
blank semantics), full16-bit credit balances, free-play labels and an explicit
lifecycle-selected prompt. arcade_prelude_display_hud synchronously updates
only high-score cells and the two UI rows; caller controls when values change.
Prompt selection is not inferred from credits, preserving the source's timed
credited state. Score/credit values are supplied by the owner, not read from
the captured background.

verify_prelude_hud.py passes all65536credit values and28high-score/mode boundary
cases against independent decimal formatting, with sanitizers. The isolated
68000scene changes values and mode over640updates; captures include high543210,
paid credit changes and FREE PLAY/PRESS START. Frame600was visually inspected.
Diagnostic ROM a627e541... passes; production ROM remainsa840997a... unchanged.
These are diagnostic inputs, not controller-driven production integration.
Full lifecycle binding, live saved-score selection and sound remain open.
No emulator frontend/window opened.

## Current RNG handoff checkpoint — 2026-10-23

ROM `c60677572a64d6aa7d577876716dad240f599b4ff9651f7ef44d77add3a8e6e9`
passes 100 read-only initial-title RNG snapshots following controller-driven boot:
seed87CD, sample0. The verifier requires all 1,000 consecutive refresh records
and clears its old success report before execution. The isolated initialization
regression previously failed on old code and passed all256 current seed/sample patterns.
Repeated-cycle reseeding and full RNG call-order parity remain unverified.

All five injected post-game scenarios pass again on this ROM, including two-player
names/rankings, paid Start interruption, fresh-process SRAM restoration and unattended
name timeout. These do not establish natural game-over reachability.
The20-signature original title audit still reports native lifecycle FAIL.

The matching benchmark records49.13/34.58 presentations per second in1P/2P,
worst60-refresh windows21/20, and final simulation debt0/267. Neither performance
gate passes; neither workload completes a course. Source files were checked against
the build manifest and archived with ROM/symbols/reports in
`out/parity-audit/rng-handoff/build`. This is an artifact snapshot, not yet a
clean-checkout reproducibility proof.

## Arcade title clock advancement — 2026-10-23

ROM `ccae88ae2319316426319c0120f37a447d6f6b8104d9418246f2299de1b67bda` now advances the shared source clock during Arcade title waiting,
including Freeze, at the existing55.4Hz source cadence. Original9112 increments
23:24 before the9130 Freeze dispatch gate. The title scheduler re-anchors after
game_run so demo refreshes are not counted twice. Home title behavior is unchanged.

The old c606 build records zero clock updates over the frozen observation interval;
the new build records1099 in both coin/Start and unattended scenarios, matching
1189 Genesis refresh intervals at source cadence. Frozen dispatch, retained coin
and subsequent Start/demo resume checks pass. Baseline evidence is preserved in
`out/parity-audit/title-freeze/before-title-clock`.

Current attract validation checks5333 script updates across3 entries/exits. Initial
RNG snapshots and all5 injected post-game/SRAM scenarios pass. Current benchmark:
18000updates each,51.33/33.64presentations/s, worst60-refresh windows24/20,
final debt0/153. Both60FPS gates fail. The changed clock changes trajectories;
this is not a matched-workload optimization result. Build/evidence snapshot:
`out/parity-audit/title-freeze/clock-build`.

Remaining: replace900-refresh title lifecycle; connect recovered prelude/rankings;
share fractional scheduler phase across all scene owners; resolve boot/setup/queue
IRQ timing and natural whole-course parity. This patch fixes missing title clock
advancement, not complete lifecycle parity.

## Shared Arcade scheduler — 2026-10-23

ROM `deafc71cb6e18c8bc49a88377e29af90e5aebb8f5277f3c9cc0a9c07b3f0fc32` uses one persistent Arcade pacing object across title, gameplay and
post-game scenes. Entering a scene preserves fractional phase and pending updates;
only the host-refresh anchor changes. Home retains its independent scheduler,
pause and survival policies. Setup intervals remain excluded: this is not yet
original IRQ/queue timing. Full six-state title integration remains open.

The scheduler handoff verifier checks 200,000 observations and 54,546 repeated
entries against a 64-bit rational oracle under NTSC/PAL, wrap, partial draining
and Home isolation with ASan/UBSan. Home production-loop shims pass eight
scenarios plus 512 attract timeout cases; cadence arithmetic passes 320,000
observations. Target attract validation covers 5,332 updates across three
entries/exits; both Freeze scenarios and all five injected post-game/SRAM
scenarios pass. Post-game traces verify gameplay points at the shared owner.

Performance remains failing: 18,000 updates each; 51.30/29.62 presentations/s
in 1P/2P; worst 60-refresh windows 24/20; final source debt 0/136, peak 11/195.
The 2P result is worse than the prior baseline. Timing changes alter consumed
inputs/trajectories, so this is not an isolated CPU-cost comparison. No full
course completed. The benchmark reader now reads the shared object directly,
not the gameplay pointer's storage. Source/build/reports are archived under
`out/parity-audit/pacing-handoff/build`.

## Demo difficulty initialization — 2026-10-23

ROM `e09fc4eac35b7278230362d8225504d6fdd688e6908788fddce06882ccf7dc0f` fixes 8F80..8F87: source loads difficulty54 only for live-game22;
otherwise it clears B before storing stage5A. Native game_init previously
applied configured difficulty to both paid and attract sessions. Demos now
initialize stage/combat difficulty to zero; paid games keep the DIP selection.

The windowless before/after matrix injects each raw/decoded difficulty setting
at title refresh900, then observes the first four target gameplay updates after
controller-paid Start or unattended demo entry. Prior deafc71 ROM fails all three
non-Easy demo cases (stages4/8/12); current build passes all eight demo/paid cases,
including combat difficulty. Source bytes are pinned in the verifier and title
audit. Current attract sequencer check passes5332updates over3entries/exits.

This is initial difficulty verification, not full demo trajectories or title
lifecycle parity. No new performance result is claimed; previous benchmark
remains attached to deafc71. Source/build/evidence archive:
`out/parity-audit/demo-difficulty/build`.

## Attract score-write guard — 2026-10-23

ROM `543786d825898c1bdbc73dbe04784e1765b7bef16ed2059ebe8743cae8a0e61d` routes all21 additive game/entity award sites through the inline
score owner. Original4CBE..4CC5 branches directly to4D58 when live-game22 is zero.
The new guard suppresses demo score writes while reward argument evaluation,
actor state, combat increments and sound remain outside the guard. Live unsigned
arithmetic is unchanged; original eight-digit decimal overflow remains separate.

Prior e09fc4e ROM's controller-only demos reached700/250points; current demos
remain0/0 over5777observations. A separate first paid session reaches820points.
The initial paid verifier was invalid because it included later title snapshots
carrying demo scores; preserved before/limitations.json records that defect.
The corrected verifier ends paid observations before any subsequent demo and
uses pulsed firing. Host extraction checks15360gate/owner/evaluation cases under
ASan/UBSan. Current attract script validation covers5332updates and all five
injected post-game/SRAM cases pass.

Current benchmark18,000updates each:51.30/29.70presentations/s, worst60-refresh
windows24/20, final debt0/134. Both60FPS gates still fail; no completed course.
Source/build/evidence archived in `out/parity-audit/demo-score/build`. Full title
lifecycle, natural gameplay parity and performance remain open.

## Demo all-out lifecycle — 2026-10-23

ROM `461d619c67e8937064ea4aea14c5e3790e91a7d7a313bf9b7fc8717061eb2a7a` replaces the demo's150-update game-over beat with source97C6/97D2/97E3:
after both out, setup loads4 and falls through into the phased countdown; each
clock24&63==0 decrements it; the next update after expiry exits. Gameplay input,
players and world dispatch stop during this wait. The old generic GAME OVER
overlay is no longer triggered for this demo path. Paid continue timing remains
separate. Setup sends10/14 through the existing gated sound abstraction; exact
original80-prefix sound-driver semantics and audible comparison remain unverified.

All256 initial clock phases pass the byte-pinned countdown test. Controller-only
target runs naturally complete two all-out countdowns, checking468wait updates:
camera and input/script state remain fixed while clock/timer advance. The attract
input verifier now excludes ending dispatch and checks5157active script updates;
514ending snapshots are reported separately. All5injected post-game/save scenarios
and demo/paid score checks pass. No full original actor-death timing, visual or
whole-title-lifecycle parity claim. Previous benchmark remains associated with
543786d; this build was not performance-benchmarked. Archived source/build/reports:
`out/parity-audit/demo-end/build`.

## Current 2P cost profile — 2026-10-23

Isolated `out/parity-audit/profile-461d` profiles production461d619 without
changing its ROM. Diagnostic7f478b24 runs3600source updates in2P. Inclusive
SGDK subtick means: simulation1111.1, collision358.9, actor loop270.2,
render270.4 per presentation, scheduler87.2, pool context70.8. G/H/I/shot
adapters have3523/6747/4640/4794calls. These timer probes perturb execution,
include nested work/interrupts and cannot be summed or treated as CPU cycles.
Opening coverage contains no main/C/D/E/large enemies. Detailed counters and
next experiment are in checkpoint.json; no optimization or speedup is claimed.
The profiling builder now accepts a separate output directory, preserving
historical diagnostics. Next compare native adapter/dispatch changes on a
current exact-input snapshot, then collision scans; retain only equivalent,
measured improvements. No MAME or RetroArch frontend was launched.

## Native actor dispatch experiment rejected — 2026-10-23

Production461d619 remains unchanged. Splitting native G/H/I adapters out of
the legacy dispatcher matches3619per-tick records and56snapshots of3504bytes
(entity records, native pools, RNG/controllers/flags) with no differences.
The fixed-input3600-update benchmark is worse in both modes: baseline
52.9808/28.5596 versus candidate52.7653/28.0666 presentations/s in1P/2P.
Instrumented equal-state interval also increases3895 to3901refreshes. Reject.
These are diagnostic opening workloads with deterministic refills, not full
course or hardware results. `out/parity-audit/native-dispatch/decision.json`
binds the result. Fixed assembly profiling labels needed unique suffixes in
both isolated variants after GCC duplicated a block; production labels unchanged.

Replay tools now accept explicit entity variants and output directories instead
of requiring the historical actor-tail experiment. Pool comparisons now include
all48published Entity records. Next target is collision traversal; preserve
family/slot order, multi-hit behavior and reward/contact side effects.

## Demo setup input retention — 2026-10-23

Arcade 9492..94B5 clears current inputs 44:45, delays and script pointers,
but retains previous inputs 46:47 until 9A2C. Native setup now preserves
those bytes. The pinned-ROM regression fails against the archived prior
implementation and passes all 65,536 previous-input pairs under ASan/UBSan.
Natural original trace comparison passes 5,179 updates / 41,432 fields.
Production ROM `521cf095faed8de55dd59e773f55137244c7b9ceea47f346f89e379b801a44be`
builds; controller-only windowless target check passes 5,157 source updates
across three demo entries/exits. Ending and incoherent snapshots are excluded
as recorded in the report. This establishes input-sequencer behavior only;
the six-state title lifecycle remains unbound and current performance unmeasured.
Manifest-checked source/build and reports: `out/parity-audit/attract-init/build`.

Contact-coordinate cache experiment rejected: fixed 3,600-update diagnostic
presentation rates regress from 52.9808/28.5596 to 52.3035/27.5495 (1P/2P).
Sampled state matches (3,619 per-tick records, 56 pool snapshots), but these
opening workloads do not cover all actor families, whole courses or full state.
No candidate collision changes entered production. See
`out/parity-audit/contact-coordinates/decision.json`. No emulator frontend opened.

## Connected production title lifecycle — 2026-10-23

Production title.c now consumes the recovered six-state sequencer instead of
its 900-refresh Arcade loop. Prelude players/pickups, phased prelude countdown,
shared RNG reseeding, blocking gameplay demo with source-tick lifecycle callback,
individual/total saved ranking boards and credited screen are connected.
Home retains its separate menu. Start purchase remains controller-specific;
credit changes select state5 after the state handler. Original demo early exit
and outer countdown are both handled, including simultaneous completion.

ROM `2e24d8c371d1e74cf2f609e6704c339b67db0cdb6e780dd0236966125f8d2594` passes 14,000-refresh controller-only idle smoke,
repeated board/prelude cycles, P1 start from prelude, P2 start during demo and
simultaneous paid starts from rankings. Captures reviewed for prelude and total
ranking; source pixel equivalence is not claimed. Ranking/title/header/HUD art
uses disjoint VRAM ranges below tile1501; no new full-screen RAM buffer is added.
The existing source-ROM input sequencer runtime check also passes.

Remaining fidelity limits: source command queue busy/setup timing is collapsed
into synchronous scene loads; title background is still captured art; retained
player/input/world state between prelude and game needs source comparison.
Thus the integration closes the disconnected-lifecycle defect, not M2 as a whole.

Current 18,000-update controller workloads: 49.9715/28.9344 presentation Hz,
55.3902/55.3474 simulation Hz, worst60-refresh presentation counts20/19,
final debt4/17 (1P/2P). Both target checks fail. Lives refilled, no course complete;
refresh-latched inputs mean this is not a controlled comparison with older ROMs.
Reports/captures under out/parity-audit/title-connected. No frontend launched.

## Native-only shot dispatch optimization retained — 2026-10-23

When no legacy slot has ever been allocated, six empty legacy family passes
are now bypassed. Main/C/D/E/F dispatch remains before the native I/H/G pass;
Survival and allocated legacy pools retain the original loops. The monotonic
allocation latch is rechecked after native family calls. No actor counts,
collision order, sound or simulation cadence are intentionally reduced.

Exact-input 18,000-update diagnostics improve 1P50.9411→51.3443 and
2P28.5036→30.9367 presentation Hz. Instrumented replay matches18,018tick
records and281raw3504-byte pool snapshots with zero differences; matched
17,920source intervals take20,098→19,920refreshes. These checks exclude
some globals, food/background and audio state and do not complete a course.
Production allocation/reset120,050cases and legacy candidate-order100,000cases
pass ASan/UBSan. The latter checks candidate ordering, not collision effects.

Current ROM `06d73bc6f5748cac63d5c71c1a64d85db02851cc12253148e109af03308ebf10` passes repeated connected-title cycles.
Production 18,000-update controller workloads measure50.3075/30.3102Hz,
worst60-refresh presentation counts24/20, final simulation debt0/25(1P/2P).
Both60FPSgates still fail. Different refresh-latched trajectories prevent direct
speedup claims from older production rates; controlled evidence is above.
Evidence archived in out/parity-audit/native-collision-path/build. No frontend.

## Admission cache experiment deferred — 2026-10-23

An isolated in-place cache-identity update matches60,000host calls per variant
and3,618target tick records/56pool snapshots, but short2P presentation gain is
only27.6200→27.6342Hz. Not adopted; production06d73bc remains unchanged.
Full cache counters, input bytes and output bytes are compared by the host test;
replay still excludes some globals/audio/food. Evidence: admission-inplace.

Fresh windowless inclusive profile06d73: simulation1179.7, entities921.4,
collision354.8, actorloop344.7, render284.1, sprites212.0, admission100.8mean
subticks. Instrumentation/interrupt costs are included and probes nest; these
are not additive CPU budgets or production FPS. Opening workload has no
main/C/D/E/large actors. Next target is native actor adapter overhead, not a
further cache rewrite. Diagnostic build/report bound in profile-06d73.

## Native actor adapters inlined — 2026-10-23

G/H and I adapter statements move intact into always-inline helpers at the
entity update loop. Survival and other actor kinds retain the general path.
Whitespace-normalized bodies match the prior branches; compiled helpers have
no separate symbols. GCC folds the remaining dispatcher too: combined symbol
sizes change20,668→13,512bytes in diagnostic builds, with entity_update_all's
static frame80→36bytes. This is not a complete runtime stack measurement.

18,000-update fixed-input benchmark:1P51.3443→52.1138,2P30.9367→33.8180Hz.
18,018tick records and281pool snapshots match without differences. Equal
17,920source intervals take19,920→19,707refreshes in the instrumented replay.
Coverage excludes some globals/food/audio and does not complete a course.

Current ROM `3fea1f2a0a8f0834a2ab8dd60038a5da4fe5742f4ef684c8c479cd4bdb9b0f46` builds and passes connected idle title cycles.
Production controller workload measures50.9631/34.4769presentationHz,
worst60-refresh counts20/20, final debt0/29. Both60FPSgates remain failed.
The exact-input comparison supports the local speedup; production input latching
still prevents comparing old/new trajectories as equal work.

Compiler duplicated a scheduling block, requiring unique zero-byte assembly
marker suffixes. The windowless subsystem profiler supports the new adapter
call site; historical MAME single-address profilers now reject duplicated
markers explicitly. No frontend opened. Subsequent player-shot inlining gave
only0.57percent short2P gain and was not adopted. Archive:native-inline/build.

## Eight-digit live-score rollover — 2026-10-23

`game_score_add` now retains the low eight decimal digits, matching the four
packed-BCD additions at source4CD9..4CF4. The final decimal carry is discarded.
The original live-game gate remains: attract-mode awards evaluate their caller
arguments but do not alter either player's score. Ordinary in-range awards avoid
division; external/old-save u32 operands are normalized before summing.

`verify_score_rollover.py` pins the original ADDA/ADCA+DAA byte chain and checks
200,002 cases against an independent four-byte decimal carry model.
`verify_score_rollover_target.py` executes the actual inline helper with volatile
operands in an isolated -O3/LTO 68000 ROM: 243 cases, zero errors.
`verify_score_gate.py` passes 15,360 ownership, attract-gate, and argument-side-
effect cases with the corrected decimal rule. This closes the individual-score
arithmetic gap previously left open by the gate-only check. It does not close
combined ranking-score arithmetic, high-score update ordering or per-award
bonus-life timing. Full-course comparison remains required.

## Combined score wrap and extended-course coverage — 2026-10-23

The existing postgame `pack_score(p1+p2, scores[2])` already retains exactly four
BCD bytes. With individually normalized scores, its binary intermediate is
below200,000,000 and cannot overflow u32. `verify_combined_score.py` pins the
source4D66..4D81 ADDA/ADCA/DAA sum and passes200,000 comparisons, including
100,000 exact-wrap cases. No implementation change was necessary. This closes
the combined ranking arithmetic question, not live combined-display timing.

The current production ROM8b9936bf completed55,000 source updates per mode in a
windowless controller workload with lives refilled. Both modes reached camera
22016 at tick48811, reset to0 at48812 and continued. Rates58.78/57.47
presentations/s, worst60-refresh windows32/27, final debt0/0. The longer
coverage exposes late barrier-scene drops missed by opening-only workloads.
This is traversal/restart evidence, not normal playthrough or source-equivalence
proof for every scene. Full data: out/parity-audit/extended-course/report.json.

## Confirmed bonus-life mismatch — 2026-10-23

ROM4183 selects pointers at41FE using raw DSW2 bits3..4. The four13-byte
records at4206,4213,4220,422D contain flags and three packed-BCD amounts.
The existing menu labels and fixed three-award checker are wrong. Source
FC2C runs only when live22 is nonzero, clock24 low3 bits are zero, and the
selected player (clock bit3) is not out (88/89). Phase0 success advances to1;
phase1 remains1 and adds the recurring increment. Thus raw indices0/1 award
20K/30K once, and2/3 award20K then80K then every100K, or30K then90K then
every120K. FCA1 increments an8-bit reserve with no nine-life cap. FC63 also
rejects a nonzero leading score byte; FC92 independently compares both score
words, a quirk retained by the extracted routine.

`src/arcade_bonus_life.c` is now connected to the gameplay checker. Menu
labels follow raw DIP ordering; the reserve bridge preserves byte rollover
without the former nine-life cap. Results-board awards no longer invoke the
checker immediately. The host source oracle passes 120,000 cases; the extracted
bridge passes reserve and mode cases. The earlier isolated target check passed
1,668 assertions before the byte-load workaround.

Integration exposed a compiler widening bug: AND.L read g_config+1 (odd) and
raised an address error. Explicit byte access fixes that instruction. Production
ROM `766392a374fcfd7015e36a9c7a1bbc67ddb94a39654b02f29ba7f1d9d2e362eb` passes P2 paid start and 6,000-refresh 1P/2P
opening gameplay checks with no missed presentations. This establishes boot and
opening regression recovery, not complete-course bonus scheduling equivalence.
Evidence: out/parity-audit/bonus-life*, out/parity-audit/title-connected/demo-p2,
and out/genplus-results.json.

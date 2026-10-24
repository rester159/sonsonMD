# SonSon Native — NEXT WORK (prioritized)

The game is done & playable (PROJECT_STATE.md). Everything below is **faithfulness refinement**, not
bug-fixing. Work the doctrine (START_HERE.md): behavior = RE-first + cite the $address; verify 60fps.

> **NOTE (2026-06-19):** the former TOP ITEM — wire the spawn scheduler to the arcade `$FCD9` 87-wave
> schedule — is **DONE** (commit `7b3dfcd`). The hand-authored wave program is gone; `entity_schedule_tick`
> now reads `ss_wave_mask[]` per wave-id and spawns the masked classes. Verified: 87/87 waves, per-wave type
> histogram matches the masks, cap held at 10, FPS 1.0000 (`tools/wave_sched_verify.lua`). Details kept below
> for reference. **All remaining headless-completable refinement is done — what's left is the OWNER-INPUT
> TAIL, which cannot be closed without the owner.**

---

## DONE (2026-06-19, commit `7b3dfcd`) — spawn SEQUENCING is now arcade-faithful

**Why:** the native previously spawned enemies from a **hand-authored** per-level wave program that only
*approximated* the arcade progression. This was reverse-engineered, byte-verified, and wired in — the right
enemy mix now appears at the right scroll position (the last gameplay-faithfulness gap).

**How it was wired:** `wave_id_for_camera()` maps the wrapped `cameraX` onto all 87 waves
(`wid = cameraX*87/g_levelW` — the native ribbon-length re-expression of the arcade `$90&0x7F` / 256px-per-wave
cadence, cite `$EF98`). `entity_schedule_tick()` reads `ss_wave_mask[wid]`, walks bits LSB-first (cite `$EFBB`),
and spawns each set bit's `EntityType` at the right edge; enemyMain keeps the byte-faithful `$F0C9` ground
placement, classI→THROWER (native lobs via a thrower), bits 8/10 left unarmed (special-context). Cap (10) and
`fromSched` recycle preserved. Verify harness: `tools/wave_sched_verify.lua` (re-derive probes per build).

**The arcade mechanism (RE'd + verified, ss.03e):**
- Per-frame scheduler `$EF51` → producer `$EF89`.
- **wave-id = the scroll counter `$0090`** (`$EF98`: `LDB <$90; ANDB #$7F`) — i.e. the wave you're in IS
  your scroll position. As you scroll, wave-id advances and a new wave loads.
- wave-id indexes the **87-entry pointer table `$FCD9`**. Each wave entry = a **u16 class-presence MASK** +
  **popcount(mask) placement bytes**.
- **Bit i of the mask arms control record `$0638 + i*$10`**; that class's per-class scheduler (gated
  `LDB ,X; BEQ ret` on its enable byte) then spawns it. One placement byte per set bit (LSB-first) → that
  record's param `$E` (a per-spawn position/variant parameter).
- Verified independently: 87 pointers, structural invariant (entry length = 2 + popcount) holds 64/64,
  OR of all 87 masks = `$07FF` (every class used somewhere).

**Bit → arcade class → native EntityType (in `src/entity.c`'s `EntityType` enum):**
| bit | arcade class | native EntityType | notes |
|-----|--------------|-------------------|-------|
| 0 | enemyMain | `ENEMY_A` / `THROWER` | walkers + thrower (only 4 waves!) |
| 1 | classC | `CREATURE_C` | 16 waves |
| 2 | classD | `EMITTER` | 30 waves |
| 3 | classE | `COMPOSITE_E` | 13 waves |
| 4 | classF | `FLYER` | 17 waves |
| 5 | classH | **CONFIRM** — the X-tracker (sub_D083/$D091); 34 waves (2nd most common) | check the native has a faithful stand-in; CREATURE_C/G may currently double for it |
| 6 | classG | `CREATURE_G` | **41 waves — most common** |
| 7 | classI | `ENEMY_SHOT` | lobbed shot (19 waves) — note it's a projectile class |
| 9 | obj16x16 | `LARGE` | 5 waves |
| 8,10 | rec06B8 / rec06D8 | (special; bit8 gated by `$9D`) | low priority |

**The data is already extracted:** `inc/ss_wave_sched.h` — `ss_wave_mask[87]`, `ss_wave_place[190]`,
`ss_wave_place_off[88]`, plus `SS_WAVE_BIT_*` defines. (Regenerate with the inline Python in commit
`63b9696`'s flow if needed; full disasm at `tools/ss03e_disasm.txt`.)

**What to change (`src/entity.c`):** replace the hand-authored wave program — `k_wave_program` /
`WaveStep` segments + `k_wave_row[4][12]` + `k_wave_mix` + `entity_schedule_init` / the per-frame
`entity_schedule_tick` (search those names) — with a driver that:
1. Maps the native scroll position (`cameraX` / ribbon progress) → a wave-id, matching the arcade
   `$90` cadence (tune so all 87 waves span the ribbon sensibly; the arcade `$90` is the scroll-accumulator
   integer byte, scroll engine `sub_A697`).
2. On each wave-id advance, reads `ss_wave_mask[wid]` and spawns the masked classes (bit→EntityType),
   using the placement byte for the spawn row/variant (`k_spawn_row_y[6]` are the tier Ys).
3. Keeps the on-screen cap (10) and recycle behavior.

**Verify:** `bash build.sh`; re-derive probes from `out/symbol.txt`; run a MAME `genesis` lua that drives
the player and logs the live entity TYPE histogram vs scroll/wave-id, confirming the mix follows the
schedule (classH/I/F early, classG/D/C/E/obj16x16 entering later). Confirm `g_frameCounter` ratio ≈ 1.000
(60fps) in a busy scene. Compare against the arcade with a `sonson`-driver capture if useful.
**Caution:** the current spawn system works and is verified — change carefully, keep the cap, watch fps.

---

## OWNER-INPUT TAIL (cannot be closed headless — needs the owner)
1. **3 rare-enemy point values** (COMPOSITE_E / LARGE / ORB): their score core is SWI-obfuscated and they
   spawn only at deep waves the headless autopilot can't reach. Needs a **human-played `.inp` / save-state**
   that reaches those enemies, then measure the `$006C-6F` score delta per kill.
2. **Un-isolable SFX `0x07/09/0A/0E/0F`:** real handlers with non-empty streams, but never fire in any
   reachable attract/driven path. Needs reachable in-game events (human save-state) to isolate + wire.
3. **Color / feel / mix on real Genesis hardware:** per the doctrine there is no RE ground truth for color
   or framerate — the owner judges these by eye/ear on real HW. Get their notes, then tune.
4. (Optional) re-record `0x04/0x05/0x06` onto the SFX chip so those music-slot cues can play without
   clobbering BGM (audio-engine work).

---

## TOOLS AVAILABLE (in `sonson_native/tools/`)
- **QA soaks:** `soak.lua` (long in-game), `re_soak.lua` (session re-entry). Re-derive addrs first.
- **RE harnesses:** `composite_re2.lua` (pinned full stage sweep), `composite_re3.lua` (natural segment
  coverage), `scan_disasm.py` (unidasm wrapper). `ss03e_disasm.txt` (full ss.03e disasm).
- **fps:** `fps_probe.lua` / the `g_frameCounter` ratio pattern.
- **Asset bakers:** `gen_assets.py`, `gen_level.py`, `gen_anim.py`, `gen_collision.py`.

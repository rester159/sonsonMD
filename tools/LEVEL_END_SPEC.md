# SonSon — Level-End / Round-Advance Mechanic: RE + Capture Spec

**Status:** RESEARCH ONLY (no code edited). This documents what actually ends a
SonSon level and advances the next, with RE addresses + MAME capture evidence, and
a concrete plan for `sonson_native`'s `game.c`.

**Date:** 2026-06-17. **Sources:** RE artifacts under `…/son son/sonson/`
(`port/src/sub_*.c`, `regions.jsonl`, ROM `rom/ss.01e`); authoritative captures via
MAME `sonson` (NTSC arcade, `D:\portForge\tools\mame\mame.exe`, romset
`D:\portForge\tools\mame\roms\sonson`). Capture logs + screenshots saved in this
`tools/` dir (filenames cited inline).

---

## 1. WHAT SONSON'S LEVEL-END MECHANIC ACTUALLY IS

### TL;DR — there is NO discrete level-end. It is a continuous forced auto-scroller.

SonSon is **one endless horizontally auto-scrolling ribbon**. The world scrolls
right at a fixed rate forever; the player is pinned in a screen band and runs/climbs
the multi-tier vine playfield. "Levels"/"rounds" are **not** gated by reaching a
goal, a boss, a temple, or a fixed distance with a stop. Instead:

- A **segment table** (`$6485`, 26 records × 7 bytes) is walked by a segment index
  **`$0067`** that cycles `0 → 25 → 0 → …`. Each record feeds the next chunk of
  background-tilemap stream + a per-segment music/event cue + a countdown reload.
- A separate **global stage/difficulty counter `$005A`** (mirrored to `$0096`)
  **increments by 1 each time a segment expires**, saturating (capped) at `0x1C` (28).
  `$0096` is what the enemy-wave spawner reads (its hi-nibble selects the wave list) —
  so as you travel, enemies get harder/different, but the playfield never "ends."
- On the BG-stream side, when `$0067` wraps `25 → 0` the tilemap source pointer
  **snaps back to the start (`$6D3B`)** — i.e. **the background scenery LOOPS**.

So "advancing a level" in SonSon = **the stage counter `$005A`/`$0096` ticking up as
you auto-scroll through the looping segment ribbon.** There is no end-of-level
transition screen, no boss, no climb-to-the-top finish. (The brick "ruins/temple"
sections people remember are just BG tiles mid-ribbon — see level5 render, §1.5.)

### 1.1 The scroll engine — `sub_A697` (RE: code_8000 `$A697`, file `port/src/sub_A697.c`)

The perpetual rightward scroll is a 16-bit phase accumulator:

```
$A697  LDD  <$91          ; 16-bit sub-pixel scroll fraction at $91/$92
$A699  ADDD #$008D        ; += 0x008D every call (the forward-creep constant)
$A69C  STD  <$91
$A69E  BCC  $A6A2         ; on carry-out of the fraction…
$A6A0  INC  <$90          ;   …bump the coarse scroll byte $90  (the visible scroll)
$A6A2  STA  <$E6          ; copy hi-byte to $E6 = the HW scroll-X register latch
       … (every bit-3 crossing of the phase: refill the column double-buffer,
          JSR $A82F (column blit), BSR $A730 (segment/stage advance), set $8B=1)
```

- `$0090` = coarse scroll position byte (what the playfield window slides by);
  `$0091/$0092` = the sub-pixel fraction. `$00E6` = the hardware scroll register.
- **`+$008D` per frame** is the only forward motion source. NB: the native port
  already mirrors this exactly — `RUN_DRIFT_SUBPX` in `game.h`, applied every frame
  in `player_control` (`game.c`). So the native creep is faithful.
- **Measured scroll rate (capture):** `$0090` increments ≈ every **465 frames**
  (65536 / 141 = 465). At arcade 55.4 fps that's ≈ **8.4 s per `$90` step**.
  Evidence: `tools/levelend_trace2.txt` — `scrHi` 01→02→03 at frames 1865 / 2330 / 2795
  (Δ≈465 each).

### 1.2 The segment/stage advance — `sub_A730` (RE: code_8000 `$A730`, `port/src/sub_A730.c`)

Called from `sub_A697` each time the scroll crosses a column-buffer boundary:

```
$A730  LDB <$8A; ANDB #$1F; BNE done        ; only act on a 32-col boundary
$A738  JSR $54E7                             ; (refill / draw the new column band)
$A73B  DEC <$98                              ; $98 = per-segment countdown
$A73D  BNE  L_A772                           ; not expired yet -> just reload $94/$8E/$6A below
       ; ---- segment EXPIRED: bump the global stage, then advance the segment ----
$A73F  LDB <$5A; CMPB #$1C; BCC L_A74A       ; stage cap = 0x1C (28)
$A745  INCB; STB <$5A; STB <$96              ;   $5A++ AND mirror to $96 (the wave-stage)
L_A74A LDB <$67; INCB; CMPB #$1A; BCS L_A752 ; segment cap = 0x1A (26)
$A751  CLRB                                   ;   $67 wraps 25 -> 0
L_A752 STB <$67
$A754  LDA #$07; MUL ($07 * $67)             ; record index = 7 * $67
$A757  LDX #$6485; LEAX D,X                   ; X -> leveldata[$67]
$A760  LDD ,X;  STA <$98 / STB <$9E          ; +0 = new segment countdown -> $98
                                              ; +1 = event/wave byte -> $9E (and $9D=old)
$A766  LDB <$A0; STB <$9F; LDB $2,X; STB <$A0 ; +2 = secondary event byte -> $A0
$A76E  LDD <$90; STD <$65                     ; remember scroll at this segment start
L_A772 LDB <$67; LDA #$07; MUL; LDX #$6485; LEAX D,X
$A77C  LDD $3,X; STD <$94                      ; +3 word = BG tilemap stream pointer -> $94
$A780  LDD $5,X; STD <$8E; STD <$6A            ; +5 word = 2nd stream pointer -> $8E/$6A
$A786  RTS
```

Key facts proven here:
- **`$0067` (segment)** cycles `0..25` (cap `0x1A`=26), wrapping to 0.
- **`$005A` (stage)** increments once per segment expiry, **saturating at `0x1C`=28**
  (`CMPB #$1C; BCC` skips the `INCB`, so it never exceeds 28 — it just stops rising).
- **`$0096`** is written = `$005A` here. It is the difficulty/wave selector.
- **`$0098`** = per-segment frame/column countdown, reloaded from `leveldata[$67]+0`.

### 1.3 The level-data table — ROM `$6485`, 26 records × 7 bytes

Dumped from `rom/ss.01e` (loads at `$4000`; `$6485` = file offset `0x2485`).
The RE region for `$6485` is currently mislabeled `lsr_neg_rti` (invalid-code) in
`regions.jsonl` — **it is actually this 7-byte-stride data table** (confirmed by
`sub_A730` + `sub_FBE6` reading it). Layout per record:

| off | meaning (RE) | consumed by |
|----|---------------|-------------|
| +0 | segment countdown reload → `$98` | `sub_A730` |
| +1 | event/wave code (00 / 01 / 02) → `$9E` | `sub_A730`, `sub_FBE6` |
| +2 | secondary event byte (`$14` or `$0F`) → `$A0` | `sub_A730` |
| +3..4 | **BG tilemap stream pointer** (big-endian word) → `$94` | column streamer |
| +5..6 | 2nd stream pointer (word) → `$8E`/`$6A` | column streamer |

Full table (rec = `$67` value):

```
rec  dur evt +2   +3word  +5word
 0   05  00  00   6D3B    825B
 1   05  00  00   6E0B    82DB
 2   05  00  00   6EDB    835B
 3   01  01  14   6FAB    83DB   <- evt=01 (checkpoint cue)
 4   03  00  00   707B    845B
 5   05  00  00   714B    84DB
 6   05  00  00   721B    855B
 7   05  00  00   72EB    85DB
 8   01  01  14   73BB    865B   <- evt=01
 9   03  00  00   748B    86DB
10   05  00  00   755B    875B
11   05  00  00   762B    87DB
12   05  00  00   76FB    885B
13   01  01  14   77CB    88DB   <- evt=01
14   03  00  00   789B    895B
15   05  00  00   796B    89DB
16   05  00  00   7A3B    8A5B
17   05  00  00   7B0B    8ADB
18   01  01  14   7BDB    8B5B   <- evt=01
19   03  00  00   7CAB    8BDB
20   01  01  14   7D7B    8C5B   <- evt=01
21   03  00  00   7E4B    8CDB
22   01  01  14   7F1B    8D5B   <- evt=01
23   03  00  00   7FEB    8DDB
24   02  00  00   80BB    8E5B
25   01  02  0F   818B    8EDB   <- evt=02 (last record before wrap)
```

- `+3word` climbs **monotonically by `0xD0`** (`$6D3B → $818B`): a contiguous
  ~5 KB tilemap-column stream in `ss.01e`. On `$67` wrap, `$94` snaps back to `$6D3B`
  ⇒ **BG loops** (verified: `$818B + 0xD0 = $825B`, but the wrap resets to `$6D3B`).
- `+5word` climbs by `0x80` (a second contiguous stream in `ss.02e`).
- `evt` is **just a sound/event cue**, not a level transition (decoded below).

### 1.4 The `evt` codes are music cues, NOT level ends — `sub_FBE6` + `$647B`

`sub_FBE6` ("update_state_machines", RE `$FBE6`) re-reads `leveldata[$67]+1` (the
`evt` byte) and dispatches:

```
evt == 02  ->  JSR $647B  ; $647B:  LDA #$17 ; JMP $6341   (sound command 0x17)
evt == 01  ->  (checkpoint-class cue via $6457/$645C: LDA #$11/#$12 ; JMP $6341)
```

`$6341` is the **sound-command sender**. So `evt` codes fire BGM/jingle cues at
segment boundaries (`$0F`–`$18` are the verified SonSon sound-command range). They
do **not** load a new level, show a banner, or stop the scroll. (Raw bytes:
`$647B = 86 17 7E 63 41` = `LDA #$17; JMP $6341`.)

### 1.5 The per-level (re)start — `$8F4x` init block (in `port/src/sub_82A1.c` region)

The "start a level / restart after the level-init" routine resets the scroll engine:

```
$8F5B  CLRA; CLRB
$8F5D  STD <$65          ; clear scroll-at-segment-start
$8F5F  STD <$90          ; **scroll position $90/$91 reset to 0**
$8F61  STB <$67          ; **segment index reset to 0**
 …
$8F80  LDB <$54; TST <$22; BNE; CLRB
$8F87  STB <$5A          ; **stage $5A <- $54** (persistent stage-to-start), else 0
```

- **`$0054`** = the persistent "stage to (re)start at." A fresh game loads it into
  `$5A`; this is why capture shows `$5A` jump to a nonzero value (4) right at the
  start of play — the boot/level-init copies `$54`. (Writers of `$54`:
  `sub_4167`, `sub_6EDD/6F0D/6F1A/6F38` — the boot/dip/new-game path.)
- The scenery palette/tiles change with the stage band (different `$96` ⇒ different
  BG sections of the looping ribbon), giving the visual variety the prior captures
  baked as "levels 1-8" (see `tools/render_level{1..8}_blanked.png`,
  `tools/clean_level{1,2,3}_round{1,2,3}.png`).

### 1.6 CAPTURE EVIDENCE (files in this `tools/` dir)

- **`arcade_clean_midlevel.png`** — pristine mid-level arcade gameplay (SONSON HUD,
  6 vegetated tiers + vines, frog/turtle enemies, fruit, water line, score-popup).
  No goal/boss/temple in view. Snapped at `seg=0 stage=4 scrHi=02`
  (≈512 px scrolled), alive.
- **`arcade_gameplay_scrolled.png`** (= raw `0031.png`) — further into a stage:
  mummywrap enemies, different brick-tier sections; still the same continuous ribbon.
- **`arcade_gameplay_early.png`** (= `0029.png`) — a GAME-OVER frame; behind the
  "GAME OVER" overlay you can see the grey-tier scenery + skull enemies (the
  ribbon keeps scrolling under the overlay).
- **`levelend_trace.txt`, `levelend_trace2.txt`** — per-frame RAM logs of a driven
  game (`md $02` play). Show: gameplay starts `seg=0 stage=0`; level-init sets
  `stage←4` immediately; `scrHi` 0→5 over ~2700 frames (continuous scroll);
  `seg 0→1` at frame 3260 as `stage→5`; `$98` counts `05→04→…→01` then reloads.
  (The driven AI reliably dies around stage 5 — a player-skill ceiling, **not** a
  level boundary; `md` flips `02→00` = game over, never a level-clear screen.)
- Capture scripts (reusable): `levelend_trace.lua`, `levelend_trace2.lua`,
  `clean_shot.lua`, `find_lives.lua`, plus the pre-existing `ss_level_cap2.lua`
  (level-window stitcher) and `arch_stageforce.lua` (forces `$96`/`$5A`).
- **Probe addresses (maincpu DP):** `$0002` md (01 attract / 02 in-game),
  `$0005` st (screen dispatch), `$0067` segment, `$005A` stage, `$0096` wave-stage,
  `$0090/$0091` scroll, `$0098` segment countdown, `$0054` persistent start-stage,
  `$0073` player grace/i-frame byte.

---

## 2. DATA NEEDED TO REPRODUCE IT NATIVELY

The native engine does **not** need a vertical "climb-to-the-top" section, a goal
sprite, or a boss — **none exist**. To be faithful it needs:

1. **The scroll model (already present).** Forward creep `+$008D`/frame is already
   in `sonson_native` (`RUN_DRIFT_SUBPX`). ✅
2. **A stage counter `stage` (0..28, saturating) and a segment counter `seg`
   (0..25, wrapping)** in `GameState`, advanced by traversed scroll distance, *not*
   by a finish line. Mapping to capture:
   - One `seg` step ≈ one `$98`-countdown cycle. `$98` is reloaded from
     `leveldata[seg]+0` (values 1/2/3/5) and decremented once per **32-column scroll
     boundary** (`sub_A730`: `ANDB #$1F`). A 32-col band = 256 px of scroll. So one
     `seg` step ≈ `dur × 256 px` of scroll (dur ∈ {1,2,3,5}).
   - `stage++` (cap 28) each `seg` step. `stage` drives enemy difficulty/wave.
3. **The wave-stage hook:** `entity.c` should pick its spawn list from `stage`
   (the hi-nibble of `$0096` selects the list: stages `$00–$0F` band 0, `$10–$1C`
   band 1 — see §1, the `$0096` hi-nibble). The native `entity.c` already ramps
   difficulty per round; switch it to read `stage` for arcade fidelity (optional —
   see §4).
4. **Looping BG.** The native port already streams a wide per-level map and the
   arcade BG genuinely **loops** every 26 segments (`$67` wrap → tilemap ptr
   `$6D3B`). The 8 baked `ss_levels[]` maps (`tools/level{1..8}_capture.json`,
   ~175–181 cols ≈ 5.5 screens each) are good scenery; faithfully they are windows
   into the one looping ribbon, but **cosmetically they are fine as distinct levels.**
5. **(Optional) segment music cues.** The `evt` column fires sound commands at
   segment boundaries (`$11/$12/$17`, via `$6341`). Low priority — cosmetic audio.

**NOT needed:** vertical-section tilemaps, a temple/goal tilemap, a boss entity.
The "ruins/temple" look is ordinary BG tiles already captured in the level maps.

---

## 3. CONCRETE IMPLEMENTATION PLAN for `sonson_native`

Goal: replace the camera-reaches-end stand-in with the authentic **distance-driven
stage advance over a continuously (looping) scrolling ribbon.** Two faithful options;
pick per the owner's "looks-right to an average user" bar.

### Option A (most faithful — endless looping scroll, recommended)

In `game.c` / `ss_level.c`:

1. **Make the level scroll endlessly (loop the map).** Today `player_control`
   clamps `cameraX` to `maxCam = g_levelW - SCREEN_W` and `bg_stream` stops at the
   map's right edge. Change the camera to **never clamp at the right**: let
   `cameraX` advance unbounded and have `stream_column` index the level map
   **modulo `g_curLevel->cols`** (`levelCol % cols`) so columns wrap — the BG ribbon
   repeats, exactly like the arcade's `$67`-wrap → `$6D3B` reset. The 64-col ring
   buffer already wraps in hardware; only the *source* index needs `% cols`.
2. **Drive `seg`/`stage` by scroll distance.** Add to `GameState`:
   `u8 seg; u8 stage; u16 segScrollLeft;` Initialise `seg=0`, `stage` from a
   `g_startStage` (= arcade `$54`; default 0), `segScrollLeft = SEG_DUR[seg] * 256`.
   Each frame, subtract the camera's pixel advance from `segScrollLeft`; when it
   reaches 0:
   ```c
   if (stage < 28) stage++;          /* saturating, arcade $5A cap 0x1C  */
   seg = (seg + 1) % 26;             /* arcade $67 cap 0x1A              */
   segScrollLeft = SEG_DUR[seg] * 256;
   /* optional: snd cue from SEG_EVT[seg]; entity re-seed at new difficulty */
   ```
   where `SEG_DUR[26]` / `SEG_EVT[26]` are baked from the `$6485` table (§1.3).
3. **Use `stage` for difficulty + HUD "round."** Either keep the HUD "RND" =
   `stage` (or `stage` mapped to a 1-based round), and feed `entity.c`'s spawn
   /difficulty from `stage` (hi-nibble selects the wave band). The per-`seg`
   boundary is the natural place to re-seed/escalate the wave.
4. **Delete the round-clear "push the wall" beat.** Remove `FLOW_ROUND_CLEAR`, the
   `endDwell`/`ROUND_CLEAR_DWELL` logic, and the `round_advance()` "reached the end"
   detection in `game_update` (`game.c` lines ~641–671, 693–704). They model a
   finish line that does not exist. If a "new scenery" beat is still wanted
   cosmetically, trigger a brief banner on `stage` band changes (every 16 stages),
   not on a wall-push.

This yields the arcade feel: run forever, scenery loops, enemies escalate with
`stage` until you die — game over returns to the front-end (already wired).

### Option B (minimal change — keep discrete levels, fix the trigger)

If the owner prefers the current "8 distinct baked levels" cosmetic structure
(it's pretty and already built):

1. Keep `ss_levels[]` + `round_advance()`, but **change the advance trigger** from
   "camera maxed + push the right wall for `ROUND_CLEAR_DWELL`" to **"traversed N
   segments of scroll"** — i.e. advance the round after the camera has scrolled the
   level's full width once (which is what the arcade does over ~26 segments). This
   keeps levels finite but removes the artificial "stop and shove the wall" feel: the
   level simply ends when you've scrolled to the far edge, then the next baked level
   loads. Functionally close to today but without requiring the player to *push*
   into a clamp — the perpetual creep carries them to the end.
2. Keep `stage`/difficulty ramp keyed off `g_game.round` (already done).

### Effort estimate
- **Option A:** ~half a day. The scroll already creeps; the work is (a) make
  `stream_column` wrap `% cols` + un-clamp the camera right edge (~10 lines), (b)
  add the `seg/stage/segScrollLeft` counters + the per-32px boundary tick (~30
  lines), (c) bake `SEG_DUR[26]`/`SEG_EVT[26]` from §1.3 (a const array), (d) rip
  out the `FLOW_ROUND_CLEAR`/`endDwell` path (~40 lines deleted), (e) point
  `entity.c` difficulty at `stage`. Play-test: confirm it scrolls endlessly, the BG
  loops seamlessly at the wrap, and `stage`/enemies escalate. No byte-golden (RNG
  timing-tied per project doctrine) — verify by eye + an FPS/seg/stage HUD probe.
- **Option B:** ~1–2 hours (just swap the trigger condition + drop the dwell).

---

## 4. IS THE CURRENT "CAMERA-REACHES-END" STAND-IN ACCEPTABLE?

**Partly. It is structurally wrong but cosmetically close-ish; recommend replacing
the *trigger* (and ideally adopting Option A) before calling levels "faithful."**

What the stand-in gets RIGHT:
- The perpetual forward creep (`RUN_DRIFT_SUBPX = $008D`) is exactly the arcade.
- Multi-tier vine playfield, lane wrap, scrolling world — all faithful.
- "Game over returns to front-end" — faithful.

What it gets WRONG vs the arcade:
1. **There is no finish line in SonSon.** The stand-in makes the player *push into
   the right edge and hold for a dwell* to clear the round. The arcade has the world
   scroll forever (looping) with a *saturating stage counter* doing the "advance."
   The "push the wall to win" interaction does not exist in the original and will
   feel off to anyone who knows the game.
2. **The arcade BG loops; the stand-in stops at the map edge.** A faithful level is
   endless-looping scenery, not a finite strip with a wall.
3. **"Rounds" in the arcade are a difficulty counter (`$96`/`$5A`), not discrete
   level swaps.** The stand-in's round 1→level1 … cycling is a reasonable cosmetic
   stand-in but isn't how the original structures progression.

**Recommendation:** adopt **Option A** (endless looping scroll + distance-driven
saturating `stage`), which is both the faithful mechanic and a small diff given the
creep + ring-buffer streaming already exist. If schedule-bound, **Option B**
(swap the trigger to "scrolled the level width," delete the wall-push dwell) is an
acceptable interim that removes the most obviously-unfaithful interaction for ~1–2 h.

---

## APPENDIX — address quick-reference

| addr | name | role |
|------|------|------|
| `$0002` | md | 01 attract / 02 in-game |
| `$0005` | st | screen/state dispatch index |
| `$0054` | startStage | persistent stage to (re)start at → copied into `$5A` at level-init |
| `$005A` | stage | global stage counter, ++ per segment expiry, **cap 0x1C (28)** |
| `$0065/$0066` | scrollAtSegStart | scroll snapshot at segment start |
| `$0067` | seg | segment index into `$6485` table, cycles 0..25 (**cap 0x1A=26**) |
| `$008A` | colCounter | column counter; `&$1F` gates the 32-col segment boundary |
| `$008E/$008F` `$006A` | stream2Ptr | 2nd tilemap stream pointer (from table +5) |
| `$0090` | scrollHi | coarse scroll position (visible window slide) |
| `$0091/$0092` | scrollFrac | sub-pixel scroll fraction; `+= $008D`/frame |
| `$0094/$0095` | streamPtr | BG tilemap stream pointer (from table +3) |
| `$0096` | waveStage | = `$5A`; hi-nibble selects the enemy-wave list (`$F0C9`) |
| `$0098` | segCountdown | per-segment countdown; reload = `leveldata[seg]+0` |
| `$009D/$009E/$009F/$00A0` | eventBytes | table +1/+2 event/wave bytes fed to `sub_FBE6` |
| `$00E6` | hwScrollX | hardware scroll-X register latch |
| `$0073` | playerGrace | player i-frame/grace byte (pin `0xFF` to survive in capture) |

| ROM addr | what |
|----------|------|
| `$6485` | leveldata table (26 × 7 bytes) — segment dur/evt/2 stream ptrs |
| `$6D3B..~$818B` | BG tilemap-column stream (ss.01e), stride 0xD0/record; **loops** |
| `$825B..` | 2nd BG stream (ss.02e), stride 0x80/record |
| `$6341` | sound-command sender (`evt` cues jump here) |
| `$647B` | evt=02 handler: `LDA #$17; JMP $6341` (a BGM cue, NOT a level end) |
| `$F0C9` | enemy-wave spawn-list pointer table (indexed by `$96` hi-nibble) |

| RE handler | file | role |
|-----------|------|------|
| `sub_A697` | `port/src/sub_A697.c` | scroll accumulator + boundary-driven calls |
| `sub_A730` | `port/src/sub_A730.c` | **segment/stage advance + leveldata reload** |
| `sub_A82F` | `port/src/sub_A82F.c` | per-boundary column blit |
| `sub_FBE6` | (code_c000) | reads `evt`, fires segment sound cues |
| `$8F4x` init | `port/src/sub_82A1.c` region | level (re)start: scroll/seg reset, `$5A←$54` |

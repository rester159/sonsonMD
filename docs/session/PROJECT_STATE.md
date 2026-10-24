# SonSon Native — PROJECT STATE (current)

**Status: DONE + PLAYABLE at 60fps, QA-verified.** Ongoing work = arcade-faithfulness refinement
(see NEXT_WORK.md). Branch `main`, working tree clean except a few untracked `tools/*.txt` RE dumps.

**Current build artifact:** `out/rom.bin` — 393,216 bytes, md5 `05384a6074be0297953f21b0b69c16d2`
(built Jun 19 12:16). Wired the spawn scheduler to the arcade `$FCD9` 87-wave schedule (commit `7b3dfcd`),
then refined it so classG/classH honor the placement-byte high bit → spawn from behind (commit `15ebd96`).
Verified 87/87 waves + cap-held + FPS 1.0000 via `tools/wave_sched_verify.lua`.

---

## What's implemented (all committed, all 60fps)

### Front-end (boot flow, a state machine in `src/main.c`)
SEGA splash (Revenge-of-Shinobi rolling-sheen logo + Sonic chant) → CAPCOM logo + register-exact FM
jingle → Arcade/Home **selector** ("PORTED BY RESTER 159, 2026") → Arcade **DIP-switch** screen *or*
Home **options** menu → the game. Files: `sega_splash.c`+`sega_logo_shinobi.c`, `capcom_splash.c`+
`capcom_logo.c`, `selector.c`, `dipswitch.c`, `options.c`. DIP/option choices feed a `GameConfig g_config`
(lives/bonus/difficulty/demo) the engine reads.

### Gameplay (`src/game.c`, `src/entity.c`)
- **Seamless looping level "ribbon":** all 8 baked level maps stream as one continuous looping ribbon
  (distinct VRAM bases, coordinate wrap at the loop). No per-level cold-fill. (`game.c` ribbon globals,
  `stream_column`, `ribbon_locate`, `segment_advance`, coordinate wrap.)
- **Terrain collision (RE-grounded, arcade `sub_A847`):** continuous-Y over **6 tiers**
  (`SS_TIER_Y = {200,168,136,104,72,40}`); RUN / CLIMB_UP / CLIMB_DOWN / FALL via per-column tier
  solidity (`src/ss_collision.c`, `tools/gen_collision.py`). The player seats on tiers, climbs on
  UP/DOWN, falls through gaps.
- **Enemy roster (faithful AI re-expressed from the RE, X-tracking + scripted/seated Y):** walkers &
  thrower (`ENEMY_A`/`THROWER`), `FLYER` (glide→dive homing), lobbed `ENEMY_SHOT` (parabola), `CREATURE_C`,
  `CREATURE_G`, `EMITTER`, `LARGE`, `ORB`, `COMPOSITE_E`. Per-class spawners + an on-screen cap of 10
  (arcade `$00A6`). **All these archetypes are arcade-faithful** (proven this session — see NEXT_WORK.md).
- **Spawn SEQUENCING (RE-grounded, arcade `$FCD9`/`$EF98`/`$EFBB`):** the enemy mix is driven by the arcade's
  exact 87-wave schedule (`inc/ss_wave_sched.h`), not a hand-authored program. `entity_schedule_tick`
  (`src/entity.c`) maps the wrapped `cameraX` to a wave-id (`cameraX*87/g_levelW`), reads `ss_wave_mask[wid]`,
  and spawns the masked classes (bit→`EntityType`) at the right edge. Commit `7b3dfcd`.
- **Fruit pickups** (4 types by color bank, escalating score), **score popups** (`ET_POPUP`, font-glyph
  hardware sprites), **scoring + extra lives** (`check_extra_life`, DSW2 thresholds), **death/respawn**
  (respawn at the right band edge, arcade `sub_B1C2`), **1-shot fire cap** (arcade single shot slot `$0E28`).

### Audio (`src/sound.c`, `src/sound_replay_data.c`)
Record-&-replay of the arcade AY-3-8910 register streams (recorded from the verified transliteration
driver): AY#2→PSG (BGM), AY#1→YM2612 (SFX), spare layer→YM ch3-5 (1-up). Advanced from a **V-Blank
callback at 480Hz** (decoupled from frame rate → music keeps tempo). Commands verified by tapping the
arcade sound latch: `0x18` BGM, `0x0B` fire, `0x08` enemy-defeat, `0x14` player-death, `0x02` pickup,
`0x0C` high-value pickup, `0x04` 1-up (layered), `0x16`+`0x10` round.

### Graphics pipeline (build-time)
`tools/gen_assets.py` (arcade gfx ROM → Genesis 4bpp tiles+palettes, byte-verified vs the port's
`build_char`/`build_sprite`), `tools/gen_level.py` (level maps → ribbon tiles), `tools/gen_anim.py`
(sprite animation frames), `tools/gen_collision.py` (per-column tier solidity). Palette fitting solved
(chars PAL0 / sprites PAL1). Output data: `src/ss_gfx.c`, `ss_level.c`, `ss_anim.c`, `ss_collision.c`.

---

## QA verification (this session)
- **30k-frame in-game soak** (`tools/soak.lua`, commit `6deece0`): 5 full seamless-ribbon loops,
  entity pool bounded (max 15 of 48, no leak), worst windowed fps ratio **0.9983** (sustained 60fps),
  score sane, **0 anomalies**.
- **Session re-entry soak** (`tools/re_soak.lua`, commit `cd9fb71`): 15 clean
  game-over→selector→new-game cycles, every re-entry resets cleanly, selector always recovers, **0 failures**.
- Re-confirmed: the only thing that ever "stops" the game is player death→game-over→selector
  (a responsive menu, not a hang).

---

## Probe addresses — CURRENT build (`out/rom.bin`, md5 3b0e7b51)
**These shift every build. Re-derive from `out/symbol.txt` after any rebuild** (`grep <symbol> out/symbol.txt`).
- `g_frameCounter` @ `0xFF007C` (volatile u32, +1/game-frame → the fps meter)
- `g_levelW` @ `0xFF0072` (u16; 0 = front-end, 11344 = in-game ribbon width)
- `g_game` @ `0xFF0808` (GameState; `Player`=0x1A, so: score u32 @+0x1A = `0xFF0822`,
  lives u8 @+0x1E = `0xFF0826`, round u8 @+0x24 = `0xFF082C`, stage u8 @+0x25, cameraX s16 @+0x26 = `0xFF082E`)
- `g_entities` @ `0xFF0834` (stride **0x2A**, active u8 @+0, type u8 @+1, x s16 @+2, fromSched u8 @+0x1C)
- `g_player_dead_request` @ `0xFF0076`
- **Soak keep-alive gotcha:** to survive a long run, **refill `lives` every frame** (`0xFF0836`); do NOT
  pin invuln (that write lands after `game_update`, which re-zeros it intra-frame, so the player still dies).

## Git history (recent, branch main)
```
7b3dfcd RE: wire native spawn scheduler to arcade $FCD9 87-wave schedule
63b9696 RE: decode arcade wave-spawn schedule ($FCD9) — roster faithfulness RESOLVED
cd9fb71 QA: session re-entry soak — 15 clean cycles
6deece0 QA: 30k-frame soak — 5 ribbon loops, 60fps, no leak/hang
6465d83 Enemy scores: measure 4 more via forced single-type spawns
```

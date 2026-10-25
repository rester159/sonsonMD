# Son Son II sound: driver contract and Genesis conversion

Source: `arcade/Son_Son_II_J_TEng.pce` (sha1 3b9e8d71942cb51a32cf62a1b84596249d991dc0),
driver code bank 0x0A `$8000-$8909`, data bank 0x0A `$890A`-bank 0x0B `$B999`.
All evidence comes from the project's offline research model (`tools/ss2`,
no MAME) running the original HuC6280 code.

| File | Role |
| --- | --- |
| `tools/ss2/sound/extract.py` | Decoder → `out/ss2/sound/ss2_sound.json` (schema `ss2-sound/1`) and `src/ss2_sound_data.c`; independent semantic timeline |
| `tools/ss2/sound/pce_ref.py` | Runs the original driver in the model (isolated calls or full game boot) and logs PSG writes / driver events |
| `tools/ss2/sound/capture_ingame.py` | Boot + scripted stage-1 play → ordered driver-call schedule + PSG log |
| `tools/ss2/sound/validate.py` | Original driver vs native sequencer vs decoder (all request ids + scenarios + in-game) |
| `tools/ss2/sound/host/` | Host builds (ASan/UBSan): `seqhost` (sequencer), `renderhost` (Genesis mapping with stubbed chips) |
| `inc/ss2_sound.h`, `src/ss2_sound.c` | Genesis API + virtual-PSG → YM2612/SN76489 renderer |
| `inc/ss2_sound_seq.h`, `src/ss2_sound_seq.c` | Native rebuild of the driver (interprets original byte code) |
| `inc/ss2_sound_data.h`, `src/ss2_sound_data.c` | Generated: period table, data region, FM patches |

Regenerate: `cd tools/ss2/sound && python3.14 extract.py && python3.14 validate.py`.

## 1. Scheduling (bank 0)

* VDC IRQ `$E0B9`: at `$E165` the sound block is skipped when `$6C|$6D|$C7 ≠ 0`.
  Otherwise `$E16D` maps bank 0x0A at `$8000` and 0x0B at `$A000` (TAM #$10/#$20),
  pops **one** entry from the ring `$2285[16]` (read index `$2295`, empty = `$FF`,
  the slot is reset to `$FF` when read), calls `JSR $8003` with it, then `JSR $8000`.
* Producers call bank 4 `$BBF5`: writes at `$2296` only if that slot is `$FF`,
  otherwise the request is **dropped** (ring full). Init `$E2EE` clears both indices
  and fills the ring with `$FF`.
* Call sites (`LDA #id / JSR $BBF5`): bank 0 `$00,$12,$1B,$20,$02,$21,$1A`
  (and stage music `LDA #$03 / ADC $69`), bank 2 `$11,$2A,$25,$1C,$17,$29,$18,$1D,$1E`,
  bank 3/4 `$28,$01,$1B,$10,$13`, bank 5 `$27`, bank 9 (player)
  `$0F,$15,$16,$19,$1F,$22,$23,$24,$25`. In-game capture: title `$00`, intro `$11`,
  text blips `$2A`, stage 1 music `$03`, after a miss `$F0` then `$0F`.
* `$8000` = `$8060`: if busy flag `$C8 = 0`: sfx tick `$8718`, then music tick `$8288`.
* `$8003` = `$8081`: `$C8 = $FF`, request `$808A`, `$C8 = 0`.

## 2. Driver RAM

| Addr | Meaning |
| --- | --- |
| `$C8` | busy (request in progress) |
| `$C9-$CF` | scratch (`$CF` = channel loop counter) |
| `$D0/$D1` | current record pointer; `$D2/$D3` instrument record |
| `$D4:$D5` | tempo accumulator (8.8); `$D6:$D7` step = `$3000 / T`; `$D8` last tick |
| `$D9` | active sfx priority; `$DA` sfx channel mask (bit `5-c` = channel c) |
| `$DB` | sfx "parsed this tick"; `$DC` sfx wait; `$DD` sfx loop counter; `$DE/$DF` sfx stream |
| `$6B` | (game) hold: music channels output volume 0 and do not advance (pause) |
| `$2DEC + 0x18*c` | music channel c (table below) |
| `$2E7C + 6*k` | sfx slot k for channel `5-k`: +0/1 period<<2, +2 slide, +3 volume<<3, +4 vol slide, +5 bit7 noise / wave+1 |

Music channel record: +0/1 stream ptr, +2 length counter, +3 gate counter, +4 gate
ratio (/256), +5 flags (b0 dot-next, b1 tie mode, b2 tied, b3 triplet, b4 instrument
changed, b7 noise), +6 volume offset (`v-31`), +7 envelope level (b3-7) and phase
(b0-1: attack/decay/sustain/release), +8 envelope rate accumulator, +9 instrument,
+A octave (b7 = +2 octaves), +B transpose, +C detune (signed period add), +D pitch
slide (signed, period<<2 per tick), +E/F period<<2, +10 LFO counter (b7 phase),
+11 vibrato delta, +12/13 vibrato accumulator, +14 tremolo delta, +15 tremolo level,
+16 repeat counter, +17 balance.

## 3. Requests (`$808A`)

* `id ≥ $F0` (`$811B`, low 2 bits): `F0` stop music + stop sfx, `F1` stop sfx,
  `F2/F3` stop music.
* Otherwise `id` is reduced modulo `count = [$89B4]+1 = 43`; header = `[$89B7 + 2*id]`.
* Header byte 0 = 0 → **music** (ids `$00-$14`; `$13` and `$14` share a header):
  stop music (`$8146`: tempo 0, `$D8=$FF`, clear records, `$815B`), then six channel
  stream pointers (0 = unused channel), balance `$AA` each.
* Header byte 0 = priority ≠ 0 → **sfx** (ids `$15-$2A`), byte 1 = channel mask,
  stream from byte 2. Ignored if `priority < $D9` (equal priority replaces).
  Channels of the old effect not used by the new one are returned to music
  (`$DA = old & ~new`, `$812C`), then `$D9/$DA` set, each new channel gets balance `$AA`.
* `$812C` stop sfx: `$DA ^= $3F`, `$815B` restores every channel whose bit is now
  clear (silence, music balance, reload the music instrument wave), clears
  `$D9-$DD` and the slots. `$815B` also writes main volume `$FF`, LFO regs 0.

Channel allocation: music owns all six channels; an active sfx masks its channels
(`$806B` drops music writes while bit 7 of the shifted `$DA` is set). Masks used:
`$01` (ch5), `$03` (ch4-5), `$07` (ch3-5). Priority 3..15.

## 4. Music (`$8288`)

Per frame `$D4:$D5 += $D6:$D7`; while `$D4 ≠ $D8`: `$D8++` and tick channels 0..5
(`$82A1`, select reg 0 written for every channel, even masked ones).
Tempo `T`: ticks per frame = `$3000/T/256` (T = 16..91 in the data).

Channel tick `$82DF`: hold → write vol `$80` and return. Length counter: decrement;
at 0 fetch (`$8317`); else gate counter decrement, at 0 → release (unless tied);
then envelope + output. **A fetch tick produces no output** (output resumes the
next tick).

Commands (`$841B`, table `$8420`, 19 entries; `$13-$1E` undefined, never used):

| op | args | effect |
| --- | --- | --- |
| 00 | T | tempo `$3000/T`; resets accumulator/`$D8` |
| 01 | g | gate ratio (note length × g / 256, min 1) |
| 02 | v | volume offset `(v&31)-31` |
| 03 | i | instrument; reloads the 32-sample wave (`$848B`) |
| 04 / 05 / 06 | – | toggle triplet / toggle tie mode / dot next note |
| 07 | o | octave (semitone base, keeps b7); 08/09 set/clear +24 semitones |
| 0A / 0B / 0C | b | transpose / detune / pitch slide |
| 0D | n, addr | repeat: n=0 jump; else first pass sets counter n, later passes decrement, fall through at 0 |
| 0E | addr | jump when counter = 1 (last pass), counter cleared; else skip |
| 0F | addr | jump |
| 10 | – | end of channel (record cleared, silenced) |
| 11 | – | toggle noise mode (off → reg7 = 0) |
| 12 | b | balance |

Notes (`byte ≥ $1F`, `$8321`): length code = byte>>5; base 3 (2 in triplet mode),
doubled code-1 times (3,6,12,24,48,96,192); dot adds the previous half. Pitch =
byte&31: 0 = rest (gate 0). Otherwise semitone = octave(+24) + transpose + pitch-1,
period = `$81C8[semitone]` (96-entry table, C1 = 3421) + detune, stored <<2; in
noise mode the value pitch-1 is the noise frequency. Tied notes keep the previous
period and envelope. Envelope and LFO restart on a new instrument, or on each
untied note when instrument byte +$24 bit 7 is set.

Instrument record (`$8A0D + 40*i`): 32 wave samples, +20 attack, +21 decay,
+22 sustain level, +23 release (rates: accumulator +rate per step, one ±8 level
step per 16; 0 = skip phase), +24 LFO half-period (b7 retrigger), +25 vibrato
delta, +26 tremolo delta, +27 unused.

Output (`$866E`): vibrato accumulator += delta; tremolo level += delta (clamped);
period += slide; written period = (period + vibrato)>>2 (regs 2/3, or reg 7 =
value|$80 in noise mode); volume = min(max(tremolo>>3,1), level>>3) + offset,
clamped at 0, reg 4 = vol|$80. LFO: every period ticks the tremolo delta is
negated, the vibrato delta every second time (triangle).

## 5. Sound effects (`$8718`)

One shared stream for all channels of the effect. Per frame, for slots 5..0 → channels
0..5 in mask order (`$8718` walks bit 0 = channel 5 first): if not parsed this tick
and wait ≠ 0 → apply period slide and volume slide; else parse: `00 n` wait n
(continue parsing), `01 n addr` loop (n=0 jump), `02/03` end (→ `$812C` with the
**partially shifted** `$DA`, a source quirk reproduced exactly), `04 b` balance,
`$80|noise b1 b2 slide wave vol vslide` = this channel's event (b1 = period table
index or noise freq, `$FF` keeps; b2 signed period add; vol `$FF` keeps; wave 0-4 from
`$890A` reloaded when it changes). After the walk: wait--, parsed = 0.

## 6. Genesis representation (choice)

Per the sgdk-audio strategy matrix: the source is a 6-voice wavetable PSG with
software envelopes, and its *sequencer* is fully recovered, so the best fit is
**native reconstruction**: the original byte code (12,432 bytes) is interpreted by
a native C rebuild of the driver that produces the exact PCE register stream, and
a renderer maps that stream to the Genesis chips each frame. Rejected:
register-stream/VGM (no PCE→YM translation exists; the stream is 2.5 M writes for
the validated material and would still need the same mapping), PCM (ROM cost,
DAC/Z80 contention with no fidelity need), XGM2 (would require converting to FM
events offline anyway and adds a Z80 driver; our music needs no PCM).

* PCE channel c → YM2612 channel c (all six FM channels, DAC off).
* Timbre: each 32-sample wave → algorithm 7 with four sine carriers at the four
  strongest harmonics (MUL 1..15) and TL from their amplitudes (+6 dB headroom).
  Retained AC power: 81-100 % (ramps/saws lowest: instruments 2, 10, 11, 25).
* Envelopes stay in software (driver volume per tick): FM AR=31, SL=0, RR=15;
  volume → carrier TL: 2 TL/volume step (1.5 dB), 4 TL/balance and main-volume
  step (3 dB). Pan: L/R enabled when that side's balance nibble ≠ 0.
* Pitch: `fnum<<block = K/period`, K = 4404020 (NTSC YM 7.670453 MHz) or
  4444559 (PAL); ≤ 1.6 cents error (host-checked).
* Noise (channels 4/5 noise mode) → SN76489 white noise clocked by tone 3,
  N = 2·(NF^31); louder of the two noise users wins. SN tone channels unused.
* Timing: one sequencer frame per 60 Hz frame; on PAL every 5th tick runs two.

## 6b. Sequencer performance (68000)

`src/ss2_sound_seq.c` keeps the source's arithmetic but not its memory
layout: typed channel records (word periods, pointer-based stream fetch,
instrument bytes +$20..+$24 cached at instrument change, `ss2_snd_period`
as a word table), the $DA mask passed as a per-channel flag, closed-form
envelope rate steps, and a per-channel output cache (`o_per`/`o_vol`,
`recalc`) that recomputes a value only when one of its inputs changed.
Every PSG write of the source is still emitted in source order. Two facts
used: the LFO counter is dead state while both LFO deltas are 0 (only
`$83B0` makes them nonzero and it restarts the counter), and instrument
bytes are ROM constants.

Target profile (`tools/ss2/profile.sh` workload, stage 1 music `$03`,
built on commit f178d98 with only the sequencer files swapped; average
shown in tenths of a line, max in lines):

| Section | Before | After |
| --- | --- | --- |
| slot 5 `music_tick` | 46.7 / 76 | 15.3 / 33 |
| slot 4 ring pop + `sfx_tick` | 4.4 / 18 | 3.6 / 13 |

## 7. Caller contract

* `snd_shutdown()` (SonSon) before `ss2_sound_init()`; `ss2_sound_shutdown()` before
  `snd_init()`. `ss2_sound_init` loads `Z80_DRIVER_NULL` and holds the Z80 bus,
  exactly as `src/sound.c`; every YM write is additionally bracketed by
  `Z80_getAndRequestBus`. No Z80 RAM, DMA or banking is used.
* `ss2_sound_request(id)` = bank 4 `$BBF5` (ring of 16, drop when full).
* `ss2_sound_tick()` once per frame where the source IRQ ran `$E16D` (skip when
  `$6C|$6D|$C7`); same context as requests (not interrupt-safe against them).
* `ss2_sound_set_hold(v)` mirrors RAM `$6B` (pause: the game sets it and requests `$1A`).
* Cost: code 15 KB + data 13 KB (m68k -O3, before LTO), 314 bytes RAM. Per-frame
  68000 time not yet measured on target.

## 8. Validation (`validate.py`, research model = ground truth)

| Layer | Evidence | Result |
| --- | --- | --- |
| Static decode | every reachable byte-code path of 43 ids, both loop edges | 0 undefined opcodes / bad pointers; 11,822 commands; every statically reachable music command executes (decoder coverage) |
| Commands/events | decoder timeline vs original driver events (`$8321`/`$881E` traced) | all cases identical incl. frame (e.g. stage 1 `$03`: 1381/1381) |
| Pitch semantics | decoder base period vs driver record +E/F at `$8398` | all pitched non-tied notes identical (e.g. `$03`: 1024/1024) |
| Native sequencer | host `seqhost` vs original driver, every PSG write (frame, reg, channel, value) in order | **2,599,663 / 2,599,663 writes identical** over: 43 ids (music run to full code coverage + 1200 frames, 2400-4929 frames), F0/F1/F2, mixed music+sfx+priority+pause (direct and through the ring), and the in-game boot → title → intro → stage 1 play with a pause (3400 frames, 142,296 writes, 1,624 events, driven by the game's own ordered request/tick calls); 0 data faults |
| Renderer (before the Z80 YM writer; `renderhost` no longer host-compiles and is skipped) | host `renderhost` (stub chips), all 43 ids × 3000 frames | FM key state equals PCE tone audibility on all 291,019 sounding channel-frames (0 mismatches); pitch error ≤ 1.64 cents; 0 YM writes without the bus held; ≤ 66 YM writes/frame |
| Target build | `bash build.sh` | builds; files compile warning-free with `-Wall -Wextra` |

**Not verified**: audible timbre/balance (no listening), real YM2612/SN76489
output, PAL timing on hardware, per-frame 68000 cost, noise colour/rate mapping
(PCE noise clock formula taken from chip documentation, not measured),
HuC6280 volume curve (1.5/3 dB steps from documentation).

## 9. Fidelity gaps

* Waveform timbre approximated with 4 partials (bright ramps lose 8-19 % power);
  phase relationships and aliasing character of the 32-sample tables are lost.
* Asymmetric balance (e.g. `$3A`, `$A3`) collapses to centre or hard-pan.
* Two simultaneous noise voices share one SN noise generator.
* PCE DDA/LFO hardware modes are unused by this driver (reg 8/9 always 0).


## Integration note (gameplay)

The tick gate in the caller contract applies to the VBlank path only. During
gameplay the HUD raster-split IRQ (`$E122..$E148`) jumps to `$E16D`
unconditionally, so the driver runs every frame there; `$6C` is `$88` while
the split is active. `src/ss2_main.c` therefore pops the ring and ticks every
gameplay frame. Non-split screens (title, intro) must use the VBlank rule.

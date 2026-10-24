# Son Son II (PC Engine) → Genesis port notes

Source: `arcade/Son_Son_II_J_TEng.pce` (HuCard, 256 KiB, English patch;
sha1 3b9e8d71942cb51a32cf62a1b84596249d991dc0). Son Son II is a PC Engine
title (NEC Avenue 1989), not an arcade board. It shares this cartridge with
SonSon via the game chooser (`src/game_select.c`).

## Tooling (tools/ss2)

| Tool | Purpose |
| --- | --- |
| `huc6280.py`, `pce.py`, `render.py` | Offline research model: HuC6280 interpreter + VDC/VCE/pad/timer, renderer. Never part of the cartridge. **Do not use MAME** (owner's instruction). |
| `run.py ROM OUT FRAMES a-b:KEYS...` | Scripted play, PNG frames. Keys `U D L R 1 2 S N` (N = RUN). |
| `snapshot.py` | Deterministic stage-1 start snapshot (frame 1560) cached in `out/ss2/`. |
| `disas.py BANK:ADDR [n] [mpr overrides]` | Disassembler with MPR context. Full bank dumps live in `out/ss2/bankNN.asm`. |
| `watch.py`, `coverage.py` | Who writes a RAM address; executed-code coverage. |
| `capture_pairs.py OUT FRAMES SCRIPT [ENTRY EXIT]` | Work-RAM before/after a routine call each frame (default: player update `00:EC57..EC5A`). |
| `hosttest/run.sh PAIRS [ROUTINE]` | Replays pairs through native C on the host (ASan/UBSan) and diffs all 8 KiB (zp $26-$3F scratch and stack page masked). |
| `gen_ss2.py` | Converter → `src/ss2_data.c`, `src/ss2_rom.c`, `inc/ss2_data.h`. |
| `shot.py FRAMES SCRIPT SHOTS` | Headless Genesis Plus GX (libretro core via dlopen) captures of `out/rom.bin`. Keys `U D L R S a b c`. |

## Architecture decisions

* **State image.** Native logic runs over `ss2_ram[0x2000]`, laid out as the
  HuCard work RAM (`inc/ss2_ram.h`, named accessors). Reason: byte carry chains,
  shared arrays and out-of-range indexing (e.g. `$22C7-1` with no hitter) match
  the source exactly and validation is a byte diff. This is native C, not CPU
  emulation.
* **Control flow** mirrors source routine boundaries: JMP = tail call,
  JSR = call. Each block cites its source address.
* **Unported paths** call `ss2_unported(addr)`; the host test reports frames
  that reach them instead of passing silently.
* **Tables** are read through `ss2_rom_byte()` with the gameplay MPR mapping
  (banks 1-4 at $4000-$BFFF, bank 9 at $C000, bank 0 at $E000) from
  `src/ss2_rom.c` (banks 0,1,2,3,4,6,7,9). Trim to used tables later.
* **Display**: H32. PCE frame 256x240 = 8 black lines + 48-line HUD +
  184-line playfield. Genesis drops the black band and the bottom 8 lines; HUD
  in the window plane (rows 0-5). Genesis line g shows PCE line g+8; playfield
  vscroll = cam_y + 13 (i.e. world y at line 48 = cam_y + 61).
* **Palettes** (stage 1): all PCE palettes use 8 entries. BG palettes 5-15 →
  Genesis lines 0-1 (exhaustive partition + weighted closest-colour merge,
  visually indistinguishable); sprites + HUD → lines 2-3 (lossless, 25 colours).

## Source formats (verified)

* Stage map: bank `0x0C | s>>1`, offset `(s&1)*0x1000`; 8x8 screens of 8x8
  metatile bytes; world 2048 px square, wrapping. 704/704 live BAT cells match.
* Metatiles: bank `6 | s>>2` at `$C000`+ (`$5D/$5E`), 8 bytes = 4 blocks
  (TL TR BL BR) of `(b0, b1)`; tile `0x180+0xC0+((b0+6)&F8)*4+((b0+6)&7)*2`,
  palette `b1>>4`, collision attribute `b1&0F`.
* Collision attributes (bank 3 `$8307`): 0 empty, 2 climbable vine (passable,
  sets `$61`), 3 solid + `$62` (jump-kind change), 4 solid hazard (`$63`),
  other non-zero solid.
* Stage tiles: list at bank 1 `$548F` (7 stages) of (bank, blocks, addr);
  0x60-byte 3bpp 16x16 blocks loaded to VRAM `$1800` as an 8-wide block grid.
* Palettes: BG 0-4 common at ROM `0x2000`, stage BG 5-15 via table at ROM
  `0x20D0`; sprite 0-7 common at `0x2050`, stage sprite 8-15 via `0x25AE`.
* Player frames: bank `0x10`, 0x180-byte 32x32 3bpp frames (21); animation
  table bank 9 `$DB23` (set `$22B2`, state `$22D8`, step `($22B3>>3)&3`,
  entries `(ptr lo, ptr hi, attr hi)`).
* Pad: `$16` held / `$20` pressed; bits L D R U Run Sel II I. Run+Select = reset.

## Frame structure (bank 0)

Task 1 (`$EC3B`): wait VBlank; player (`04:$BD16` demo input, `09:$C000`,
`09:$D03D` projectiles); frame select `$EC5D`; upload `02:$79A2`; BCD play
clock `$66-$68`; `$E841` map streaming. Task 2 (`$ECCF`): 32 object slots
(`$EE01`, struct-of-arrays at `$245A + n*$20`), player position trail
`$22DB-$22E4`, collisions `03:$8732`, HUD `02:$7649/$7688`, sprites.

## Status

| Subsystem | State |
| --- | --- |
| Level decode/render/stream | done, matches PCE frame |
| Player `$C000` | rebuilt; 700/700 trace frames exact (walk/jump/hurt/staff) |
| Cloud `$CE74`, magic `$D1E8`, push `$7FD8`, `$C4B8`, `$C4D7` | unported markers |
| Projectiles `$D03D` | pending |
| Object engine `$EE01`, enemies (banks 10, 23), collisions `$8732` | pending |
| HUD, sound, title, stages 2-7 | pending |

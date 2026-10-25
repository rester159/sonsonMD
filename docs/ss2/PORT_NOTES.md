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
| `capture_vbl.py OUT K FRAMES SCRIPT [WX WY]` | VBlank-to-VBlank pairs (multi-frame sequences such as doors); replay with `SS2_CONTINUOUS=1 hosttest/run.sh PAIRS ss2_frame_vbl`. WX WY teleports the player first. |
| `capture_sprset.py` | Pairs around the sprite-set loaders `$774F/$77A5/$77D0`. |
| `gen_shop.py` | Shop scene background (from a saved research-model shop screen, `research/npc/cap.py SAVE=240 SAVEPKL=out/ss2/shop_s1.pkl`) -> `src/ss2_shop_data.c`. |
| `research/bank05,09,17/` | The enemy agents' spawn lists, injected captures and replays. |
| `research/npc/` | Townsperson/shop captures (`cap.py`: townspeople spawn only off-screen, so a two-phase warp; `runs.sh` the 38 recorded conversations and shop visits with RAM pokes for money/flags; `rep.sh`, `difsum.py`). Replays use `SS2_MASK_SOUND=1` (the host build has no sound driver). |

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

## Display additions (renderer, src/ss2_main.c)

* **Dialogue window / HUD icons.** Logic queues map writes in
  `ss2_bat_queue` (PUT at a world tile, RESTORE a rectangle from the map,
  HUD icon table). Window cells (PCE BG tiles `$180+ch`, palette 1) go into
  the plane A ring; glyph tiles are uploaded on demand into tiles
  1440-1535 (SGDK's font slots, unused in SS2). Item-box icons (`$BADE`
  tables `$4E07`-`$4EB3`) are resident after the sprite tiles.
* **Plane B** shares plane A's map (`$E000`) and scroll, so it sits
  invisibly behind it; its table area `$C000-$CFFF`, `$D800-$DFFF` (under
  the H40 window) and `$F600-$FFFF` hold the glyph cache (tiles 1504-1567),
  resident icons (1568-1663) and the run-time sprite cache. On exit
  plane B returns to `$C000` (SGDK reloads its font there for SonSon) and
  `$D800-$DFFF` is cleared.
* **Run-time sprite cells.** Static tables cover the cells the stage's own
  sprite sets load. Cells whose source (`ss2_cell_bank/src`) differs from
  that default (interiors `$77A5`, `$77D0`, staff `$79F7`), and palette 13
  after a door loads an interior palette (`ss2_pal13_variant`), are
  converted on demand from the HuCard pattern through per-stage colour
  maps (`spr_cmap`, `spr_cmap13`; nearest colours where a palette is not in
  the stage fit) into 8-tile blocks (18 at `$D800`/`$F600`, LRU, at most 4
  conversions per frame).

* **Shop scene** (`ss2_shop_screen`): the source's shop picture (one for
  all shops; camera 0, BAT rows 7-29 = world tiles) replaces plane A and
  CRAM lines 0-1; prices and the staff digit are ported BAT writes; the
  shopkeeper, goods icons and cursor are built into the sprite table.
* **Diagnostic builds** (`build_native.py -D ...`): `SS2_DBG_STAGE=k`,
  `SS2_DBG_WX/WY` (teleport), `SS2_DBG_NPC_X/Y` + `SS2_DBG_P2X/Y` (two-phase
  warp so off-screen spawners fire), `SS2_DBG_LOCK7F`, `SS2_DBG_KEY`,
  `SS2_DBG_MAGIC=n`, `SS2_DBG_CLOUD`.

## Tasks (bank 0 $E1A5 scheduler) -- src/ss2_task.c

Four slots in zp $00+4n: state (0 free, 1 waiting, 2 running, 4 ready, 8
new entry), timer, resume data. VBlank IRQ counts waits down ($E14B); the
idle loop resumes ready slots from slot 0 after every yield. During play:
slot 0 main loop ($EC3B: $FDD9 pause, body $EC42-$EDA2, then if no
VBlank yet: wait 1, $FE5A weapon box, $EDB7 flash, loop), slot 1 title RUN
handler idling at $E2A6, slot 3 score task ($FD05). Native slots are
coroutines (68000 register swap / host ucontext); `ss2_wait(n)` = $E1FE,
`ss2_task_restart(fn)` = the source's "LDX #$xx / TXS" restarts (e.g. a
door ends with PLA x4 / JMP $EDB0). Host replay modes: per-frame
(`ss2_frame_tick_then_vblank`) and continuous through the real scheduler
(`SS2_CONTINUOUS=1 ... ss2_frame_coro`), and VBlank-bracketed
(`ss2_frame_vbl`). `ss2_busy(n)` models source work that holds the CPU
across n VBlanks without yielding (the `$EBE1` redraw crosses 5): VBlanks
count, no other task runs. A restart records the new entry and the
scheduler rebuilds the slot's context from its own stack (building it on
the stack still in use corrupted the return path).

## Frame structure (bank 0)

Per frame: VBlank IRQ (pad $FEDE, $12++, $14=0, task countdown, sound
ring pop + driver tick -- the raster-split IRQ path runs sound every
gameplay frame), then slot 0 (player, frame select, clock, spawn scan
$E845->$BC10, objects $ED0D, trail, spark, respawn timers, HUD bars),
slot 3 (seed advance, score/money awards, extra life, hi-score).

## Parallel work (agents, isolated worktrees)

Per-bank object code lives in its own file so branches merge cleanly:
ss2_obj05.c + ss2_objfix.c (bank 5, fixed bank 3), ss2_obj17.c +
ss2_obj07.c + ss2_objfix4.c (banks $17/$07, fixed bank 4), ss2_obj09.c +
ss2_obj0f.c + ss2_obj1f.c. Sound driver: ss2_sound*.c (see SOUND.md).

## Toolchain hazard

GCC 16.1 -O3/LTO emits `move.b (aN)+,d(aN,...)` for some copy loops; the
68000 updates aN before computing the destination (bytes land one too
high). Hit: SS2 RAM bootstrap copy, Z80 list copy, SGDK libmd Z80_upload
(also in SonSon v1.0: SEGA chant PCM driver load). tools/check_autoinc_hazard.py
runs after each link; keep load and store separate (empty asm barrier).

## Status

| Subsystem | State |
| --- | --- |
| Level decode/render/stream | all 7 stages, verified on target (VRAM 764-1410 of 1488 static tiles) |
| Player `$C000` | exact (cloud, magic, push, $C4B8, $C4D7 pending) |
| Shots/magic `$D03D` (7 slots `$22E5`), cloud, revive/trap paths | exact on all shot traces (research/shots); drawn on target (shots = PCE sprite entries 7-13, cloud = entry 1; freeze flash and sweep via `$2201`) |
| Object engine, spawner, items, containers, score task, HUD bars | exact on traces |
| Enemies banks 5/7/9/$0F/$17/$1F + fixed 3/4 | exact on agent traces (see research/); boss calls `$6984` (message), `$6F40/$6F73` (defeat/drops), `$E1E2` task, `$F617` stage clear pending |
| Task scheduler | exact (per-frame, continuous, VBlank-bracketed) |
| Sprite sets `$774F/$77A5/$77D0`, staff art `$79F7` | RAM side exact (all 7 stage loads); cell sources recorded for a runtime sprite cache (renderer still uses the static census LUT) |
| Doors `$7DB5` | stage-1 round trip 289/299 exact (rest: redraw in-progress bytes); on target: fade out, redraw, fade in. Locked-door message `$BADE` pending |
| Fades `$71BA..$7297` | exact in door traces; target scales the Genesis palette by level |
| Sound | driver PSG-exact; YM via Z80 list |
| Dialogue window `$6984`/`$69EE` (boss speech) | exact on stage 2/3 boss traces; renders on target |
| HUD item icons `$BADE`, key items (kinds 10-14, 17, 26-28), stand-on items (kind 2) | ported |
| Townspeople, fortune teller, shops (item kind 29, bank 3 `$997F`-`$9E7A`, shop `$9E7B`-bank 4 `$A665`) | 31 tracer conversations exact (all 7 stages, every choice continuation, items given, money paid/lost); 7 shop visits (all 14 goods kinds, price rises, short money, owned goods) exact up to the exit, where `$7882`+`$EBE1` end at a VBlank boundary (source 8 or 9 VBlanks, native 3+5), replay then a frame behind; shop scene drawn on target (picture, prices, shopkeeper, icons, cursor) |
| Item kind 11 (`$81A6`) | wired to the shots system's `$D86A` |
| Pause magic select `$FE0C`, weapon box `$FE5A`, freeze flash `$EDB7`, sweep BG `$EDA0` | ported (shots traces m2 exact) |
| Stage init/clear, title, intro, death/continue, ending | pending (SS2 starts from per-stage RAM bootstrap) |
| Cartridge | SELECT GAME -> SS2 -> START+A -> SELECT GAME -> SonSon ARCADE/HOME verified headless |

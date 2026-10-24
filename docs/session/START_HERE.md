# SonSon Native — Session Resume: START HERE

You are picking up an in-progress project: a **from-scratch NATIVE Genesis/Mega Drive
re-implementation of Capcom's arcade SonSon (1984)**, written in C on SGDK. It is
**done and playable at 60fps**; ongoing work is incremental arcade-faithfulness refinement.

Read these three docs in order:
1. **START_HERE.md** (this file) — environment, how to build/run/verify, the working doctrine.
2. **PROJECT_STATE.md** — exactly what is built, verified, and where everything lives.
3. **NEXT_WORK.md** — the prioritized to-do, with the reverse-engineering detail needed to execute the top item.

There is also a detailed chronological project log in auto-memory at
`C:\Users\edang\.claude\projects\D--genesis-ports--capcom-son-son\memory\sonson-native-project.md`
(loaded via MEMORY.md). This resume kit is the clean, current-state summary; the memory is the full history.

---

## The project in one paragraph
The sibling project `..\sonson` is a *transliteration* port (it emulates the arcade 6809 CPU
in C — correct but ~5x too slow, caps ~48fps). This project, **`sonson_native`**, throws that
out and rebuilds SonSon as a true native Genesis game: arcade graphics are converted to Genesis
4bpp tiles+palettes **once at build time**, and the game writes Genesis VRAM/SAT directly. Result:
**real 60fps**, which the transliteration could never hit. The arcade behavior is reproduced
faithfully by reverse-engineering the original and re-expressing each routine in native C.

## Locations
- **Repo / working dir:** `D:\genesis ports\_capcom\son son\sonson_native` — git, branch **main**.
- **This resume kit:** `D:\genesis ports\_capcom\son son\session_resume_docs\`
- **Arcade ROMs (RE ground truth, gitignored):** `sonson_native\arcade\` — `ss.01e`/`ss.02e`/`ss.03e`
  (6809 program, map: ss.01e=$4000, ss.02e=$8000, ss.03e=$C000), plus char/sprite/PROM ROMs.
- **Transliteration oracle (behavioral reference):** `..\sonson\port\src\sub_*.c` + `..\sonson\census_semantic.toml`.

## Toolchain
- **Build:** `bash build.sh` from the repo root → produces `out/rom.bin` (uses
  `D:/portForge/tools/sgdk/makefile.gen`). Build symbols land in `out/symbol.txt`.
- **Run (play it):** open `out/rom.bin` in **BlastEm** (owner's emulator) or any Genesis emulator.
- **Verify (headless):** MAME at `D:/portForge/tools/mame/mame.exe`. **Use the `genesis` driver = NTSC USA**
  (the `megadriv` driver is PAL — wrong timing). Example:
  `mame genesis -cart out/rom.bin -autoboot_script tools/<verify>.lua -skip_gameinfo -nothrottle -video none`
- **RE / disassembly:** `unidasm.exe` at `D:/portForge/tools/mame/unidasm.exe`, arch `m6809`
  (e.g. `unidasm ss.03e -arch m6809 -basepc 0xC000`). MAME `sonson` driver = the real arcade machine,
  for live arcade introspection (drive it via `:SYSTEM` Coin/Start inputs; in-game when `$0002==2`).

## How 60fps is measured (deterministic, not eyeballed)
The ROM exports a `volatile u32 g_frameCounter` incremented once per game-frame. A verify lua reads it
over a window of emulator frames; ratio ≈ 1.000 means full 60fps. **Probe addresses shift every build —
always re-derive from `out/symbol.txt`.** (Current build's are in PROJECT_STATE.md.)
Front-end nav for harnesses: pulse the `:ctrl1:mdpad:PAD` field "P1 Start" while `g_levelW==0`.

---

## THE WORKING DOCTRINE — non-negotiable (the owner will hold you to this)
Decide the track **per bug, up front**:

- **BEHAVIOR** (position, animation, HP, collision, scripts, spawns, sequencing, warps, shop logic):
  **start in the RE.** Decode the actual arcade routine = ground truth, then read the native code, find
  where it deviates, fix the native to match — **citing the disasm $address**. No symptom-first, no
  guessing, no probe-first.
- **COLOR & FRAMERATE:** there is **no RE ground truth** (the Genesis VDP/palettes/68k aren't the arcade) —
  judge these by *perceived quality on real Genesis hardware*, never by RE-matching.

And the **Iron Law:** *no fix without a root cause confirmed by evidence.* This session nearly removed six
valid enemy archetypes on an unconfirmed hunch; a coverage-independent static decode proved them faithful.
The doctrine is what caught it. Honor it.

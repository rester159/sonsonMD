# Boot and Son Son II audio candidate — 2026-10-26

External report: Collection V1.0 boots both games on Model 1 VA3 and Model 2 VA0, but Son Son II is quiet. Model 3 VA1 can black-screen until repeated resets; Son Son II is reported worse. Collection Son Son I looping appears corrected; standalone V1.0 still has the reported audio issue and Model 3 boot difficulty. Tester ROM hashes and cart/firmware are unavailable. Do not assign an exact artifact or physical cause from that report alone.

## Changes

`src/boot/sega.s` no longer infers initialized RAM from controller-register contents. Every cartridge entry checks the hardware version, writes TMSS where applicable, and enters SGDK `_start_entry` to clear RAM and reload initialized data. Previously nonzero I/O selected `_reset_entry`, skipping that initialization and TMSS. This also makes hardware reset restart static game/audio state. The application ignores `hardReset`; persistent settings remain in SRAM and are reloaded by `save_init`.

The previous CAPCOM boot hardening (a8cfcff) remains included. The new fix addresses an additional startup risk and does not establish which path failed on the tester's Model 3.

`src/ss2_sound.c` recovers the extractor's eight TL units (6 dB) of reserve from the final FM carrier attenuation. Algorithm 7 uses four carriers, so their relative amplitudes are preserved unless an attenuation reaches the saturation floor. Operators encoded silent (TL 127) remain silent; PSG noise levels and sequencer timing are unchanged. No other game's mix is changed.

## Evidence

- MAME Genesis inherited-state fixture: after normal startup, fill work RAM with A5 and set controller control registers nonzero, then re-enter the ROM reset vector. Two entries each: baseline leaves initialized `g_config` as A5A5A5A5; candidate restores DFCB0301 both times. This reproduces an initialization failure, not a physical Model 3 circuit.
- Linked startup disassembly: the only entry path reaches `_start_entry`; controller-register tests are absent and conditional TMSS unlock remains.
- MAME controller-driven smoke: both games reach gameplay, soft reset, and reach gameplay again. Screenshots retained under `out/hardware-20261025/game1` and `game2`. Separate stale-state captures progress through the CAPCOM screen.
- Genesis Plus GX actual 68000/Z80/YM execution: 43 sound requests, six seconds each, followed by 60 seconds of stage music with staff/jump/hurt effects and stop settling. Same fixture before/after; no renderer stubs.
- Mixed RMS: 893.775 -> 1574.391 (+4.918 dB). PSG noise is unchanged, so mixed gain differs from FM-only 6 dB.
- Candidate peak across all cases: 12213/32767 (8.57 dB digital headroom); zero clipped samples; post-stop peak zero.
- Native build passes its existing code-generation hazard guard. It reports the pre-existing allowed SGDK Z80_upload hazards; this change does not repair those SDK routines.

These are bounded emulator checks, not whole-game certification, analog output validation, or a physical Model 3 pass. PAL audio and cartridge-specific behavior were not tested in this run.

## Artifacts

`dist/SonSonCollection-v1.0-hardware-candidate.bin`, `out/rom.bin`, and the build script's automatic `dist/SonSonCollection-v1.0.bin` copy are identical:

SHA-256 `b3d3f8b550f8c73995be2a80d84d73a198649ac4665f7485e9e7c68180b861fe`

The standalone `dist/SonSonMD-v1.0.bin` is not changed by this Collection build.

Evidence, baseline ROM, logs, CSVs, fixture source/build and screenshots are in `out/hardware-20261025/`. Baseline captured from the current pre-change `out/rom.bin`: `cc103643f8eb5e3cbb74382cbd2890118621c6e930a62db1bd605bb04c42dba5`; it is not claimed to be the tester's ROM.

## Reproduction

Run `python3 tools/build_native.py` for the Collection. Boot fixture:

```
mame genesis -cart out/rom.bin -video none -sound none -nothrottle -skip_gameinfo -autoboot_script tools/hardware_regression/boot_probe.lua -seconds_to_run 44
```

Use fresh isolated MAME cfg/nvram/snapshot directories when comparing artifacts. The fixture reports `DATA_REINITIALIZED ... pass=true`; its RAM writes are intentional diagnostic injection. `game_probe.lua` uses `SS_GAME=1` or `SS_GAME=2`, controller input and soft reset; allow 65 emulated seconds. MAME restarts the Lua script on reset, so the wall limit bounds the second pass.

Audio fixture: build an isolated project with the same SGDK startup, `ss2_sound*.c`, required headers, and `tools/hardware_regression/audio_fixture_main.c` as main. Compile `audio_probe.c` with host `cc -O2`. Run it with the Genesis Plus GX dylib path, fixture ROM path, and linked `probe_case` address from its symbol map. It prints per-case sample count, DC-removed RMS, peak, and clipped sample count; case 43 is overlap, case 44 silence. Use identical source and SDK apart from the renderer gain change for the comparison. The ready-built pair and results are retained in the evidence directory.

The HuC6280 reference also shows a nonuniform balance table, whereas the existing port uses a simple 3 dB balance step. That is a separate fidelity issue, not changed here or claimed as the cause of this report. Reference: https://github.com/mamedev/mame/blob/master/src/devices/sound/c6280.cpp

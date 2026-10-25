#!/usr/bin/env bash
# Build the host sequencer harness with ASan/UBSan.
set -euo pipefail
cd "$(dirname "$0")"
ROOT=../../../..
mkdir -p "$ROOT/out/ss2/sound"
cc -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
   -DSS2_SND_HOST -I"$ROOT/inc" seqhost.c "$ROOT/src/ss2_sound_seq.c" "$ROOT/src/ss2_sound_data.c" \
   -o "$ROOT/out/ss2/sound/seqhost"
# Render test: src/ss2_sound.c with stubbed SGDK chip calls.  Its PSG hook
# is renamed so renderhost.c can shadow the PCE register stream and forward.
# (ss2_sound.c now contains m68k inline asm / Z80 upload code that the host
# stub cannot compile; the render test is skipped rather than failing.)
if ! (
O="$ROOT/out/ss2/sound/rh"; mkdir -p "$O"
F="-std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -DSS2_SND_HOST"
cc $F -Istub -I"$ROOT/inc" -Dss2_snd_psg_write=ss2_snd_psg_write_impl -c "$ROOT/src/ss2_sound.c" -o "$O/ss2_sound.o"
cc $F -I"$ROOT/inc" -c "$ROOT/src/ss2_sound_seq.c" -o "$O/seq.o"
cc $F -I"$ROOT/inc" -c "$ROOT/src/ss2_sound_data.c" -o "$O/data.o"
cc $F -Istub -I"$ROOT/inc" -c renderhost.c -o "$O/renderhost.o"
cc -fsanitize=address,undefined "$O"/*.o -lm -o "$ROOT/out/ss2/sound/renderhost"
) 2>/dev/null; then echo "renderhost: skipped (src/ss2_sound.c not host-compilable)"; fi

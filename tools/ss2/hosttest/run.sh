#!/bin/bash
# run.sh PAIRS [ROUTINE] -- build the host replay test and run it
set -e
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
OUT="$ROOT/out/ss2/hosttest"; mkdir -p "$OUT"
R="${2:-ss2_player_update}"
cc -O1 -g -fsanitize=address,undefined -DROUTINE=$R -I"$ROOT/tools/ss2/hosttest/shim" -I"$ROOT/inc" \
   "$ROOT/tools/ss2/hosttest/test_routine.c" "$ROOT/src/ss2_player.c" "$ROOT/src/ss2_logic.c" "$ROOT/src/ss2_rom.c" -o "$OUT/test_$R"
"$OUT/test_$R" "$1" "${3:-8}"

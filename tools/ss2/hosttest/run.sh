#!/bin/bash
# run.sh PAIRS [ROUTINE] [MAXREPORT] -- build the host replay test and run it.
# Compiles every SS2 logic source (src/ss2_*.c except presentation, sound
# and generated graphics) with ASan/UBSan against a genesis.h type shim.
set -e
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
OUT="$ROOT/out/ss2/hosttest"; mkdir -p "$OUT"
R="${2:-ss2_player_update}"
SRCS=()
for f in "$ROOT"/src/ss2_*.c; do
    case "$(basename "$f")" in
        ss2_main.c|ss2_sound*.c|ss2_data.c) ;;
        *) SRCS+=("$f") ;;
    esac
done
cc -O1 -g -fsanitize=address,undefined -Wno-deprecated-declarations -DSS2_HOST -DROUTINE=$R -I"$ROOT/tools/ss2/hosttest/shim" -I"$ROOT/inc" \
   "$ROOT/tools/ss2/hosttest/test_routine.c" "${SRCS[@]}" "$ROOT/tools/ss2/hosttest/frame_tick.c" -o "$OUT/test_$R"
SS2_IRQ_MASK="$ROOT/out/ss2/irq_mask.bin" "$OUT/test_$R" "$1" "${3:-8}"

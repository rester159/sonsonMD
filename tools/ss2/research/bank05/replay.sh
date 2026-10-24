#!/bin/bash
# replay.sh PAIRS... -- build the whole-frame host replay once, then run it
# over each pairs file (first 3 differing frames reported per file).
set -e
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
R=ss2_frame_tick_then_vblank
OUT="$ROOT/out/ss2/hosttest"; mkdir -p "$OUT"
SRCS=()
for f in "$ROOT"/src/ss2_*.c; do
    case "$(basename "$f")" in
        ss2_main.c|ss2_sound*.c|ss2_data.c) ;;
        *) SRCS+=("$f") ;;
    esac
done
cc -O1 -g -fsanitize=address,undefined -DROUTINE=$R -I"$ROOT/tools/ss2/hosttest/shim" -I"$ROOT/inc" \
   "$ROOT/tools/ss2/hosttest/test_routine.c" "${SRCS[@]}" "$ROOT/tools/ss2/hosttest/frame_tick.c" -o "$OUT/replay_b5"
for p in "$@"; do
    echo "== $p"
    SS2_IRQ_MASK="$ROOT/out/ss2/irq_mask.bin" "$OUT/replay_b5" "$p" "${MAXREP:-3}" || true
done

#!/bin/bash
# replay.sh PAIRS... -- build replay.c against the SS2 logic (like
# tools/ss2/hosttest/run.sh, per-frame mode) and run it on each pair file.
set -e
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
OUT="$ROOT/out/ss2/hosttest"; mkdir -p "$OUT"
SRCS=()
for f in "$ROOT"/src/ss2_*.c; do
    case "$(basename "$f")" in
        ss2_main.c|ss2_sound*.c|ss2_data.c) ;;
        *) SRCS+=("$f") ;;
    esac
done
cc -O1 -g -fsanitize=address,undefined -Wno-deprecated-declarations -DSS2_HOST -I"$ROOT/tools/ss2/hosttest/shim" -I"$ROOT/inc" \
   "$ROOT/tools/ss2/research/bank09/replay.c" "${SRCS[@]}" "$ROOT/tools/ss2/hosttest/frame_tick.c" -o "$OUT/replay09"
for p in "$@"; do
    echo "== $(basename "$p")"
    SS2_IRQ_MASK="$ROOT/out/ss2/irq_mask.bin" "$OUT/replay09" "$p" "${MAXREP:-8}" || true
done

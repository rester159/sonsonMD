#!/bin/bash
# baseline.sh OBJ05.c OBJFIX.c PAIRS... -- replay with alternative versions of
# the two bank files (e.g. the pre-port stubs) to separate pre-existing
# differences from ones caused by this port.
set -e
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
A05="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"; AFX="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"; shift 2
SRCS=()
for f in "$ROOT"/src/ss2_*.c; do
    case "$(basename "$f")" in
        ss2_main.c|ss2_sound*.c|ss2_data.c|ss2_obj05.c|ss2_objfix.c) ;;
        *) SRCS+=("$f") ;;
    esac
done
OUT="$ROOT/out/ss2/hosttest"; mkdir -p "$OUT"
cc -O1 -DROUTINE=ss2_frame_tick_then_vblank -I"$ROOT/tools/ss2/hosttest/shim" -I"$ROOT/inc" \
   "$ROOT/tools/ss2/hosttest/test_routine.c" "${SRCS[@]}" "$A05" "$AFX" "$ROOT/tools/ss2/hosttest/frame_tick.c" -o "$OUT/replay_base"
for p in "$@"; do
    echo "== $p"
    SS2_IRQ_MASK="$ROOT/out/ss2/irq_mask.bin" "$OUT/replay_base" "$p" "${MAXREP:-3}" | tail -1 || true
done

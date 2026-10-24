#!/bin/bash
# replay.sh [-b] PAIRS... -- whole-frame replay of play.py captures with an
# unported-address histogram. -b rebuilds the binary first.
set -e
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
OUT="$ROOT/out/ss2/b17"; mkdir -p "$OUT"
BIN="$OUT/replay"
if [ "$1" = "-b" ] || [ ! -x "$BIN" ]; then
    [ "$1" = "-b" ] && shift
    SRCS=()
    for f in "$ROOT"/src/ss2_*.c; do
        case "$(basename "$f")" in
            ss2_main.c|ss2_sound*.c|ss2_data.c) ;;
            *) SRCS+=("$f") ;;
        esac
    done
    cc -O1 -g -fsanitize=address,undefined -I"$ROOT/tools/ss2/hosttest/shim" -I"$ROOT/inc" \
       "$ROOT/tools/ss2/research/bank17/replay.c" "${SRCS[@]}" "$ROOT/tools/ss2/hosttest/frame_tick.c" -o "$BIN"
fi
for p in "$@"; do
    echo "== $p"
    SS2_IRQ_MASK="$ROOT/out/ss2/irq_mask.bin" "$BIN" "$p" "${MAXREP:-6}" || true
done

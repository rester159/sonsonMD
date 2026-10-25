#!/bin/bash
# replay.sh [regress] -- replay the shot/magic captures (capture_magic.py,
# see runs.sh) through the native code; 'regress' also runs the standard
# frame/walk regressions.
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
RUN="$ROOT/tools/ss2/hosttest/run.sh"
S="$ROOT/out/ss2/shots"
MAX="${MAXREP:-8}"
if [ "$1" = regress ]; then
    echo "== pairs_frame tick";   bash "$RUN" "$ROOT/out/ss2/pairs_frame.bin" ss2_frame_tick_then_vblank "$MAX" | tail -1
    echo "== pairs_frame coro";   SS2_CONTINUOUS=1 bash "$RUN" "$ROOT/out/ss2/pairs_frame.bin" ss2_frame_coro "$MAX" | tail -1
    echo "== pairs_walk";         bash "$RUN" "$ROOT/out/ss2/pairs_walk.bin" ss2_player_update "$MAX" | tail -1
fi
for f in "$S"/*.bin; do
    b="$(basename "$f" .bin)"
    echo "== $b"
    case "$b" in
        vbl_*) SS2_CONTINUOUS=1 bash "$RUN" "$f" ss2_frame_vbl "$MAX" ;;
        *)     bash "$RUN" "$f" ss2_frame_tick_then_vblank "$MAX" ;;
    esac
done

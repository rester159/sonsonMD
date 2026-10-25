#!/bin/bash
# ab.sh BASE_ROOT [DATA] -- A/B equivalence of SS2 logic changes.
# Builds the host replay from BASE_ROOT's sources and from this checkout's,
# replays every recorded trace through both (SS2_DUMP) and requires the
# native RAM after each frame to be byte-identical. This checks a change
# against the previous native code, whatever either's agreement with the
# trace. DATA = the out/ss2 directory holding the captures (default ours).
set -u
CAND="$(cd "$(dirname "$0")/../../.." && pwd)"
BASE="$(cd "$1" && pwd)"
DATA="${2:-$CAND/out/ss2}"
OUT="$CAND/out/ss2/ab"; mkdir -p "$OUT"
build() {   # root routine -> binary
    local root=$1 r=$2 tag=$3 srcs=()
    for f in "$root"/src/ss2_*.c; do
        case "$(basename "$f")" in ss2_main.c|ss2_sound*.c|ss2_data.c) ;; *) srcs+=("$f") ;; esac
    done
    [ -f "$root/src/ss2_rom.c" ] || python3 "$root/tools/ss2/gen_rom.py" \
        || { echo "$root: no src/ss2_rom.c (needs arcade/ HuCard dumps)"; exit 2; }
    cc -O1 -w -DSS2_HOST -DROUTINE=$r -I"$root/tools/ss2/hosttest/shim" -I"$root/inc" \
       "$CAND/tools/ss2/hosttest/test_routine.c" "${srcs[@]}" "$root/tools/ss2/hosttest/frame_tick.c" -o "$OUT/$tag-$r" \
        || { echo "BUILD FAILED: $tag $r"; exit 2; }
}
fail=0; total=0
run() {     # routine pairs [env...]
    local r=$1 p=$2; shift 2
    [ -f "$p" ] || return
    [ -x "$OUT/base-$r" ] || build "$BASE" $r base
    [ -x "$OUT/cand-$r" ] || build "$CAND" $r cand
    rm -f "$OUT/a.bin" "$OUT/b.bin"
    env "$@" SS2_IRQ_MASK="${MASK:-$DATA/irq_mask.bin}" SS2_DUMP="$OUT/a.bin" "$OUT/base-$r" "$p" 0 > /dev/null 2>&1
    env "$@" SS2_IRQ_MASK="${MASK:-$DATA/irq_mask.bin}" SS2_DUMP="$OUT/b.bin" "$OUT/cand-$r" "$p" 0 > /dev/null 2>&1
    total=$((total + 1))
    if cmp -s "$OUT/a.bin" "$OUT/b.bin" && [ -s "$OUT/a.bin" ]; then
        printf "same  %6d frames  %s %s\n" $(( $(stat -f %z "$OUT/a.bin") / 8192 )) "$r" "${p#$DATA/}"
    else
        fail=$((fail + 1)); printf "DIFF  %s %s\n" "$r" "${p#$DATA/}"
    fi
}
rm -f "$OUT"/base-* "$OUT"/cand-*
for p in pairs_frame pairs_frame2 pairs_obj pairs_obj2; do
    run ss2_frame_tick_then_vblank "$DATA/$p.bin"
    run ss2_frame_coro "$DATA/$p.bin" SS2_CONTINUOUS=1
done
run ss2_player_update "$DATA/pairs_walk.bin"
for f in "$DATA"/shots/*.bin; do
    case "$(basename "$f")" in vbl_*) run ss2_frame_vbl "$f" SS2_CONTINUOUS=1 ;; *) run ss2_frame_tick_then_vblank "$f" ;; esac
done
for f in "$DATA"/npc/*.bin "$DATA"/door/*.bin; do
    MASK="$DATA/irq_union.bin" run ss2_frame_vbl "$f" SS2_CONTINUOUS=1 SS2_MASK_SOUND=1
done
for f in "$DATA"/ending/end*.bin; do
    MASK="$DATA/irq_union.bin" run ss2_frame_vbl_ending "$f" SS2_CONTINUOUS=1 SS2_MASK_SOUND=1
done
echo "$((total - fail))/$total traces identical"
[ $fail = 0 ]

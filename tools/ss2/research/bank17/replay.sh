#!/bin/bash
# replay.sh [-b] PAIRS... -- whole-frame replay of play.py captures with an
# unported-address histogram. -b rebuilds the binary first.
set -e
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
OUT="$ROOT/out/ss2/b17"; mkdir -p "$OUT"
BIN="$OUT/replay"
if [ "$1" = "-b" ] || [ ! -x "$BIN" ]; then
    [ "$1" = "-b" ] && shift
    # recommended shared change, applied to a copy: the $ED0D slot loop
    # continues from the X a handler leaves (ss2_obj_xout, ss2_obj17.c)
    python3.14 - "$ROOT/src/ss2_objects.c" "$OUT/ss2_objects_xout.c" <<'PY'
import sys
s = open(sys.argv[1]).read()
old = """    for (x = 0; x < 0x20; x++)
        if (O(0x245A, x)) ss2_obj_update(x);
}"""
new = """    for (x = 0; x < 0x20; x++)
        if (O(0x245A, x))
        {
            extern u8 ss2_obj_xout;
            ss2_obj_xout = 0xFF;
            ss2_obj_update(x);
            if (ss2_obj_xout != 0xFF) x = ss2_obj_xout;
        }
}"""
assert old in s
s = s.replace(old, new)
# and the bank 2 boss-death hit handler $6F6D (ss2_o17_boss_sparks)
old = """    if (addr < 0xA000) return ss2_item_handler(addr, x) || ss2_objfix_handler(addr, x);"""
new = """    if (addr == 0x6F6D) { extern void ss2_o17_boss_sparks(u8); ss2_o17_boss_sparks(x); return 1; }
    if (addr < 0xA000) return ss2_item_handler(addr, x) || ss2_objfix_handler(addr, x);"""
assert old in s
open(sys.argv[2], 'w').write(s.replace(old, new))
PY
    SRCS=("$OUT/ss2_objects_xout.c")
    for f in "$ROOT"/src/ss2_*.c; do
        case "$(basename "$f")" in
            ss2_main.c|ss2_sound*.c|ss2_data.c|ss2_objects.c) ;;
            *) SRCS+=("$f") ;;
        esac
    done
    cc -O1 -g -fsanitize=address,undefined -I"$ROOT/tools/ss2/hosttest/shim" -I"$ROOT/inc" \
       "$ROOT/tools/ss2/research/bank17/replay.c" "${SRCS[@]}" "$ROOT/tools/ss2/hosttest/frame_tick.c" -o "$BIN"
fi
for p in "$@"; do
    echo "== $p"
    SS2_KNOWN=${SS2_KNOWN-1} SS2_IRQ_MASK="$ROOT/out/ss2/irq_mask.bin" "$BIN" "$p" "${MAXREP:-6}" || true
done

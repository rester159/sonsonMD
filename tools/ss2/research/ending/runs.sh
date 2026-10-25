#!/bin/bash
# runs.sh [A|B|...]: the recorded ending captures (out/ss2/ending/end*.bin).
#  A: 353 zenny left (the money -> score tally), button I every 40 frames
#     up to frame 2000 for the dialogue (none later: while $6D = $88, frames
#     ~3865-4630, I/II restarts the game through the listener $E2A6)
#  B: no money, the secret-code flag $8A = $FF (the alternate sprites)
#  S: as A to frame 7950, pickling the distinct screens to out/ss2/end_*.pkl
# With PKL=dir, A and B also pickle every 200 pairs into dir/end<run>/.
# Replay: SS2_CONTINUOUS=1 SS2_IRQ_MASK=out/ss2/irq_union.bin SS2_MASK_SOUND=1
#   bash tools/ss2/hosttest/run.sh out/ss2/ending/endA.bin ss2_frame_vbl_ending
cd "$(dirname "$0")/../.."
OUT=../../out/ss2/ending
mkdir -p "$OUT"
SCR=$(python3.14 -c "print(' '.join('%d-%d:1' % (f, f) for f in range(60, 2000, 40)))")
run() {
    local name=$1; shift
    local sv=""
    if [ -n "$PKL" ]; then
        mkdir -p "$PKL/end$name"
        sv=$(python3.14 -c "import sys; print(','.join('%d:%s/p%05d.pkl' % (i, sys.argv[1], i) for i in range(200, 9001, 200)))" "$PKL/end$name")
    fi
    SAVE="$sv" python3.14 research/ending/cap.py "$OUT/end$name.bin" 9000 "$SCR" "$@" > "$OUT/end$name.out" 2>&1
}
case "$1" in
    A) run A 220B=00 220C=00 220D=00 220E=00 220F=00 2210=03 2211=05 2212=03 ;;
    B) run B 208A=FF ;;
    # the distinct screens for the Genesis tile converters (out/ss2/end_*.pkl)
    S) sf=""
       for it in 140:tally 915:msg 1480:pic_msg 1700:pic 3000:pic_scrolled 4300:pic_text \
                 4815:credits1 5336:credits2 5857:credits3 6378:credits4 6899:credits5 \
                 7420:credits6 7941:credits7; do
           sf="$sf${sf:+,}${it%%:*}:../../out/ss2/end_${it#*:}.pkl"
       done
       SAVEF="$sf" python3.14 research/ending/cap.py "$OUT/endS.bin" 7950 "$SCR" \
           220B=00 220C=00 220D=00 220E=00 220F=00 2210=03 2211=05 2212=03 > "$OUT/endS.out" 2>&1 ;;
esac

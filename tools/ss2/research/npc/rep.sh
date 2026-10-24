#!/bin/bash
# rep.sh MAXREPORT NAME... -- replay out/ss2/npc/NAME.bin natively and print
# the first differences (the binary from runs.sh / hosttest run.sh)
cd "$(dirname "$0")/../../../.."
n=$1; shift
for f in "$@"; do
    echo "== $f"
    SS2_IRQ_MASK=out/ss2/irq_union.bin SS2_CONTINUOUS=1 SS2_MASK_SOUND=1 \
        out/ss2/hosttest/test_ss2_frame_vbl out/ss2/npc/$f.bin $n
done

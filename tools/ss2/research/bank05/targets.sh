#!/bin/bash
# targets.sh -- targeted (teleport / poke) traces for the bank 5 and fixed
# bank 3 objects the explorer traces do not reach. Runs in parallel; output
# out/ss2/b5/tg_*.{pairs,cov,log}.
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
T="$ROOT/tools/ss2/research/bank05/trace.py"
O="$ROOT/out/ss2/b5"
N=${FRAMES:-1500}
run() { name=$1; shift; python3.14 "$T" "$@" > "$O/tg_$name.log" 2>&1 & }
run c02a      3 $N explore:0 "$O/tg_c02a"      --at=05:C02A:0
run bat0      0 $N explore:2 "$O/tg_bat0"      --at=05:C408:0
run hop1      4 $N explore:0 "$O/tg_hop1"      --at=05:C5CB:0
run fallplat  3 $N explore:1 "$O/tg_fallplat"  --at=05:8AC2:0
run block1    0 $N explore:0 "$O/tg_block1"    --at=05:8D40:0 --poke=76=03
run block4    3 $N explore:3 "$O/tg_block4"    --at=05:8D3A:0 --poke=76=03
run vessel5   4 $N explore:0 "$O/tg_vessel5"   --at=05:8D2E:0 --poke=76=03
run boss5a    4 $N explore:0 "$O/tg_boss5a"    --at=05:CF3E:0 --poke=7F=01
run boss6     5 $N explore:0 "$O/tg_boss6"     --at=05:D8A1:0 --poke=7F=01
run boss7a    6 $N explore:0 "$O/tg_boss7a"    --at=05:CF42:0 --poke=7F=01
run boss7b    6 $N explore:0 "$O/tg_boss7b"    --at=05:D8A5:0 --poke=7F=01
run boss7c    6 $N explore:0 "$O/tg_boss7c"    --at=05:CF4E:0 --poke=7F=01
wait

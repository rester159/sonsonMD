#!/bin/bash
# runs.sh [NAME...] -- the bank $17/$07/fixed-4 verification captures
# (play.py, solo mode: other code banks removed). Runs in parallel; output
# in out/ss2/b17/NAME.pairs (+ .log). Replay: replay.sh out/ss2/b17/*.pairs
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
OUT="$ROOT/out/ss2/b17"; mkdir -p "$OUT"
# name stage frames bot warp(screen x,y, x, y [,dx]) [extra opts]
RUNS=(
  "lob1     0 900 explore 1,4,78,70"
  "lob5     4 900 explore 7,3,B8,50"
  "lob6     5 900 explore 2,3,78,D0"
  "div1     0 900 explore 3,4,30,60"
  "div4     3 900 explore 6,2,60,E0"
  "pod1     0 900 explore 3,3,D0,90"
  "pod1b    0 900 explore 1,5,C0,D0"
  "pod4     3 900 explore 1,2,B0,B0"
  "pod6     5 900 explore 1,1,00,18"
  "crw2     1 900 explore 2,4,80,B0"
  "crw4     3 900 explore 7,3,80,40"
  "crw7     6 900 explore 1,4,80,C0"
  "scn1     0 900 explore 6,2,A0,20"
  "scn2     1 900 explore 5,7,90,A0"
  "scn3     2 900 explore 3,0,40,E0"
  "stk4     3 900 explore 2,5,60,C0"
  "stk6     5 900 explore 1,1,40,60"
  "boss2    1 1500 explore 6,2,F0,D8 boss"
  "boss3    2 1500 explore 0,0,E0,F0 boss"
  "boss7a   6 1500 explore 3,0,A0,38 boss"
  "boss7b   6 1500 explore 1,1,D0,50 boss"
)
want=" $* "
for r in "${RUNS[@]}"; do
    set -- $r
    name=$1; k=$2; n=$3; bot=$4; w=$5; shift 5
    [ "$want" != "  " ] && [[ "$want" != *" $name "* ]] && continue
    (cd "$ROOT/tools/ss2" && python3.14 research/bank17/play.py "$OUT/$name.pairs" $k $n $bot life magic solo warp=$w "$@" > "$OUT/$name.log" 2>&1) &
done
wait
grep -H . "$OUT"/*.log | grep -v "^.*s[0-9].log\|^.*e[0-9].log"

#!/bin/bash
# runs.sh -- the shot/magic captures (capture_magic.py) into out/ss2/shots,
# run in parallel; executed source addresses go to out/ss2/shots/cov.txt.
# Replay with replay.sh. Stage 1 from the snapshot, player walking right
# into the first enemies; '12' = II+I together (cast), N = RUN (pause).
cd "$(dirname "$0")/../.."
S=../../out/ss2/shots; mkdir -p "$S"; rm -f "$S"/cov.txt
PY="${PY:-python3.14}"
M=22BB=20,22BC=20,22BD=20,22BF=3F
cap() { COV="$S/cov.txt" INJ="$2" $PY research/shots/capture_magic.py "$S/$1.bin" "$3" "$4" > "$S/$1.log" 2>&1; }
vcap() { VBL=1 cap "$@"; }
# magic 1 fireballs, run dry (6 points, 7 casts): $79 delay, selection drop
cap m1  22BB=06,22BD=06,22BF=3F,22C0=1 400 "10-330:R 20-21:12 40-41:12 70-71:12 120-121:12 160-161:12 200-201:12 240-241:12" &
# magic 2 freeze ($71), magic 3 pillar, magic 5 sweep ($74)
cap m2  $M,22C0=2 300 "10-250:R 20-21:12 150-151:12" &
cap m3  $M,22C0=3 300 "10-250:R 20-21:12 120-121:12 220-221:12" &
cap m3b 22BB=40,22BC=40,22BD=40,22BF=3F,22C0=3 400 "10-60:R 30-31:12 100-101:12 130-131:12 170-171:12 230-231:12 300-301:12 180-350:R" &
cap m5  $M,22C0=5 300 "10-250:R 20-21:12 160-161:12" &
# cloud: summoned ($22C2), ride, staff and magic 1 from the cloud
cap cl  $M,22C0=1,22C2=08 400 "150-200:R 200-240:U 240-300:L 260-261:12 300-301:12 320-360:DR 340-341:12" &
# pause selection: Right x4 -> shield (magic 4), walk into enemies
vcap vbl_m4 $M,22C0=0 400 "20-23:N 40-41:R 50-51:R 60-61:R 70-71:R 90-93:N 110-380:R" &
# shield on low magic: hits drain it, the shield ends ($D6EB -> $D702)
vcap vbl_s1 22BB=03,22BC=20,22BD=03,22BF=3F,22C0=0 400 "20-23:N 40-41:R 50-51:R 60-61:R 70-71:R 90-93:N 110-380:R" &
# shield on, pause again, Left -> magic 3: $FEAE dismisses it ($D702, $D03D)
vcap vbl_s2 $M,22C0=0 300 "20-23:N 40-41:R 50-51:R 60-61:R 70-71:R 90-93:N 150-153:N 170-171:L 190-193:N 200-280:R" &
# $C4D7: thrown by a trap ($86 latch injected with hurt $5A, hitter slot 0)
cap c4d7 2086=01,2297=20,22C8=5A,22C7=01 120 "" &
# $C4B8: dying with $22C1 bit 1: revive with half life
cap c4b8 2297=40,22C1=02,22C7=01 200 "" &
wait
cat "$S"/*.log

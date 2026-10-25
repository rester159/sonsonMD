#!/bin/bash
# runs.sh [NAME...] -- townsperson and shop captures (research/npc/cap.py)
# into out/ss2/npc/NAME.bin (log NAME.log) and their native replays
# (NAME.rep, SS2_CONTINUOUS=1 ss2_frame_vbl with out/ss2/irq_union.bin and
# SS2_MASK_SOUND=1: the host build has no sound driver).
#
# Each line: name, stage index, record world x y, approach DX and key, the
# script frame of the touch (T), events, frames, RAM pokes. The approach key
# is held from frame 10 to T+9. Events: '-' = button I every 14 frames from
# T+26 (pages; a choice takes the highlighted answer); otherwise a comma
# list of OFFSET:KEYS (frames after T) -- prefixed with '+' to add to the
# button-I train, else alone (shops).
cd "$(dirname "$0")/../../../.."
mkdir -p out/ss2/npc
script() {
    python3 - "$@" <<'EOF'
import sys
hit, key, ev, frames = int(sys.argv[1]), sys.argv[2], sys.argv[3], int(sys.argv[4])
s = ['10-%d:%s' % (hit + 9, key)]
if ev == '-' or ev.startswith('+'):
    s += ['%d-%d:1' % (f, f) for f in range(hit + 26, frames, 14)]
    ev = ev[1:]
for e in ev.split(','):
    if ':' in e:
        o, k = e.split(':')
        s.append('%d-%d:%s' % (hit + int(o), hit + int(o), k))
print(' '.join(s))
EOF
}
cap() {
    name=$1 k=$2 x=$3 y=$4 d=$5 key=$6 hit=$7 ev=$8 frames=$9; shift 9
    s=$(script $hit $key "$ev" $frames)
    python3.14 tools/ss2/research/npc/cap.py out/ss2/npc/$name.bin $k $frames "$s" $x $y $d "$@" > out/ss2/npc/$name.log 2>&1
    SS2_IRQ_MASK=out/ss2/irq_union.bin SS2_CONTINUOUS=1 SS2_MASK_SOUND=1 out/ss2/hosttest/test_ss2_frame_vbl out/ss2/npc/$name.bin ${MAXREP:-8} > out/ss2/npc/$name.rep 2>&1
    echo "$name: $(grep pairs out/ss2/npc/$name.log | cut -c1-240)"
    echo "    $(grep final out/ss2/npc/$name.log | cut -c7-140)"
    echo "    replay: $(tail -1 out/ss2/npc/$name.rep)"
}
export -f cap script
# the replay binary (run.sh compiles it; the empty run replays nothing)
SS2_CONTINUOUS=1 bash tools/ss2/hosttest/run.sh /dev/null ss2_frame_vbl > /dev/null 2>&1
LIST=$(cat <<'EOF'
s1t0 0 1696 544 -100 L 54 - 300
s1t1 0 272 512 60 R 13 - 300
s1shop48 0 848 1616 60 R 13 200:1,250:1,300:R,350:1,400:1,450:D,500:1,550:L,600:1,650:R,700:R,750:1 900 220F=09 2210=09
s1shop49 0 1744 1184 -60 L 13 200:1,250:1,300:R,350:1,400:D,450:1,500:R,550:1 750 2210=03 2211=05
s2shop48 1 1248 1952 -60 L 31 200:1,250:R,300:1,350:D,400:1,450:L,500:1,550:R,600:R,650:1 800 220F=09
s2shop49 1 512 576 -60 L 75 200:1,250:R,300:1,350:D,400:1,450:L,500:1,550:R,600:R,650:1 850 220F=09
s5shop48 4 0 1472 -60 L 22 200:1,250:R,300:1,350:D,400:1,450:L,500:1,550:R,600:R,650:1 800 220E=02
s2t1gift 1 1424 1952 -60 L 13 - 300
s2t1buy 1 1424 1952 -60 L 13 - 300 207A=01 220F=03
s2t1no 1 1424 1952 -60 L 13 +122:R 300 207A=01 220F=03
s2t1poor 1 1424 1952 -60 L 13 - 300 207A=01
s2t2 1 720 1568 60 R 13 - 300
s3t1 2 1184 160 -100 L 71 - 360 220F=03
s3t1poor 2 1184 160 -100 L 71 - 360
s3t1b 2 1184 160 -100 L 71 - 360 2072=01
s3t1bno 2 1184 160 -100 L 71 +52:R 360 2072=01
s3t2buy 2 832 224 -60 L 22 - 300 2210=05
s3t2again 2 832 224 -60 L 22 - 300 2210=09 207B=01
s3t2poor 2 832 224 -60 L 22 - 300
s3t2no 2 832 224 -60 L 22 +94:R 300 2210=05
s4t1 3 1360 464 60 R 13 - 300
s4t1key 3 1360 464 60 R 13 - 300 22C1=40
s4t1key2 3 1360 464 60 R 13 - 300 22C1=40 207A=01
s4t1b 3 1360 464 60 R 13 - 300 207A=01
s4t2 3 1824 1072 -100 L 22 - 300
s5t1pay 4 224 160 100 R 54 - 330 220F=09
s5t1lose 4 224 160 100 R 54 - 330 2210=03
s5t1no 4 224 160 100 R 54 +94:R 330 2210=03
s5t2 4 672 320 60 R 32 - 300
s5t2shop 4 672 320 60 R 32 200:1,250:1,300:R,350:1,400:D,450:1,500:L,550:1,600:R,650:R,700:1 850 207B=01 220E=09 220F=09 2210=09
s6t1 5 672 352 -60 L 31 - 300 2210=03
s6t1no 5 672 352 -60 L 31 +66:R 300 2210=03
s6t2 5 1568 320 -60 L 22 - 330
s6t2b 5 1568 320 -60 L 22 +94:D 330
s6t2c 5 1568 320 -60 L 22 +85:D,88:D 330
s7t1shop 6 1376 1728 60 R 22 200:1,250:R,300:1,350:D,400:1,450:1,500:L,550:1,600:R,650:R,700:1 850 220F=09 2210=09
s7t2 6 1680 128 -60 L 13 - 300
s7t2key 6 1680 128 -60 L 13 - 300 22C1=20
EOF
)
if [ $# -gt 0 ]; then LIST=$(echo "$LIST" | grep -E "^($(echo "$@" | tr ' ' '|')) "); fi
echo "$LIST" | xargs -P ${JOBS:-6} -L 1 bash -c 'cap "$@"' _

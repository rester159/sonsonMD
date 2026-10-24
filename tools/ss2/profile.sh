#!/bin/bash
# profile.sh -- build the SS2_PROF ROM, play stage 1 headlessly for ~1200
# frames (walk right, attack every 12 frames, jump every 90) and save the
# on-HUD load readout to out/ss2/prof/hud.png. Leaves out/rom.bin untouched.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
python3.14 tools/build_native.py -D SS2_PROF --out out/prof | tail -1
cp out/rom.bin out/prof/rom_keep.bin
cp out/prof/rom.bin out/rom.bin
S="850-853:D;900-903:S;1000-2200:R;$(python3.14 -c "print(';'.join('%d-%d:b'%(f,f+2) for f in range(1020,2200,12))+';'+';'.join('%d-%d:c'%(f,f+8) for f in range(1050,2200,90)))")"
python3.14 tools/ss2/shot.py 2200 "$S" 2199 out/ss2/prof >/dev/null || true
cp out/prof/rom_keep.bin out/rom.bin
python3.14 -c "
from PIL import Image
Image.open('out/ss2/prof/frame-2199.png').crop((0,0,256,48)).resize((768,144),Image.NEAREST).save('out/ss2/prof/hud.png')"
echo out/ss2/prof/hud.png

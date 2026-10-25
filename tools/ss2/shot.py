#!/usr/bin/env python3
"""shot.py FRAMES SCRIPT SHOTS [OUTDIR] -- capture out/rom.bin headlessly.

Builds tools/ss2/gpgx_shot.c on demand, runs the Genesis Plus GX libretro
core without a frontend and converts the PPM captures to PNG (plus a contact
sheet). Example: shot.py 1500 "900-905:S;1200-1206:D;1250-1256:S" 1100,1400
"""
import os, subprocess, sys
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CORE = Path(os.environ.get('GENPLUS_CORE', str(Path.home() / 'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')))


def main():
    frames, script, shots = sys.argv[1], sys.argv[2], sys.argv[3]
    out = Path(sys.argv[4]) if len(sys.argv) > 4 else ROOT / 'out' / 'ss2' / 'shots'
    out.mkdir(parents=True, exist_ok=True)
    exe = ROOT / 'out' / 'ss2' / 'gpgx_shot'
    src = HERE / 'gpgx_shot.c'
    if not exe.exists() or exe.stat().st_mtime < src.stat().st_mtime:
        subprocess.run(['cc', '-O2', str(src), '-o', str(exe)], check=True)
    rom = os.environ.get('SS2_ROM', str(ROOT / 'out' / 'rom.bin'))
    subprocess.run([str(exe), str(CORE), rom, frames, str(out), script, shots], check=True, timeout=600)
    ims = []
    for f in shots.split(','):
        p = out / ('frame-%s.ppm' % f)
        im = Image.open(p)
        im.save(out / ('frame-%s.png' % f))
        p.unlink()
        ims.append(im)
    w, h = ims[0].size
    cols = min(4, len(ims))
    sheet = Image.new('RGB', (w * cols, h * ((len(ims) + cols - 1) // cols)))
    for i, im in enumerate(ims):
        sheet.paste(im, ((i % cols) * w, (i // cols) * h))
    sheet.save(out / 'sheet.png')
    print(out / 'sheet.png')


if __name__ == '__main__':
    main()

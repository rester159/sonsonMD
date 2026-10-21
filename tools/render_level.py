#!/usr/bin/env python3
"""render_level.py -- render a captured wide level (levelN_capture.json) to a PNG
for eyeball verification (is it CLEAN -- no DEPOSIT COIN / banner text?).

Reuses gen_assets.bake_char_tile + the verbatim arcade char palette to paint each
(code,bank) cell. Output: a (cols*8) x (rows*8) PNG. Optionally blank the top HUD
rows (--blank-top N) exactly as gen_level does, to preview the shipped look.

USAGE: python render_level.py tools/levelN_capture.json out.png [--blank-top 5] [--cols-max 64]
"""
import os, sys, json, struct, zlib
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_assets as G

def char_palette_rgb():
    # PAL0 (16 char colors) = the committed SS_PAL_CHAR VDP words (0x0BGR, each
    # channel a nibble 0..14 in steps of 2). Convert to RGB888 (scale nibble*16).
    out = []
    for w in G.SS_PAL_CHAR:
        r = (w & 0x00F); g = (w & 0x0F0) >> 4; b = (w & 0xF00) >> 8
        out.append((r*16, g*16, b*16))
    return out

def write_png(path, w, h, rgb):
    def chunk(typ, data):
        c = typ + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            r,g,b = rgb[y*w+x]; raw += bytes((r,g,b))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)

def main(argv):
    cap_path = argv[0]
    out_png = argv[1]
    blank_top = 0
    cols_max = None
    i = 2
    while i < len(argv):
        if argv[i] == "--blank-top": blank_top = int(argv[i+1]); i += 2
        elif argv[i] == "--cols-max": cols_max = int(argv[i+1]); i += 2
        else: i += 1
    roms = G.Roms(os.path.join(G.REPO, "arcade"))
    pal = char_palette_rgb()
    cap = json.load(open(cap_path))
    cols = cap["level_cols"]; rows = cap["rows"]
    if cols_max: cols = min(cols, cols_max)
    vram = cap["videoram"]; cram = cap["colorram"]
    W = cols*8; H = rows*8
    rgb = [(0,0,0)] * (W*H)
    fullcols = cap["level_cols"]
    for row in range(rows):
        for col in range(cols):
            if row < blank_top:
                continue
            idx = row*fullcols + col
            v = vram[idx]; a = cram[idx]
            code = v + 256*(a & 3); bank = a >> 2
            tile = G.bake_char_tile(roms, code, bank)  # 8 u32, 4bpp rows
            for py in range(8):
                wpix = tile[py]
                for px in range(8):
                    pen = (wpix >> ((7-px)*4)) & 0xF
                    color = pal[pen] if pen != 0 else (0,0,0)
                    X = col*8+px; Y = row*8+py
                    rgb[Y*W+X] = color
    write_png(out_png, W, H, rgb)
    print("wrote %s (%dx%d, %d cols)" % (out_png, W, H, cols))

if __name__ == "__main__":
    main(sys.argv[1:])

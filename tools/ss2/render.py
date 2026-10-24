"""Render a PCE VDC/VCE state (from pce.PCE) to a PIL image, plus decoders.

Tile format: 16 words; words 0-7 rows of planes 0/1 (lo/hi byte), words 8-15
rows of planes 2/3. Sprite cell: 64 words, plane p row r = word p*16+r, bit 15
leftmost. Colour word: bits 0-2 blue, 3-5 red, 6-8 green.
"""
from PIL import Image


def rgb(c):
    b, r, g = c & 7, (c >> 3) & 7, (c >> 6) & 7
    return (r * 255 // 7, g * 255 // 7, b * 255 // 7)


def tile_pixels(vram, t):
    base = (t * 16) & 0x7FFF
    out = []
    for row in range(8):
        w0 = vram[(base + row) & 0x7FFF]
        w1 = vram[(base + 8 + row) & 0x7FFF]
        r = []
        for x in range(8):
            b = 7 - x
            r.append(((w0 >> b) & 1) | (((w0 >> (8 + b)) & 1) << 1) |
                     (((w1 >> b) & 1) << 2) | (((w1 >> (8 + b)) & 1) << 3))
        out.append(r)
    return out


def cell_pixels(vram, cell):
    base = (cell * 64) & 0x7FFF
    out = []
    for row in range(16):
        ws = [vram[(base + p * 16 + row) & 0x7FFF] for p in range(4)]
        out.append([sum(((ws[p] >> (15 - x)) & 1) << p for p in range(4)) for x in range(16)])
    return out


def bg_size(vreg):
    mwr = vreg[9]
    w = (32, 64, 128, 128)[(mwr >> 4) & 3]
    h = 64 if mwr & 0x40 else 32
    return w, h


def render(m, width=256, sprites=True, bg=True):
    vram, cram, vreg = m.vram, m.cram, m.vreg
    lines = m.line_regs or [(vreg[7], vreg[8] + i, vreg[5]) for i in range(224)]
    h = len(lines)
    img = Image.new('RGB', (width, h), rgb(cram[0]))
    px = img.load()
    bw, bh = bg_size(vreg)
    tcache = {}
    prio = [[0] * width for _ in range(h)]     # 1 where BG pixel opaque
    for y, (bxr, byr, cr) in enumerate(lines):
        if not (cr & 0x80) or not bg:
            continue
        vy = byr & (bh * 8 - 1)
        row = vy >> 3
        for x in range(width):
            vx = (x + bxr) & (bw * 8 - 1)
            e = vram[(row * bw + (vx >> 3)) & 0x7FFF]
            t = e & 0xFFF
            if t not in tcache:
                tcache[t] = tile_pixels(vram, t)
            c = tcache[t][vy & 7][vx & 7]
            if c:
                px[x, y] = rgb(cram[(e >> 12) * 16 + c])
                prio[y][x] = 1
    if sprites:
        sat = m.satb
        for i in range(63, -1, -1):
            sy, sx, pat, at = sat[i * 4:i * 4 + 4]
            sy = (sy & 0x3FF) - 64
            sx = (sx & 0x3FF) - 32
            cw = 2 if at & 0x100 else 1
            ch = (1, 2, 4, 4)[(at >> 12) & 3]
            cell0 = (pat >> 1) & 0x3FF
            if cw == 2: cell0 &= ~1
            if ch == 2: cell0 &= ~2
            if ch == 4: cell0 &= ~6
            pal = 256 + (at & 15) * 16
            front = at & 0x80
            fx, fy = at & 0x800, at & 0x8000
            for cy in range(ch):
                for cx in range(cw):
                    ccx = (cw - 1 - cx) if fx else cx
                    ccy = (ch - 1 - cy) if fy else cy
                    pix = cell_pixels(vram, cell0 + ccy * 2 + ccx)
                    for r in range(16):
                        yy = sy + cy * 16 + r
                        if not 0 <= yy < h: continue
                        rr = 15 - r if fy else r
                        for c in range(16):
                            xx = sx + cx * 16 + c
                            if not 0 <= xx < width: continue
                            v = pix[rr][15 - c if fx else c]
                            if v and (front or not prio[yy][xx]):
                                px[xx, yy] = rgb(cram[pal + v])
    return img


def palette_strip(cram, scale=8):
    img = Image.new('RGB', (16 * scale, 32 * scale))
    p = img.load()
    for i in range(512):
        c = rgb(cram[i])
        for dy in range(scale):
            for dx in range(scale):
                p[(i % 16) * scale + dx, (i // 16) * scale + dy] = c
    return img

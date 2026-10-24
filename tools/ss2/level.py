"""Son Son II stage map decoder (source: HuCard bank 0 $E92C/$EA4A streamers).

Stage s map: 4 KiB at bank (0x0C | s>>1), offset ((s&1)*0x1000); an 8x8 grid
of screens, each 8x8 metatile bytes (screen = 64 bytes, row-major, screen
index = sy*8+sx). World = 2048x2048 px, wrapping.
Metatile m: 8 bytes at bank (6 | s>>2) $C000 + m*8 (base from zp $5D/$5E):
four 16x16 blocks [TL, TR, BL, BR] of (b0, b1).
Block 8x8 tile (xo, yo): tile = 0x180 + 0xC0 + ((b0+6)&0xF8)*4 + ((b0+6)&7)*2
+ xo + 16*yo; palette = b1 >> 4; b1 & 0x0F kept as the block attribute.
"""

def rom_bank(rom, b):
    return rom[(b % (len(rom) // 0x2000)) * 0x2000:][:0x2000]


def stage_map(rom, s):
    bank = rom_bank(rom, 0x0C | (s >> 1))
    return bank[(s & 1) * 0x1000:][:0x1000]


def metatiles(rom, s, base=0xC000):
    return rom_bank(rom, 6 | (s >> 2))[base - 0xC000:]


def block_words(b0, b1):
    v = (b0 + 6) & 0xFF
    t = 0x180 + 0xC0 + ((v & 0xF8) << 2) + ((v & 7) << 1)
    pal = (b1 & 0xF0) << 8
    return [[pal | (t + xo + 16 * yo) for xo in (0, 1)] for yo in (0, 1)]


def world_cell(rom, s, mx, my):
    """metatile byte at world metatile coords (64x64 grid)."""
    m = stage_map(rom, s)
    sx, sy = (mx >> 3) & 7, (my >> 3) & 7
    return m[(sy * 8 + sx) * 64 + (my & 7) * 8 + (mx & 7)]


def world_tilemap(rom, s):
    """256x256 grid of BAT words plus 128x128 grid of block attributes."""
    mt = metatiles(rom, s)
    words = [[0] * 256 for _ in range(256)]
    attr = [[0] * 128 for _ in range(128)]
    for my in range(64):
        for mx in range(64):
            m = world_cell(rom, s, mx, my)
            e = mt[m * 8:m * 8 + 8]
            for bi in range(4):
                bx, by = mx * 2 + (bi & 1), my * 2 + (bi >> 1)
                b0, b1 = e[bi * 2], e[bi * 2 + 1]
                attr[by][bx] = b1 & 0x0F
                w = block_words(b0, b1)
                for yo in (0, 1):
                    for xo in (0, 1):
                        words[by * 2 + yo][bx * 2 + xo] = w[yo][xo]
    return words, attr


def render_world(words, vram, cram):
    from PIL import Image
    import render
    img = Image.new('RGB', (2048, 2048))
    px = img.load()
    cache = {}
    for ty in range(256):
        for tx in range(256):
            e = words[ty][tx]
            t = e & 0xFFF
            if t not in cache:
                cache[t] = render.tile_pixels(vram, t)
            pal = (e >> 12) * 16
            for y in range(8):
                row = cache[t][y]
                for x in range(8):
                    c = row[x]
                    px[tx * 8 + x, ty * 8 + y] = render.rgb(cram[pal + c] if c else cram[0])
    return img


def bank_read(rom, bank, addr):
    """byte at logical addr with `bank` mapped at addr's MPR window."""
    return rom[(bank % (len(rom) // 0x2000)) * 0x2000 + (addr & 0x1FFF)]


def stage_block_list(rom, s):
    """loader entries (bank, nblocks, addr) from bank 1 $548F (code 02:7882)."""
    b1 = lambda a: bank_read(rom, 1, a)
    a = b1(0x548F + 2 * s) | (b1(0x5490 + 2 * s) << 8)
    out = []
    while b1(a) != 0xFF:
        out.append((b1(a), b1(a + 1), b1(a + 2) | (b1(a + 3) << 8)))
        a += 4
    return out


def decode_block(raw):
    """0x60 bytes -> 4 tiles (TL, TR, BL, BR) as 8x8 index rows (3bpp)."""
    tiles = []
    for t in range(4):
        d = raw[t * 24:t * 24 + 24]
        rows = []
        for r in range(8):
            p0, p1, p2 = d[r * 2], d[r * 2 + 1], d[16 + r]
            rows.append([((p0 >> (7 - x)) & 1) | (((p1 >> (7 - x)) & 1) << 1) |
                         (((p2 >> (7 - x)) & 1) << 2) for x in range(8)])
        tiles.append(rows)
    return tiles


def stage_tiles(rom, s):
    """dict PCE tile number -> 8x8 pixel rows, as loaded at VRAM $1800."""
    out = {}
    k = 0
    for bank, n, addr in stage_block_list(rom, s):
        for i in range(n or 256):
            a = addr + i * 0x60
            raw = bytes(bank_read(rom, bank + ((a - 0xC000) >> 13), 0xC000 + ((a - 0xC000) & 0x1FFF)) for a in range(a, a + 0x60))
            t0 = 0x180 + (k & 7) * 2 + (k >> 3) * 32
            for j, px in enumerate(decode_block(raw)):
                out[t0 + (j & 1) + 16 * (j >> 1)] = px
            k += 1
    return out

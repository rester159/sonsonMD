#!/usr/bin/env python3
"""Son Son II (PC Engine HuCard) -> Genesis resource converter.

Reads the owner's HuCard image (arcade/Son_Son_II_J_TEng.pce) and writes
src/ss2_data.c + inc/ss2_data.h. Source formats are documented in level.py;
the HUD is taken from the deterministic stage-1 research snapshot.

Palette ownership (Genesis CRAM lines):
  0, 1  stage background (PCE BG palettes 5-15 grouped, closest-colour merge)
  2, 3  sprites + HUD (PCE sprite palettes and HUD BG palettes 0-4)
PCE and Genesis share 3-bit RGB channels, so kept colours are exact.
"""
import os, sys, hashlib
sys.setrecursionlimit(10000)
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pickle
import level, palfit, snapshot
import render

ROOT = snapshot.ROOT
ROM = open(snapshot.ROM, 'rb').read()
PLAYER_BANK = 0x10          # bank 2 $79A2: TAM #$40 with A=$10
PLAYER_FRAMES = 21          # 0x180-byte 32x32 3bpp frames in the bank


def gen_colour(c):
    """PCE GRB333 (B bits 0-2, R 3-5, G 6-8) -> Genesis 0BBB0GGG0RRR0."""
    b, r, g = c & 7, (c >> 3) & 7, (c >> 6) & 7
    return (b << 9) | (g << 5) | (r << 1)


def bank_bytes(bank, off, n):
    return bytes(ROM[(bank % 32) * 0x2000 + off + i] for i in range(n))


def pce_words(off, n):
    return [ROM[off + 2 * i] | (ROM[off + 2 * i + 1] << 8) for i in range(n)]


def stage_bg_palettes(s):
    """PCE BG palettes: 0-4 common (bank1 $4000), 5-15 per stage ($40D0 table)."""
    pals = {p: pce_words(0x2000 + p * 16, 16) for p in range(5)}
    ptr = ROM[0x20D0 + 2 * s] | (ROM[0x20D1 + 2 * s] << 8)
    base = 0x2000 + (ptr - 0x4000)
    for p in range(5, 16):
        pals[p] = pce_words(base + (p - 5) * 16, 16)
    return pals


def stage_sp_palettes(s):
    """PCE sprite palettes: 0-7 common (bank1 $4050), 8-15 per stage ($45AE)."""
    pals = {p: pce_words(0x2050 + p * 16, 16) for p in range(8)}
    ptr = ROM[0x25AE + 2 * s] | (ROM[0x25AF + 2 * s] << 8)
    base = 0x2000 + (ptr - 0x4000)
    for p in range(8, 16):
        pals[p] = pce_words(base + (p - 8) * 16, 16)
    return pals


class LinePalette:
    """One Genesis CRAM line; index 0 transparent."""
    def __init__(self, colours):
        self.colours = [0] + sorted(colours)
        assert len(self.colours) <= 16, len(self.colours)
        self.index = {c: i for i, c in enumerate(self.colours)}

    def words(self):
        return [gen_colour(c) for c in self.colours] + [0] * (16 - len(self.colours))


class TileBank:
    """Deduplicated Genesis tiles with flip sharing."""
    def __init__(self):
        self.tiles = []
        self.lookup = {}

    def add(self, px):
        key = tuple(tuple(r) for r in px)
        if key in self.lookup:
            return self.lookup[key]
        idx = len(self.tiles)
        self.tiles.append(key)
        h = tuple(tuple(reversed(r)) for r in key)
        v = tuple(reversed(key))
        hv = tuple(reversed(h))
        for k, flags in ((key, 0), (h, 0x0800), (v, 0x1000), (hv, 0x1800)):
            self.lookup.setdefault(k, (idx, flags))
        return idx, 0

    def words(self):
        out = []
        for t in self.tiles:
            for r in t:
                v = 0
                for c in r:
                    v = (v << 4) | c
                out.append(v)
        return out


def remap(px, colour_of, line):
    return [[line.index[colour_of(c)] if c else 0 for c in r] for r in px]


def fit_lines(weights, nlines):
    best = palfit.best_partition(weights, nlines)
    _, assign, maps = best
    lines = [LinePalette(set(mp.values())) for mp in maps]
    return assign, maps, lines


def build_stage(s):
    words, attr = level.world_tilemap(ROM, s)
    tiles = level.stage_tiles(ROM, s)
    pals = stage_bg_palettes(s)
    weights = {}
    for row in words:
        for e in row:
            p = e >> 12
            for r in tiles[e & 0xFFF]:
                for c in r:
                    if c:
                        w = weights.setdefault(p, {})
                        col = pals[p][c]
                        w[col] = w.get(col, 0) + 1
    assign, maps, lines = fit_lines(weights, 2)
    bank = TileBank()
    bank.add([[0] * 8] * 8)                     # tile 0: blank (index 0)
    cache = {}

    def gen_word(e):
        if e in cache:
            return cache[e]
        p, t = e >> 12, e & 0xFFF
        li = assign.get(p, 0)
        px = remap(tiles[t], lambda c: maps[li][pals[p][c]], lines[li])
        idx, flags = bank.add(px)
        w = (li << 13) | flags | idx
        cache[e] = w
        return w

    mt = level.metatiles(ROM, s)
    nmeta = max(level.stage_map(ROM, s)) + 1
    meta_words, meta_attr = [], []
    for m in range(nmeta):
        e = mt[m * 8:m * 8 + 8]
        grid = [[0] * 4 for _ in range(4)]
        at = 0
        for bi in range(4):
            b0, b1 = e[bi * 2], e[bi * 2 + 1]
            at |= (b1 & 0x0F) << (4 * bi)
            bw = level.block_words(b0, b1)
            for yo in (0, 1):
                for xo in (0, 1):
                    grid[(bi >> 1) * 2 + yo][(bi & 1) * 2 + xo] = gen_word(bw[yo][xo])
        meta_words += [w for r in grid for w in r]
        meta_attr.append(at)
    return dict(lines=lines, bank=bank, meta=meta_words, attr=meta_attr,
                map=level.stage_map(ROM, s), nmeta=nmeta, merges=maps)


def player_frame_pixels(n):
    raw = bank_bytes(PLAYER_BANK, n * 0x180, 0x180)
    img = [[0] * 32 for _ in range(32)]
    for c in range(4):
        cell = raw[c * 0x60:c * 0x60 + 0x60]
        for r in range(16):
            ws = [cell[p * 32 + r * 2] | (cell[p * 32 + r * 2 + 1] << 8) for p in range(3)]
            for x in range(16):
                img[(c >> 1) * 16 + r][(c & 1) * 16 + x] = sum(((ws[p] >> (15 - x)) & 1) << p for p in range(3))
    return img


def build_sprites_and_hud(m, s, hud_m, use):
    sp = stage_sp_palettes(s)
    bg = stage_bg_palettes(s)
    # HUD: PCE BAT rows 1-6 (screen lines 8-55, BYR 0) of the stage-1 snapshot
    # (identical on every stage; its BG palettes 0-4 are common)
    hud_cells = [[hud_m.vram[r * 64 + c] for c in range(32)] for r in range(1, 7)]
    weights = {}
    def addw(key, col, n=1):
        w = weights.setdefault(key, {}); w[col] = w.get(col, 0) + n
    for row in hud_cells:
        for e in row:
            for r in render.tile_pixels(hud_m.vram, e & 0xFFF):
                for c in r:
                    addw(('hud', e >> 12), bg[e >> 12][c] if c else bg[0][0], 50)
    for n in range(PLAYER_FRAMES):
        for r in player_frame_pixels(n):
            for c in r:
                if c: addw(('sp', 0), sp[0][c], 4)
    # stage sprite cells observed in research play (tools/ss2/census.py)
    for (cell, p) in {(c ^ k, p) for (c, p) in use for k in (0, 1)}:
        for r in render.cell_pixels(m.vram, cell):
            for c in r:
                if c: addw(('sp', p), m.cram[256 + p * 16 + c], 1)
    assign, maps, lines = fit_lines(weights, 2)
    bank = TileBank()
    hud_map = []
    for row in hud_cells:
        for e in row:
            key = ('hud', e >> 12); li = assign[key]
            px = render.tile_pixels(hud_m.vram, e & 0xFFF)
            # HUD index 0 is opaque backdrop black on the PCE: keep it opaque
            pxm = [[lines[li].index[maps[li][bg[e >> 12][c] if c else bg[0][0]]] for c in r] for r in px]
            idx, flags = bank.add(pxm)
            hud_map.append(((2 + li) << 13) | 0x8000 | flags | idx)
    # HUD tiles drawn at run time (bank 4 $BB99/$BBC2 digits, bank 2 $76B3
    # bars): PCE BAT words -> Genesis window words
    hud_live = {}
    for w in [0x0100 | (0xB0 + d) for d in range(10)] + \
             [0x2100 | ((0x80 + t) & 0xFF) | (0x0100 if t + 0x80 > 0xFF else 0) for t in (0x0C, 0x0D, 0x0E, 0x0F, 0x3A, 0x3B, 0x3C)]:
        key = ('hud', w >> 12); li = assign[key]
        px = render.tile_pixels(hud_m.vram, w & 0xFFF)
        pxm = [[lines[li].index[maps[li][bg[w >> 12][c] if c else bg[0][0]]] for c in r] for r in px]
        idx, flags = bank.add(pxm)
        hud_live[w] = ((2 + li) << 13) | 0x8000 | flags | idx
    # colour maps for run-time sprite conversion: every PCE sprite palette
    # (exact where the fit has it, else nearest colours on the best line)
    # and the interior palette-13 variants a door loads ($493C + v*16)
    def cdist(a, b):
        return sum((((a >> sh) & 7) - ((b >> sh) & 7)) ** 2 for sh in (0, 3, 6))
    def near(li, col):
        return min(range(1, len(lines[li].colours)), key=lambda i: cdist(lines[li].colours[i], col))
    def cmap_for(cols, key):
        if key in assign:
            li = assign[key]
            idx = [lines[li].index[maps[li][cols[c]]] if cols[c] in maps[li] else near(li, cols[c])
                   for c in range(1, 8)]
        else:
            li = min(range(len(lines)), key=lambda l: sum(cdist(lines[l].colours[near(l, cols[c])], cols[c])
                                                           for c in range(1, 8)))
            idx = [near(li, cols[c]) for c in range(1, 8)]
        return [0] + [(li << 7) | i for i in idx]
    spr_cmap = []
    for p in range(16):
        spr_cmap += cmap_for(sp[p], ('sp', p))
    spr_cmap13 = []
    for v in range(8):
        spr_cmap13 += cmap_for(pce_words(0x293C + v * 16, 16), None)
    bat_words, bat_line = [], []
    for w in BAT_KEYS:
        key = ('hud', w >> 12)
        pal = bg[w >> 12]                       # the tile's own colours, always
        px = render.tile_pixels(hud_m.vram, w & 0xFFF)
        cols = {pal[c] if c else bg[0][0] for r in px for c in r}
        if key in assign:
            li = assign[key]
        else:                                   # best line for these colours
            li = min(range(len(lines)), key=lambda l: sum(cdist(lines[l].colours[near(l, c)], c) for c in cols))
        for r in px:
            v = 0
            for c in r:
                col = pal[c] if c else bg[0][0]
                v = (v << 4) | (lines[li].index[maps[li][col]] if col in maps[li] else near(li, col))
            bat_words.append(v)
        bat_line.append(2 + li)
    pli = assign[('sp', 0)]
    frames = []
    for n in range(PLAYER_FRAMES):
        img = player_frame_pixels(n)
        # Genesis 4x4 sprite: tiles in column-major order
        for tx in range(4):
            for ty in range(4):
                for y in range(8):
                    v = 0
                    for x in range(8):
                        c = img[ty * 8 + y][tx * 8 + x]
                        v = (v << 4) | (lines[pli].index[maps[pli][sp[0][c]]] if c else 0)
                    frames.append(v)
    # object sprite cells: per (cell pair, palette) 8 tiles c0.TL c0.BL c0.TR
    # c0.BR c1.TL ... so a 32-wide PCE row is one 4x2 Genesis sprite
    spr_words, lut, nt = [], {}, 0
    pairs = sorted({(cell >> 1, p) for (cell, p) in use})
    for (pc, p) in pairs:
        if ('sp', p) not in assign:
            continue    # palette seen only on cells still blank at the snapshot
        li = assign[('sp', p)]
        for cell in (pc * 2, pc * 2 + 1):
            px = render.cell_pixels(m.vram, cell)
            for tx in (0, 1):
                for ty in (0, 1):
                    for y in range(8):
                        v = 0
                        for x in range(8):
                            c = px[ty * 8 + y][tx * 8 + x]
                            v = (v << 4) | (lines[li].index[maps[li][m.cram[256 + p * 16 + c]]] if c else 0)
                        spr_words.append(v)
        lut[(p, pc)] = nt
        nt += 8
    spr_line = [2 + assign[('sp', p)] if ('sp', p) in assign else 0xFF for p in range(16)]
    return dict(lines=lines, hud_bank=bank, hud_map=hud_map, player_line=2 + pli, hud_live=hud_live,
                player_frames=frames, assign=assign, bat_words=bat_words, bat_line=bat_line,
                spr_cmap=spr_cmap, spr_cmap13=spr_cmap13, spr_words=spr_words, spr_lut=lut,
                spr_ntiles=nt, spr_line=spr_line)


def player_anims():
    """bank 9 $DB23: 7 animation sets x 6 states x 4 steps of (ptr, attr)."""
    r = lambda a: level.bank_read(ROM, 9, a)
    w = lambda a: r(a) | (r(a + 1) << 8)
    out = []
    for st in range(7):
        sp = w(0xDB23 + 2 * st)
        for state in range(6):
            fp = w(sp + 2 * state)
            for step in range(4):
                lo, hi, at = r(fp + step * 3), r(fp + step * 3 + 1), r(fp + step * 3 + 2)
                ptr = lo | (hi << 8)
                if (ptr - 0xC000) % 0x180 or not 0xC000 <= ptr < 0xC000 + PLAYER_FRAMES * 0x180:
                    out.append((0xFF, 0xFF))    # not a frame entry (tables overlap)
                else:
                    out.append(((ptr - 0xC000) // 0x180, at))
    return out


def c_array(name, ctype, vals, per=16, fmt='0x%04X'):
    lines = []
    for i in range(0, len(vals), per):
        lines.append('    ' + ','.join(fmt % v for v in vals[i:i + per]) + ',')
    return 'const %s %s[%d] = {\n%s\n};\n' % (ctype, name, len(vals), '\n'.join(lines))


# BAT words drawn at run time outside the static HUD: the dialogue window
# font/frame (bank 2 $6DBA, palette 1 tiles $180-$23F) and the item-box
# icons of bank 4 $BADE (weapon box $4E07/$4E1E, magic $4DD1-$4DFE, items $4E35-$4EB3: word = tile $180+c, palette)
def bat_keys():
    # the window font (palette 1, $180-$23F), then the palette 5 and 15 fonts
    # ($1A0-$23F: STAGE card, stage clear tally, $A4CA boxes)
    keys = [0x1180 + i for i in range(0xC0)] + [0x51A0 + i for i in range(0xA0)] + \
           [0xF1A0 + i for i in range(0xA0)]
    icons = set()
    for p in [0x4E07, 0x4E1E] + list(range(0x4DD1, 0x4E07, 9)) + list(range(0x4E35, 0x4EB4, 9)):
        hi = level.bank_read(ROM, 1, p + 2) >> 4
        a = p + 3
        while level.bank_read(ROM, 1, a) != 0x5C:
            c = level.bank_read(ROM, 1, a)
            if c != 0x2F:
                icons.add((hi << 12) | (0x180 + c))
            a += 1
    icons |= {0x3200 | (0x21A + n) for n in range(8)}   # staff level digit (bank 0 $FE95)
    return keys + sorted(w for w in icons if w not in keys)


NGLYPH = 0x200          # glyph keys before the icons (renderer: glyph cache)


BAT_KEYS = None
TILE_BUDGET = 1488      # tiles 16..1503 (SS2 VRAM map: glyphs 1504-1567, icons 1568-1663, sprite cache above;
                        # plane B shares plane A's map, so $C000-$CFFF is tile space)
ICON_SLOTS = 96
NSTAGES = 7


def main():
    global BAT_KEYS
    BAT_KEYS = bat_keys()
    hud_m = snapshot.stage1_start()
    anims = player_anims()
    src = ['/* Generated by tools/ss2/gen_ss2.py from the Son Son II HuCard (sha1 %s). Do not edit. */'
           % hashlib.sha1(ROM).hexdigest(), '#include "ss2_data.h"', '']
    src.append(c_array('ss2_bat_keys', 'u16', BAT_KEYS))
    src.append(c_array('ss2_player_anim', 'u8', [v for f, a in anims for v in (f, a)], 24, '0x%02X'))
    desc = []
    for k in range(NSTAGES):
        m = snapshot.stage_start(k)
        use = pickle.load(open(os.path.join(ROOT, 'out', 'ss2', 's%d_sprite_use.pkl' % k), 'rb'))
        st = build_stage(k)
        sh = build_sprites_and_hud(m, k, hud_m, use)
        n = 's%d' % k
        pal = st['lines'][0].words() + st['lines'][1].words() + sh['lines'][0].words() + sh['lines'][1].words()
        src.append(c_array('ss2_%s_palette' % n, 'u16', pal))
        src.append(c_array('ss2_%s_tiles' % n, 'u32', st['bank'].words(), 8, '0x%08X'))
        src.append(c_array('ss2_%s_meta' % n, 'u16', st['meta']))
        src.append(c_array('ss2_%s_attr' % n, 'u16', st['attr']))
        src.append(c_array('ss2_%s_hud_tiles' % n, 'u32', sh['hud_bank'].words(), 8, '0x%08X'))
        src.append(c_array('ss2_%s_hud_map' % n, 'u16', sh['hud_map']))
        hl = sh['hud_live']
        src.append(c_array('ss2_%s_hud_digit' % n, 'u16', [hl[0x01B0 + d] for d in range(10)]))
        bar = [0] * 64
        for t in (0x0C, 0x0D, 0x0E, 0x0F, 0x3A, 0x3B, 0x3C):
            bar[t] = hl[0x2100 | ((0x80 + t) & 0xFF) | (0x0100 if t + 0x80 > 0xFF else 0)]
        src.append(c_array('ss2_%s_hud_bar' % n, 'u16', bar))
        src.append(c_array('ss2_%s_player_tiles' % n, 'u32', sh['player_frames'], 8, '0x%08X'))
        src.append(c_array('ss2_%s_spr_tiles' % n, 'u32', sh['spr_words'], 8, '0x%08X'))
        lut = [0] * (16 * 256)
        for (p, pc), t in sh['spr_lut'].items():
            lut[p * 256 + pc] = t + 1                   # 0 = not converted
        src.append(c_array('ss2_%s_spr_lut' % n, 'u16', lut))
        src.append(c_array('ss2_%s_spr_line' % n, 'u8', sh['spr_line'], 16, '0x%02X'))
        src.append(c_array('ss2_%s_bat_tiles' % n, 'u32', sh['bat_words'], 8, '0x%08X'))
        src.append(c_array('ss2_%s_bat_line' % n, 'u8', sh['bat_line'], 16, '0x%02X'))
        src.append(c_array('ss2_%s_spr_cmap' % n, 'u8', sh['spr_cmap'], 16, '0x%02X'))
        src.append(c_array('ss2_%s_spr_cmap13' % n, 'u8', sh['spr_cmap13'], 16, '0x%02X'))
        # temporary bootstrap: stage start work RAM from the research snapshot
        src.append(c_array('ss2_%s_ram' % n, 'u8', list(m.ram), 32, '0x%02X'))
        total = len(st['bank'].tiles) + len(sh['hud_bank'].tiles) + 16 + sh['spr_ntiles']
        assert len(BAT_KEYS) - NGLYPH <= ICON_SLOTS
        assert total <= TILE_BUDGET, 'stage %d needs %d VRAM tiles' % (k + 1, total)
        desc.append('    { ss2_%s_palette, ss2_%s_tiles, %d, ss2_%s_meta, ss2_%s_attr, %d,\n'
                    '      ss2_%s_hud_tiles, %d, ss2_%s_hud_map, ss2_%s_hud_digit, ss2_%s_hud_bar,\n'
                    '      ss2_%s_player_tiles, %d, ss2_%s_spr_tiles, %d, ss2_%s_spr_lut, ss2_%s_spr_line, ss2_%s_ram,\n'
                    '      ss2_%s_bat_tiles, ss2_%s_bat_line, ss2_%s_spr_cmap, ss2_%s_spr_cmap13 },'
                    % (n, n, len(st['bank'].tiles), n, n, st['nmeta'], n, len(sh['hud_bank'].tiles), n, n, n,
                       n, sh['player_line'], n, sh['spr_ntiles'], n, n, n, n, n, n, n))
        print('stage %d: bg %d tiles, %d metatiles, hud %d, sprites %d (%d pairs), total %d/%d, bg lines %s, spr lines %s'
              % (k + 1, len(st['bank'].tiles), st['nmeta'], len(sh['hud_bank'].tiles), sh['spr_ntiles'],
                 len(sh['spr_lut']), total, TILE_BUDGET, [len(l.colours) - 1 for l in st['lines']],
                 [len(l.colours) - 1 for l in sh['lines']]))
    src.append('const Ss2StageGfx ss2_stage_gfx[%d] = {\n%s\n};\n' % (NSTAGES, '\n'.join(desc)))
    open(os.path.join(ROOT, 'src', 'ss2_data.c'), 'w').write('\n'.join(src))
    hdr = '''/* Generated by tools/ss2/gen_ss2.py. Do not edit. */
#ifndef SS2_DATA_H
#define SS2_DATA_H
#include <genesis.h>

#define SS2_NSTAGES       %d
#define SS2_PLAYER_FRAMES %d

/* per-stage presentation data (palette lines 0-1 stage BG, 2-3 sprites+HUD) */
typedef struct
{
    const u16 *palette;         /* 64 CRAM words                              */
    const u32 *bg_tiles;
    u16 bg_ntiles;
    const u16 *meta;            /* 4x4 tile words per metatile                */
    const u16 *attr;            /* block attribute nibbles (TL,TR,BL,BR)      */
    u16 nmeta;
    const u32 *hud_tiles;
    u16 hud_ntiles;
    const u16 *hud_map;         /* 32x6 window words                          */
    const u16 *hud_digit;       /* PCE tile $1B0+d                            */
    const u16 *hud_bar;         /* PCE tile $180+t (palette 2), by t          */
    const u32 *player_tiles;    /* 21 frames x 16 tiles, column-major 4x4      */
    u16 player_pal;
    const u32 *spr_tiles;
    u16 spr_ntiles;
    const u16 *spr_lut;         /* [pce palette][cell>>1] -> tile+1 (0 none)  */
    const u8 *spr_line;         /* pce sprite palette -> CRAM line            */
    const u8 *ram;              /* stage start work RAM (bootstrap)           */
    const u32 *bat_tiles;       /* tile per ss2_bat_keys entry                */
    const u8 *bat_line;         /* its CRAM line                              */
    const u8 *spr_cmap;         /* [pce sprite palette][colour 0-7]: line-2 << 7 | index */
    const u8 *spr_cmap13;       /* [interior variant][colour]: palette 13 after a door */
} Ss2StageGfx;

#define SS2_BAT_NKEYS %d
extern const u16 ss2_bat_keys[SS2_BAT_NKEYS];   /* $1180-$123F glyphs, then icons */
extern const Ss2StageGfx ss2_stage_gfx[SS2_NSTAGES];
extern const u8 ss2_player_anim[7 * 6 * 4 * 2];   /* set,state,step -> frame,attr */

#endif
''' % (NSTAGES, PLAYER_FRAMES, len(BAT_KEYS))
    open(os.path.join(ROOT, 'inc', 'ss2_data.h'), 'w').write(hdr)
    write_rom()


def write_rom():
    # source banks whose tables the native logic reads (ss2_logic.h)
    rom = ['/* Generated by tools/ss2/gen_ss2.py: HuCard banks read as data tables. Do not edit. */',
           '#include "ss2_logic.h"', '']
    # 0-9, $0F tables; $0C-$0F stage maps (bank 0 $F9F0 copies them to $3000); $10-$1F sprite pattern sources (run-time conversion)
    for b in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x0C, 0x0D, 0x0E, 0x0F) + tuple(range(0x10, 0x20)):
        rom.append(c_array('ss2_bank%X' % b, 'u8', list(bank_bytes(b, 0, 0x2000)), 32, '0x%02X'))
    open(os.path.join(ROOT, 'src', 'ss2_rom.c'), 'w').write('\n'.join(rom))


if __name__ == '__main__':
    if '--rom-only' in sys.argv:
        write_rom()
    else:
        main()

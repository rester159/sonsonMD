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


def build_sprites_and_hud(m, s):
    sp = stage_sp_palettes(s)
    bg = stage_bg_palettes(s)
    # HUD: PCE BAT rows 1-6 (screen lines 8-55, BYR 0) of the stage-1 snapshot
    hud_cells = [[m.vram[r * 64 + c] for c in range(32)] for r in range(1, 7)]
    weights = {}
    def addw(key, col, n=1):
        w = weights.setdefault(key, {}); w[col] = w.get(col, 0) + n
    for row in hud_cells:
        for e in row:
            for r in render.tile_pixels(m.vram, e & 0xFFF):
                for c in r:
                    addw(('hud', e >> 12), bg[e >> 12][c] if c else bg[0][0], 50)
    for n in range(PLAYER_FRAMES):
        for r in player_frame_pixels(n):
            for c in r:
                if c: addw(('sp', 0), sp[0][c], 4)
    # stage-1 enemy/effect palettes observed in research play (SP3/6/7/15)
    for p in (3, 6, 7, 15):
        for c in range(1, 8):
            addw(('sp', p), sp[p][c], 10)
    assign, maps, lines = fit_lines(weights, 2)
    bank = TileBank()
    hud_map = []
    for row in hud_cells:
        for e in row:
            key = ('hud', e >> 12); li = assign[key]
            px = render.tile_pixels(m.vram, e & 0xFFF)
            # HUD index 0 is opaque backdrop black on the PCE: keep it opaque
            pxm = [[lines[li].index[maps[li][bg[e >> 12][c] if c else bg[0][0]]] for c in r] for r in px]
            idx, flags = bank.add(pxm)
            hud_map.append(((2 + li) << 13) | 0x8000 | flags | idx)
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
    return dict(lines=lines, hud_bank=bank, hud_map=hud_map, player_line=2 + pli,
                player_frames=frames, assign=assign)


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


def main():
    m = snapshot.stage1_start()
    st = build_stage(0)
    sh = build_sprites_and_hud(m, 0)
    anims = player_anims()
    src = ['/* Generated by tools/ss2/gen_ss2.py from the Son Son II HuCard (sha1 %s). Do not edit. */'
           % hashlib.sha1(ROM).hexdigest(), '#include "ss2_data.h"', '']
    pal = st['lines'][0].words() + st['lines'][1].words() + sh['lines'][0].words() + sh['lines'][1].words()
    src.append(c_array('ss2_s0_palette', 'u16', pal))
    src.append(c_array('ss2_s0_tiles', 'u32', st['bank'].words(), 8, '0x%08X'))
    src.append(c_array('ss2_s0_meta', 'u16', st['meta']))
    src.append(c_array('ss2_s0_attr', 'u16', st['attr']))
    src.append(c_array('ss2_s0_map', 'u8', list(st['map']), 32, '0x%02X'))
    src.append(c_array('ss2_hud_tiles', 'u32', sh['hud_bank'].words(), 8, '0x%08X'))
    src.append(c_array('ss2_hud_map', 'u16', sh['hud_map']))
    src.append(c_array('ss2_player_tiles', 'u32', sh['player_frames'], 8, '0x%08X'))
    src.append(c_array('ss2_player_anim', 'u8', [v for f, a in anims for v in (f, a)], 24, '0x%02X'))
    open(os.path.join(ROOT, 'src', 'ss2_data.c'), 'w').write('\n'.join(src))
    hdr = '''/* Generated by tools/ss2/gen_ss2.py. Do not edit. */
#ifndef SS2_DATA_H
#define SS2_DATA_H
#include <genesis.h>

#define SS2_S0_NTILES     %d
#define SS2_S0_NMETA      %d
#define SS2_HUD_NTILES    %d
#define SS2_PLAYER_FRAMES %d
#define SS2_PLAYER_PAL    %d

extern const u16 ss2_s0_palette[64];
extern const u32 ss2_s0_tiles[SS2_S0_NTILES * 8];
extern const u16 ss2_s0_meta[SS2_S0_NMETA * 16];   /* 4x4 tile words per metatile */
extern const u16 ss2_s0_attr[SS2_S0_NMETA];        /* 4 block attribute nibbles (TL,TR,BL,BR) */
extern const u8  ss2_s0_map[4096];                 /* 8x8 screens of 8x8 metatiles */
extern const u32 ss2_hud_tiles[SS2_HUD_NTILES * 8];
extern const u16 ss2_hud_map[32 * 6];
extern const u32 ss2_player_tiles[SS2_PLAYER_FRAMES * 16 * 8];
extern const u8  ss2_player_anim[7 * 6 * 4 * 2];   /* set,state,step -> frame,attr */

#endif
''' % (len(st['bank'].tiles), st['nmeta'], len(sh['hud_bank'].tiles), PLAYER_FRAMES, sh['player_line'])
    open(os.path.join(ROOT, 'inc', 'ss2_data.h'), 'w').write(hdr)
    # source banks whose tables the native logic reads (ss2_logic.h)
    rom = ['/* Generated by tools/ss2/gen_ss2.py: HuCard banks read as data tables. Do not edit. */',
           '#include "ss2_logic.h"', '']
    for b in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x17):
        rom.append(c_array('ss2_bank%X' % b, 'u8', list(bank_bytes(b, 0, 0x2000)), 32, '0x%02X'))
    open(os.path.join(ROOT, 'src', 'ss2_rom.c'), 'w').write('\n'.join(rom))
    print('stage0 tiles', len(st['bank'].tiles), 'metatiles', st['nmeta'],
          'hud tiles', len(sh['hud_bank'].tiles), 'player line', sh['player_line'])
    print('bg lines', [len(l.colours) - 1 for l in st['lines']], 'sprite/hud lines', [len(l.colours) - 1 for l in sh['lines']])


if __name__ == '__main__':
    main()

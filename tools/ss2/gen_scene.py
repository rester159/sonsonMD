"""gen_scene.py -- full-screen pictures shown over the playfield (scenes other
than the title, which has gen_title.py): their BAT words -> Genesis tiles.

Each scene comes from research-model pickles taken while it is up. The BAT
words in the given rows become a sorted key table with tiles and a 2-line
BG palette (CRAM lines 0-1). Scenes may also bring their own sprite lines
(CRAM 2-3) and sprite colour map, as the title does.

Scenes (ss2_scene ids, inc/ss2_logic.h):
  2 CONTINUE  out/ss2/cont_s.pkl    the revive picture (bank 2 $7ADE, $FBB1 $FC32),
              BAT rows 7-29 (under the HUD); font, box and window words
              (palettes 1/5/15 at tiles $180-$23F, the blank $01A0) are left
              to the play-scene glyph path (the stage palette is up)
  3 ENDING    out/ss2/end_*.pkl     (tools/ss2/research/ending): the picture
              (bank $1E/$1F blocks, $FBB1 $B7F8, rows 0-27), its dialogue
              window, the text band below it (rows 28-63, palette 1 glyphs:
              the whole font), the credits pages (palette 5); sprite lines
              from the picture's sprite palettes (0-7 common, 8-15 $4A6C)

The credits font (bank $18 $C600, VRAM tiles $1C0-$1DF, palette 5) slides in
and out of its cells (ss2_end_font_dx), so it is not keyed: ss2_end_font
holds the 32 tiles at rest for the renderer to shift.

Output: src/ss2_scene_data.c, inc/ss2_scene_data.h.
"""
import os, pickle, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.setrecursionlimit(10000)
import render
from gen_ss2 import c_array, ROOT, fit_lines

OUT = os.path.join(ROOT, 'out', 'ss2')


def glyph(w):
    p, t = w >> 12, w & 0xFFF
    return w == 0x01A0 or (p in (1, 5, 15) and 0x180 <= t < 0x240)


def credit_font(w):
    return 0x1C0 <= (w & 0xFFF) < 0x1E0


def load(name):
    return pickle.load(open(os.path.join(OUT, name), 'rb'))


def bat_words(m, rows, keep=lambda w: True):
    bw, _ = render.bg_size(m.vreg)
    return {w for w in (m.vram[r * bw + c] for r in rows for c in range(bw)) if keep(w)}


# scene: list of (pickle, words) groups; the first group's model gives the
# sprite palettes when 'sprites' lists pickles
def scene_continue():
    m = load('cont_s.pkl')
    return [(m, bat_words(m, range(7, 30), lambda w: not glyph(w)))], None


def scene_ending():
    pic = load('end_pic.pkl')
    groups = [(pic, bat_words(pic, range(0, 28)) | {0x1180 + c for c in range(0xC0)} | {0x1202, 0x01A0})]
    for n in ('end_pic_msg.pkl',):
        m = load(n)
        groups.append((m, bat_words(m, range(0, 32))))
    for n in ('end_pic_text.pkl', 'end_pic_scrolled.pkl'):
        m = load(n)
        groups.append((m, bat_words(m, range(28, 64))))
    cred = [load('end_credits%d.pkl' % i) for i in range(1, 8)]
    words = set()
    for m in cred:
        words |= bat_words(m, range(0, 32), lambda w: not credit_font(w))
    groups.append((cred[0], words))
    sprites = [pic] + [load(n) for n in ('end_pic_msg.pkl', 'end_pic_scrolled.pkl', 'end_pic_text.pkl')]
    return groups, (sprites, cred[0])


def colour_fn(m):
    return lambda p, c: m.cram[p * 16 + c] if c else m.cram[0]


def convert(groups, font=None):
    # a word drawn under several CRAMs keeps the first group's colours
    seen, items = set(), []
    for gi, (m, ws) in enumerate(groups):
        for w in sorted(ws):
            if w not in seen:
                seen.add(w)
                items.append((w, gi))
    cram_id = {}
    gkey = [cram_id.setdefault(tuple(m.cram[:256]), len(cram_id)) for m, _ in groups]
    weights = {}

    def add(gi, m, p, t):
        col = colour_fn(m)
        for r in render.tile_pixels(m.vram, t):
            for c in r:
                d = weights.setdefault((gkey[gi], p), {})
                d[col(p, c)] = d.get(col(p, c), 0) + 1
    for w, gi in items:
        add(gi, groups[gi][0], w >> 12, w & 0xFFF)
    if font:
        fgi = len(groups)
        gkey.append(cram_id.setdefault(tuple(font.cram[:256]), len(cram_id)))
        groups = groups + [(font, set())]
        for t in range(0x1C0, 0x1E0):
            add(fgi, font, 5, t)
    assign, maps, lines = fit_lines(weights, 2)

    def tile(gi, p, t):
        m = groups[gi][0]
        col = colour_fn(m)
        li = assign[(gkey[gi], p)]
        out = []
        for r in render.tile_pixels(m.vram, t):
            v = 0
            for c in r:
                v = (v << 4) | lines[li].index[maps[li][col(p, c)]]
            out.append(v)
        return li, out
    keys, tiles, tline = [], [], []
    for w, gi in sorted(items):
        li, px = tile(gi, w >> 12, w & 0xFFF)
        keys.append(w)
        tiles += px
        tline.append(li)
    ftiles, fline = [], 0
    if font:
        for t in range(0x1C0, 0x1E0):
            fline, px = tile(fgi, 5, t)
            ftiles += px
    return keys, tiles, tline, lines[0].words() + lines[1].words(), ftiles, fline


def cdist(a, b):
    return sum((((a >> sh) & 7) - ((b >> sh) & 7)) ** 2 for sh in (0, 3, 6))


def sprite_lines(models):
    """sprite palettes in use (SAT) -> 2 lines and a 16x8 colour map"""
    m0 = models[0]
    used = set()
    for m in models:
        sat = m.satb
        for i in range(64):
            sy, sx, pat, at = sat[i * 4:i * 4 + 4]
            if sy & 0x3FF:
                used.add(at & 15)
    sw = {}
    for p in sorted(used):
        for c in range(1, 8):
            col = m0.cram[256 + p * 16 + c]
            d = sw.setdefault(p, {})
            d[col] = d.get(col, 0) + 1
    sassign, smaps, slines = fit_lines(sw, 2)

    def near(li, col):
        return min(range(1, len(slines[li].colours)), key=lambda i: cdist(slines[li].colours[i], col))
    cmap = []
    for p in range(16):
        cols = [m0.cram[256 + p * 16 + c] for c in range(8)]
        if p in sassign:
            li = sassign[p]
            idx = [slines[li].index[smaps[li][cols[c]]] if cols[c] in smaps[li] else near(li, cols[c])
                   for c in range(1, 8)]
        else:
            li = min([l for l in range(2) if len(slines[l].colours) > 1],
                     key=lambda l: sum(cdist(slines[l].colours[near(l, c)], c) for c in cols[1:]))
            idx = [near(li, c) for c in cols[1:]]
        cmap += [0] + [(li << 7) | i for i in idx]
    return slines[0].words() + slines[1].words(), cmap, sorted(used)


SCENES = [('continue', scene_continue), ('ending', scene_ending)]


def main():
    src = ['/* Generated by tools/ss2/gen_scene.py. Do not edit. */', '#include "ss2_scene_data.h"', '']
    desc = []
    font_out = None
    for name, fn in SCENES:
        try:
            groups, spr = fn()
        except FileNotFoundError as e:
            desc.append('    { 0, 0, 0, 0, 0, 0, 0 },    /* %s: not captured yet (%s) */' % (name, e.filename))
            continue
        n = 'ss2_scene_%s' % name
        keys, tiles, tline, pal, ftiles, fline = convert(groups, spr[1] if spr else None)
        src += [c_array(n + '_palette', 'u16', pal), c_array(n + '_keys', 'u16', keys),
                c_array(n + '_tiles', 'u32', tiles, 8, '0x%08X'), c_array(n + '_line', 'u8', tline, 16, '0x%02X')]
        sp, cm = '0', '0'
        if spr:
            spal, cmap, used = sprite_lines(spr[0])
            src += [c_array(n + '_spr_palette', 'u16', spal), c_array(n + '_cmap', 'u8', cmap, 16, '0x%02X')]
            sp, cm = n + '_spr_palette', n + '_cmap'
            print('%s: sprite palettes %s' % (name, used))
        if ftiles:
            font_out = (ftiles, fline)
        desc.append('    { %s_palette, %s_keys, %s_tiles, %s_line, %d, %s, %s },' % (n, n, n, n, len(keys), sp, cm))
        print('%s: %d BAT words' % (name, len(keys)))
    ftiles, fline = font_out or ([0] * 256, 0)
    src.append(c_array('ss2_end_font', 'u32', ftiles, 8, '0x%08X'))
    src.append('const u8 ss2_end_font_line = %d;\n' % fline)
    src.append('const Ss2SceneBg ss2_scene_bg[%d] = {\n%s\n};\n' % (len(SCENES), '\n'.join(desc)))
    open(os.path.join(ROOT, 'src', 'ss2_scene_data.c'), 'w').write('\n'.join(src))
    open(os.path.join(ROOT, 'inc', 'ss2_scene_data.h'), 'w').write('''/* Generated by tools/ss2/gen_scene.py. Do not edit. */
#ifndef SS2_SCENE_DATA_H
#define SS2_SCENE_DATA_H
#include <genesis.h>

/* a picture scene: tiles keyed by BAT word, CRAM lines 0-1; optionally its
 * own sprite lines (CRAM 2-3) and sprite colour map */
typedef struct
{
    const u16 *palette;         /* 32 words */
    const u16 *keys;            /* sorted BAT words */
    const u32 *tiles;
    const u8 *line;             /* CRAM line 0/1 per key */
    u16 nkeys;
    const u16 *spr_palette;     /* 32 words or 0 (the stage's) */
    const u8 *cmap;             /* sprite palette, colour -> (line-2) << 7 | index, or 0 */
} Ss2SceneBg;

#define SS2_SCENE_BG_FIRST 2    /* ss2_scene_bg[id - 2] */
extern const Ss2SceneBg ss2_scene_bg[%d];

/* the ending credits font (VRAM $1C0-$1DF, palette 5) at rest */
extern const u32 ss2_end_font[32 * 8];
extern const u8 ss2_end_font_line;

#endif
''' % len(SCENES))


if __name__ == '__main__':
    main()

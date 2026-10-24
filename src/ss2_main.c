/* SON SON II -- stage world, camera streaming and HUD.
 *
 * Source: the HuCard's stage format (bank 0 $E92C/$EA4A streamers): a 2048 px
 * square wrapping world of 8x8 screens, each 8x8 metatiles of 32x32 px.
 * The PCE frame is 256x240 with a 48-line HUD under an 8-line black band; the
 * Genesis 224-line frame drops that band and the bottom 8 playfield lines,
 * keeping the HUD in the window plane (rows 0-5) and 176 playfield lines.
 *
 * VRAM: SGDK 64x32 layout (plane B $C000, window $D000, plane A $E000).
 * Plane A holds a 64x32-tile ring of the world; columns/rows entering the
 * view are written before the scroll that exposes them.
 */
#include <genesis.h>
#include "ss2.h"
#include "ss2_data.h"
#include "ss2_ram.h"
#include "ss2_logic.h"
#include "ss2_sound.h"
#include "sound.h"

/* VRAM tile bases for the current stage (stage_gfx_load) */
#define BG_TILE      TILE_USER_INDEX
static u16 HUD_TILE, PLAYER_TILE, SPR_TILE;
static const Ss2StageGfx *gfx;
static void cache_reset(void);

#define HUD_ROWS     6
#define VIEW_TOP     (HUD_ROWS * 8)
#define VIEW_W       256
#define VIEW_H       176
#define NCOLS        33          /* tile columns touched by a 256 px view */
#define NROWS        23          /* tile rows touched by a 176 px view    */

/* camx/camy is the world pixel at the top-left of the Genesis playfield
 * (line 48). The game's camera ($45:$46, $48:$49) relates as
 * camx = cam_x, camy = cam_y + 61: the first PCE playfield line (57) shows
 * cam_y + 62 (BYR latch after the raster split) and Genesis line g shows
 * PCE line g + 8. */
static s16 camx, camy;
static s16 drawn_tx, drawn_ty;
static u16 linebuf[64];

/* the stage map lives in work RAM at $3000 (copied from ROM at stage load) */
static u16 world_tile(u16 tx, u16 ty)
{
    u8 m;
    tx &= 255;
    ty &= 255;
    m = R(0x3000 + (((ty >> 5) << 3) | (tx >> 5)) * 64 + ((ty >> 2) & 7) * 8 + ((tx >> 2) & 7));
    return (u16)(gfx->meta[m * 16 + (ty & 3) * 4 + (tx & 3)] + BG_TILE);
}

static void draw_column(s16 tx, s16 ty0)
{
    u16 i, py = (u16)(ty0 & 31), n1;
    for (i = 0; i < NROWS; i++)
        linebuf[i] = world_tile((u16)tx, (u16)(ty0 + i));
    n1 = (u16)(32 - py);
    if (n1 > NROWS) n1 = NROWS;
    VDP_setTileMapDataColumn(BG_A, linebuf, (u16)(tx & 63), py, n1, 1, CPU);
    if (n1 < NROWS)
        VDP_setTileMapDataColumn(BG_A, linebuf + n1, (u16)(tx & 63), 0, (u16)(NROWS - n1), 1, CPU);
}

static void draw_row(s16 ty, s16 tx0)
{
    u16 i, px = (u16)(tx0 & 63), n1;
    for (i = 0; i < NCOLS; i++)
        linebuf[i] = world_tile((u16)(tx0 + i), (u16)ty);
    n1 = (u16)(64 - px);
    if (n1 > NCOLS) n1 = NCOLS;
    VDP_setTileMapDataRow(BG_A, linebuf, (u16)(ty & 31), px, n1, CPU);
    if (n1 < NCOLS)
        VDP_setTileMapDataRow(BG_A, linebuf + n1, (u16)(ty & 31), 0, (u16)(NCOLS - n1), CPU);
}

static void redraw_view(void)
{
    s16 i;
    drawn_tx = (s16)(camx >> 3);
    drawn_ty = (s16)(camy >> 3);
    for (i = 0; i < NCOLS; i++)
        draw_column((s16)(drawn_tx + i), drawn_ty);
}

/* Bring the ring up to date for the current camera, columns first so a
 * diagonal move fills the corner from the row pass. */
static void stream_view(void)
{
    s16 tx = (s16)(camx >> 3), ty = (s16)(camy >> 3);
    if (tx - drawn_tx > 8 || drawn_tx - tx > 8 || ty - drawn_ty > 8 || drawn_ty - ty > 8)
    {
        redraw_view();
        return;
    }
    while (drawn_tx < tx) { drawn_tx++; draw_column((s16)(drawn_tx + NCOLS - 1), drawn_ty); }
    while (drawn_tx > tx) { drawn_tx--; draw_column(drawn_tx, drawn_ty); }
    while (drawn_ty < ty) { drawn_ty++; draw_row((s16)(drawn_ty + NROWS - 1), drawn_tx); }
    while (drawn_ty > ty) { drawn_ty--; draw_row(drawn_ty, drawn_tx); }
}

/* plane B shares plane A's map (its own table area becomes sprite cache)
 * and scrolls with it, so it sits exactly behind plane A */
static void apply_scroll(void)
{
    VDP_setHorizontalScroll(BG_A, (s16)-camx);
    VDP_setVerticalScroll(BG_A, (s16)(camy - VIEW_TOP));
    VDP_setHorizontalScroll(BG_B, (s16)-camx);
    VDP_setVerticalScroll(BG_B, (s16)(camy - VIEW_TOP));
}

/* ---- dialogue window and HUD icons (ss2_bat_queue) ---------------------
 * Window cells are PCE BG tiles $180+ch (palette 1) placed at world-tile
 * positions: they go into the plane A ring at (tx & 63, ty & 31); the
 * camera holds still while a window is up. Glyph tiles are uploaded on
 * demand into tiles 1504-1567. HUD icon tables ($BADE) go to the window
 * plane; their tiles stay resident at 1568-1663. SonSon gets SGDK's font back when plane B returns to $C000
 * (SGDK reloads it whenever the lowest table address moves). */
#define GLYPH_TILE   1504           /* SS2 VRAM map (gen_ss2.py TILE_BUDGET) */
#define GLYPH_SLOTS  64
#define ICON_TILE    1568
#define NGLYPH       0xC0           /* ss2_bat_keys[0..$BF] = $1180-$123F */
static u8 glyph_slot[NGLYPH];       /* slot + 1, 0 = not loaded */
static u8 glyph_next;

static void glyphs_reset(void)
{
    memset(glyph_slot, 0, sizeof glyph_slot);
    glyph_next = 0;
}

static u16 bat_key(u16 w)
{
    u16 i;
    for (i = NGLYPH; i < SS2_BAT_NKEYS; i++)
        if (ss2_bat_keys[i] == w) return i;
    return 0xFFFF;
}

/* Genesis map word for a PCE BAT word (palette 1 glyphs or HUD icons) */
static u16 bat_word(u16 w)
{
    u16 k;
    if (w >= 0x1180 && w < 0x1180 + NGLYPH)
    {
        k = (u16)(w - 0x1180);
        if (!glyph_slot[k])
        {
            if (glyph_next >= GLYPH_SLOTS) glyph_next = 0;     /* full: reuse */
            glyph_slot[k] = (u8)(glyph_next + 1);
            VDP_loadTileData(gfx->bat_tiles + k * 8, (u16)(GLYPH_TILE + glyph_next), 1, DMA_QUEUE);
            glyph_next++;
        }
        return TILE_ATTR_FULL(gfx->bat_line[k], 1, 0, 0, GLYPH_TILE + glyph_slot[k] - 1);
    }
    k = bat_key(w);
    if (k == 0xFFFF) return 0;
    return TILE_ATTR_FULL(gfx->bat_line[k], 1, 0, 0, ICON_TILE + k - NGLYPH);
}

/* $BADE table: absolute BAT address, palette, characters, '/' row, '\' end;
 * the HUD shows PCE BAT rows 1-6 as window rows 0-5 */
static void hud_text(u16 ptr)
{
    u16 a = (u16)(ss2_rom_byte(ptr) | (ss2_rom_byte((u16)(ptr + 1)) << 8));
    u16 hi = (u16)(ss2_rom_byte((u16)(ptr + 2)) >> 4);
    u16 col = (u16)(a & 63), row = (u16)(a >> 6);
    u8 c;
    ptr = (u16)(ptr + 3);
    while ((c = ss2_rom_byte(ptr++)) != 0x5C)
    {
        if (c == 0x2F) { row++; col = (u16)(a & 63); continue; }
        if (row >= 1 && row <= HUD_ROWS && col < 32)
            VDP_setTileMapXY(WINDOW, bat_word((u16)((hi << 12) | (0x180 + c))), col, (u16)(row - 1));
        col++;
    }
}

static struct { u16 a, ptr; } hud_last[16];   /* table shown per BAT address */

static void bat_drain(void)
{
    while (ss2_bat_tail != ss2_bat_head)
    {
        const Ss2BatOp *o = &ss2_bat_queue[ss2_bat_tail & (SS2_BATQ - 1)];
        ss2_bat_tail++;
        if (o->kind == SS2_BAT_PUT)
            VDP_setTileMapXY(BG_A, bat_word(o->w), (u16)(o->x & 63), (u16)(o->y & 31));
        else if (o->kind == SS2_BAT_RESTORE)
        {
            u8 w = (u8)(o->w >> 8), h = (u8)o->w, i, j;
            for (j = 0; j < h; j++)
                for (i = 0; i < w; i++)
                {
                    u8 tx = (u8)(o->x + i), ty = (u8)(o->y + j);
                    VDP_setTileMapXY(BG_A, world_tile(tx, ty), (u16)(tx & 63), (u16)(ty & 31));
                }
            if (w <= 6) glyphs_reset();          /* last close step: window gone */
        }
        else if (o->kind == SS2_BAT_HUDPUT)
        {
            if (o->y >= 1 && o->y <= HUD_ROWS && o->x < 32)
                VDP_setTileMapXY(WINDOW, bat_word(o->w), o->x, (u16)(o->y - 1));
        }
        else
        {
            /* the weapon box redraws every frame: skip a table already
             * showing at its BAT address */
            u16 a = (u16)(ss2_rom_byte(o->w) | (ss2_rom_byte((u16)(o->w + 1)) << 8));
            u8 i;
            for (i = 0; i < 16 && hud_last[i].a && hud_last[i].a != a; i++) { }
            if (i == 16) i = 15;
            if (hud_last[i].a != a || hud_last[i].ptr != o->w)
            {
                hud_last[i].a = a;
                hud_last[i].ptr = o->w;
                hud_text(o->w);
            }
        }
    }
}

/* ---- palette fades ($71BA etc.): the stage palette scaled per component
 * through the game's level table $75FA. Lines 0-1 (stage BG) follow the
 * BG level; lines 2-3 follow the sprite level except entries the HUD
 * uses, which the PCE never fades (its HUD palettes are outside the
 * faded ranges). */
static u16 hud_keep[2];
static u8 shown_bg, shown_sp;
static u16 fade_pal[64];

static void hud_mark(u16 w)
{
    const u32 *t = gfx->hud_tiles + (w & 0x7FF) * 8;
    u8 line = (u8)((w >> 13) & 3), r, x;
    if (line < 2) return;
    for (r = 0; r < 8; r++)
        for (x = 0; x < 8; x++)
            hud_keep[line - 2] |= (u16)(1 << ((t[r] >> (x * 4)) & 15));
}

static void hud_colours(void)
{
    u16 i;
    hud_keep[0] = hud_keep[1] = 0;
    for (i = 0; i < 32 * HUD_ROWS; i++) hud_mark(gfx->hud_map[i]);
    for (i = 0; i < 10; i++) hud_mark(gfx->hud_digit[i]);
    for (i = 0; i < 64; i++) hud_mark(gfx->hud_bar[i]);
}

static u16 fade_colour(u16 w, u8 l)
{
    u8 b = (u8)((w >> 9) & 7), g = (u8)((w >> 5) & 7), r = (u8)((w >> 1) & 7);
    b = ss2_rom_byte((u16)(0x75FA + (l << 3) + b));
    g = ss2_rom_byte((u16)(0x75FA + (l << 3) + g));
    r = ss2_rom_byte((u16)(0x75FA + (l << 3) + r));
    return (u16)((b << 9) | (g << 5) | (r << 1));
}

/* $2201 (VDC control shadow): bit 7 background, bit 6 sprites. With the
 * background off the PCE shows colour $100 there; both off blanks the
 * picture (HUD included) to it: the freeze flash and the sweep magic. */
static u8 shown_cr = 0xC0;
static u16 shown_c100;

static u16 pce_colour(u16 c)
{
    return (u16)(((c & 7) << 9) | (((c >> 6) & 7) << 5) | (((c >> 3) & 7) << 1));
}

static void apply_fade(void)
{
    u8 i, cr = (u8)(R(0x2201) & 0xC0);
    u16 c100 = pce_colour(ss2_vce[0x100]);
    if (ss2_fade_bg == shown_bg && ss2_fade_sp == shown_sp && cr == shown_cr && c100 == shown_c100) return;
    if ((cr == 0) != (shown_cr == 0)) VDP_setEnable(cr != 0);
    shown_bg = ss2_fade_bg;
    shown_sp = ss2_fade_sp;
    shown_cr = cr;
    shown_c100 = c100;
    for (i = 0; i < 64; i++)
    {
        u16 w = gfx->palette[i];
        if (i < 32) w = (cr & 0x80) ? fade_colour(w, shown_bg) : c100;
        else if (!(hud_keep[(i >> 4) - 2] & (1 << (i & 15)))) w = fade_colour(w, shown_sp);
        fade_pal[i] = w;
    }
    if (!(cr & 0x80)) fade_pal[0] = c100;           /* backdrop */
    PAL_setColors(0, fade_pal, 64, DMA_QUEUE);
}

static void video_init(void)
{
    VDP_setEnable(FALSE);
    VDP_setScreenWidth256();
    VDP_setPlaneSize(64, 32, TRUE);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_clearPlane(WINDOW, TRUE);
    VDP_setBGBAddress(0xE000);                  /* plane B = plane A's map */
    cache_reset();
    VDP_setBackgroundColor(0);
    VDP_resetSprites();
    VDP_updateSprites(1, CPU);
    HUD_TILE = (u16)(BG_TILE + gfx->bg_ntiles);
    PLAYER_TILE = (u16)(HUD_TILE + gfx->hud_ntiles);
    SPR_TILE = (u16)(PLAYER_TILE + 16);
    PAL_setColors(0, gfx->palette, 64, CPU);
    hud_colours();
    shown_bg = shown_sp = 7;
    shown_cr = 0xC0;
    shown_c100 = 0;
    ss2_fade_bg = ss2_fade_sp = 7;
    VDP_loadTileData(gfx->bg_tiles, BG_TILE, gfx->bg_ntiles, CPU);
    VDP_loadTileData(gfx->hud_tiles, HUD_TILE, gfx->hud_ntiles, CPU);
    VDP_loadTileData(gfx->spr_tiles, SPR_TILE, gfx->spr_ntiles, CPU);
    VDP_loadTileData(gfx->bat_tiles + NGLYPH * 8, ICON_TILE, SS2_BAT_NKEYS - NGLYPH, CPU);
    glyphs_reset();
    memset(hud_last, 0, sizeof hud_last);
    ss2_bat_head = ss2_bat_tail = 0;
    VDP_setTileMapDataRectEx(WINDOW, gfx->hud_map, HUD_TILE, 0, 0, 32, HUD_ROWS, 32, CPU);
    VDP_setWindowOnTop(HUD_ROWS);
    VDP_setEnable(TRUE);
}

static void video_shutdown(void)
{
    VDP_setEnable(FALSE);
    VDP_setWindowOff();
    VDP_setEnable(TRUE);
    VDP_setBGBAddress(0xC000);                  /* SGDK reloads its font for SonSon */
    VDP_clearPlane(BG_B, TRUE);
    DMA_doVRamFill(0xD800, 0x800, 0, 1);        /* sprite cache blocks under the H40 window */
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(WINDOW, TRUE);
    VDP_resetSprites();
    VDP_updateSprites(1, CPU);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_A, 0);
    PAL_setColors(0, palette_black, 64, CPU);
    VDP_setEnable(TRUE);
}

/* ---- sprites: PCE SATB entries (ss2_sat) -> VDP sprites ---------------- */

static u8 shown_frame = 0xFF;
static u8 nspr;
u16 ss2_sprite_misses;

static void put_sprite(s16 x, s16 y, u8 size, u16 attr)
{
    if (nspr >= 64) return;
    VDP_setSpriteFull(nspr, x, y, size, attr, (u8)(nspr + 1));
    nspr++;
}

/* The player's cells (0x100-0x103) are streamed per frame on the PCE; here
 * the current frame's 16 tiles go to PLAYER_TILE when it changes. */
static void player_sprite(s16 x, s16 y, u16 attr)
{
    u16 ptr = (u16)((P_FRAMEH << 8) | P_FRAMEL);
    u8 frame = (u8)((u16)(ptr - 0xC000) / 0x180);
    if (frame < SS2_PLAYER_FRAMES && frame != shown_frame)
    {
        VDP_loadTileData(gfx->player_tiles + (u16)frame * 128, PLAYER_TILE, 16, DMA_QUEUE);
        shown_frame = frame;
    }
    put_sprite(x, y, SPRITE_SIZE(4, 4), TILE_ATTR_FULL(gfx->player_pal, 1, 0, (attr & 0x0800) ? 1 : 0, PLAYER_TILE));
}

/* ---- run-time sprite cells -------------------------------------------
 * The static tables (gfx->spr_lut) hold the cells the stage's own sprite
 * sets put in VRAM. Cells reloaded later (interiors $77A5, $77D0, the staff
 * $79F7) and palette 13 after a door's interior palette are converted on
 * demand from the HuCard pattern (ss2_cell_bank/src): a cell pair for one
 * palette -> 8 tiles (two 16x16 cells, column-major like the static ones)
 * in 8-tile blocks in VRAM freed by plane B sharing plane A's map. */
static u8 def_bank[0x200];
static u16 def_src[0x200];

typedef struct { u8 b0, b1, key; u8 used; u16 s0, s1, pair, tile, last; } CacheEnt;
static const u16 cache_base[] = {
    1728, 1736, 1744, 1752, 1760, 1768, 1776, 1784,          /* $D800-$DFFF */
    1968, 1976, 1984, 1992, 2000, 2008, 2016, 2024, 2032, 2040 };  /* $F600- */
#define NCACHE (sizeof cache_base / sizeof cache_base[0])
static CacheEnt cache[NCACHE];
static u16 cache_clock;
static u8 conv_n;
static u32 conv_buf[4][64];

static void cache_reset(void)
{
    memset(cache, 0, sizeof cache);
}

/* one 16x16 PCE cell (3 planes + zero plane, rows of 16 px) -> 4 Genesis
 * tiles TL BL TR BR through colour map cm */
static void conv_cell(u8 bank, u16 src, const u8 *cm, u32 *out)
{
    const u8 *p = ss2_banks[bank & 0x1F] + (src & 0x1FFF);
    u8 r, x;
    if ((src & 0x1FFF) > 0x2000 - 0x60) { memset(out, 0, 32 * 4); return; }
    for (r = 0; r < 16; r++)
    {
        u16 w0 = (u16)(p[r * 2] | (p[r * 2 + 1] << 8));
        u16 w1 = (u16)(p[32 + r * 2] | (p[33 + r * 2] << 8));
        u16 w2 = (u16)(p[64 + r * 2] | (p[65 + r * 2] << 8));
        u32 lo = 0, hi = 0;
        for (x = 0; x < 16; x++)
        {
            u8 b = (u8)(15 - x);
            u8 c = (u8)(((w0 >> b) & 1) | (((w1 >> b) & 1) << 1) | (((w2 >> b) & 1) << 2));
            u32 v = c ? (u32)(cm[c] & 15) : 0;
            if (x < 8) lo = (lo << 4) | v; else hi = (hi << 4) | v;
        }
        out[(r >> 3) * 8 + (r & 7)] = lo;           /* TL / BL */
        out[16 + (r >> 3) * 8 + (r & 7)] = hi;      /* TR / BR */
    }
}

/* tile of a dirty cell pair for palette key (0-15, 16+v = palette 13
 * interior variant v), 0 when it cannot be made this frame */
static u16 cache_tile(u16 c, u8 key, u8 *line)
{
    const u8 *cm = key >= 16 ? gfx->spr_cmap13 + (key - 16) * 8 : gfx->spr_cmap + key * 8;
    u8 b0 = ss2_cell_bank[c], b1 = ss2_cell_bank[c + 1];
    u16 s0 = ss2_cell_src[c], s1 = ss2_cell_src[c + 1];
    u8 i, victim = 0xFF;
    u16 age = 0;
    *line = (u8)(2 + (cm[1] >> 7));
    for (i = 0; i < NCACHE; i++)
    {
        CacheEnt *e = &cache[i];
        if (!e->used)
        {
            if (victim == 0xFF || cache[victim].used) victim = i;   /* prefer a free block */
            continue;
        }
        if (e->pair == c && e->key == key && e->b0 == b0 && e->b1 == b1 && e->s0 == s0 && e->s1 == s1)
        {
            e->last = cache_clock;
            return e->tile;
        }
        if (e->last != cache_clock && (victim == 0xFF || (cache[victim].used && (u16)(cache_clock - e->last) > age)))
        {
            victim = i;
            age = (u16)(cache_clock - e->last);
        }
    }
    if (victim == 0xFF || conv_n >= 4) return 0;
    conv_cell(b0, s0, cm, conv_buf[conv_n]);
    conv_cell(b1, s1, cm, conv_buf[conv_n] + 32);
    {
        CacheEnt *e = &cache[victim];
        e->used = 1; e->pair = c; e->key = key; e->b0 = b0; e->b1 = b1; e->s0 = s0; e->s1 = s1;
        e->tile = cache_base[victim];
        e->last = cache_clock;
        VDP_loadTileData(conv_buf[conv_n], e->tile, 8, DMA_QUEUE);
        conv_n++;
        return e->tile;
    }
}

static u8 cell_dirty(u16 c)
{
    return (u8)(ss2_cell_bank[c] != def_bank[c] || ss2_cell_src[c] != def_src[c]
             || ss2_cell_bank[c + 1] != def_bank[c + 1] || ss2_cell_src[c + 1] != def_src[c + 1]);
}

static void draw_sprites(void)
{
    u8 i, r;
    nspr = 0;
    if (!(R(0x2201) & 0x40))                    /* $2201 bit 6: sprites off */
    {
        put_sprite(-128, -128, SPRITE_SIZE(1, 1), 0);
        VDP_setSpriteLink(0, 0);
        VDP_updateSprites(1, DMA_QUEUE);
        return;
    }
    cache_clock++;
    conv_n = 0;
    for (i = 0; i < 64; i++)
    {
        const u16 *e = &ss2_sat[i * 4];
        s16 sx, sy;
        u16 attr = e[3], cell = (u16)((e[2] >> 1) & 0x3FF);
        u8 cw, ch, pal, line, key;
        if (!(e[0] & 0x3FF)) continue;
        sx = (s16)((e[1] & 0x3FF) - 32);
        sy = (s16)((e[0] & 0x3FF) - 64 - 8);
        cw = (attr & 0x0100) ? 2 : 1;
        ch = (u8)(((attr >> 12) & 3) == 0 ? 1 : (((attr >> 12) & 3) == 1 ? 2 : 4));
        if (sx <= -32 || sx >= 256 || sy <= -64 || sy >= 224) continue;
        if (cw == 2) cell &= (u16)~1;
        if (ch == 2) cell &= (u16)~2;
        if (ch == 4) cell &= (u16)~6;
        if (cell >= 0x100 && cell < 0x104)
        {
            player_sprite(sx, sy, attr);
            continue;
        }
        pal = (u8)(attr & 15);
        key = (pal == 13 && ss2_pal13_variant < 8) ? (u8)(16 + ss2_pal13_variant) : pal;
        for (r = 0; r < ch; r++)
        {
            u16 c = (u16)(cell + r * 2), t;
            s16 y;
            if (key >= 16 || cell_dirty((u16)(c & 0x1FE)))
            {
                t = cache_tile((u16)(c & 0x1FE), key, &line);
                if (!t) { ss2_sprite_misses++; continue; }
            }
            else
            {
                line = gfx->spr_line[pal];
                t = gfx->spr_lut[pal * 256 + (c >> 1)];
                if (!t || line > 3) { ss2_sprite_misses++; continue; }
                t = (u16)(SPR_TILE + t - 1);
            }
            y = (s16)(sy + ((attr & 0x8000) ? (ch - 1 - r) : r) * 16);
            if (cw == 2)
                put_sprite(sx, y, SPRITE_SIZE(4, 2), TILE_ATTR_FULL(line, 1, (attr & 0x8000) ? 1 : 0, (attr & 0x0800) ? 1 : 0, t));
            else
                put_sprite(sx, y, SPRITE_SIZE(2, 2), TILE_ATTR_FULL(line, 1, (attr & 0x8000) ? 1 : 0, (attr & 0x0800) ? 1 : 0, (u16)(t + (c & 1) * 4)));
        }
    }
    if (!nspr) put_sprite(-128, -128, SPRITE_SIZE(1, 1), 0);
    VDP_setSpriteLink((u16)(nspr - 1), 0);
    VDP_updateSprites(nspr, DMA_QUEUE);
}

/* ---- HUD (window rows 0-5 = PCE BAT rows 1-6) -------------------------- */

static u8 hud_drawn_life[2] = { 0xFF, 0xFF }, hud_drawn_magic[2] = { 0xFF, 0xFF };

/* bank 4 $BB99/$BBC2: n digits from RAM at (col, row) */
static void hud_digits(u16 src, u8 n, u16 col, u16 row)
{
    u8 i;
    for (i = 0; i < n; i++)
        VDP_setTileMapXY(WINDOW, (u16)(gfx->hud_digit[R(src + i) % 10] + HUD_TILE), (u16)(col + i), row);
}

/* bank 2 $76B3: bar of max/2 cells, shown/2 full cells, an odd half cell */
static void hud_bar(const u8 *v, u16 table, u16 row)
{
    u8 x = 0, max = (u8)(v[1] >> 1), full = (u8)(v[0] >> 1);
    for (; x < full; x++)
        VDP_setTileMapXY(WINDOW, (u16)(gfx->hud_bar[ss2_rom_byte((u16)(table + x)) & 63] + HUD_TILE), (u16)(7 + x), row);
    if (v[0] & 1)
    {
        VDP_setTileMapXY(WINDOW, (u16)(gfx->hud_bar[(ss2_rom_byte((u16)(table + x)) + 1) & 63] + HUD_TILE), (u16)(7 + x), row);
        x++;
    }
    for (; x < max; x++)
        VDP_setTileMapXY(WINDOW, (u16)(gfx->hud_bar[0x3C] + HUD_TILE), (u16)(7 + x), row);
}

static void draw_hud(u8 all)
{
    if (all || (ss2_hud_dirty & 1)) hud_digits(0x2205, 6, 7, 1);
    if (all || (ss2_hud_dirty & 2)) hud_digits(0x220E, 5, 20, 1);
    ss2_hud_dirty = 0;
    if (all || hud_drawn_life[0] != ss2_hud_life[0] || hud_drawn_life[1] != ss2_hud_life[1])
    {
        hud_bar(ss2_hud_life, 0x4DB9, 3);
        hud_drawn_life[0] = ss2_hud_life[0]; hud_drawn_life[1] = ss2_hud_life[1];
    }
    if (all || hud_drawn_magic[0] != ss2_hud_magic[0] || hud_drawn_magic[1] != ss2_hud_magic[1])
    {
        hud_bar(ss2_hud_magic, 0x4DC5, 4);
        hud_drawn_magic[0] = ss2_hud_magic[0]; hud_drawn_magic[1] = ss2_hud_magic[1];
    }
}

/* Genesis pad -> PCE $16 layout (L D R U Run Sel II I):
 * C = I (jump), B = II (attack), A = SELECT, START = RUN */
static u8 pce_pad(u16 p)
{
    u8 v = 0;
    if (p & BUTTON_LEFT) v |= 0x80;
    if (p & BUTTON_DOWN) v |= 0x40;
    if (p & BUTTON_RIGHT) v |= 0x20;
    if (p & BUTTON_UP) v |= 0x10;
    if (p & BUTTON_START) v |= 0x08;
    if (p & BUTTON_A) v |= 0x04;
    if (p & BUTTON_B) v |= 0x02;
    if (p & BUTTON_C) v |= 0x01;
    return v;
}

/* VDC IRQ $E16D-$E191: pop one request from the ring at $2285 (read index
 * $2295) and run the sound driver tick. During gameplay the raster-split
 * IRQ ($E122) reaches this block every frame unconditionally; only the
 * VBlank path ($E165) is gated by $6C|$6D|$C7, and $6C is nonzero while the
 * split is active, so the gate never decides gameplay frames. */
static void sound_irq(void)
{
    u8 x, id;
    x = R(0x2295);
    id = R(0x2285 + x);
    R(0x2285 + x) = 0xFF;
    if (id != 0xFF)
    {
        ss2_sound_request(id);
        R(0x2295) = (u8)((x + 1) & 0x0F);
    }
    ss2_sound_set_hold(ZP(0x6B));
    ss2_sound_tick();
}

static void camera_from_state(void)
{
    camx = (s16)((((Z_CAMX_HI & 7) << 8) | Z_CAMX_LO));
    camy = (s16)(((((Z_CAMY_HI & 7) << 8) | Z_CAMY_LO) + 61) & 2047);
}

#ifdef SS2_PROF
#define PROF(n) ss2_prof_mark(n)
#else
#define PROF(n)
#endif

#ifdef SS2_PROF
/* profiling build: worst frame load in scanlines (262 = one NTSC frame)
 * and frames whose work ran past the next VBlank, drawn on the HUD row */
static u16 prof_max, prof_over, prof_frames;
u16 ss2_prof_sec[8];          /* worst lines per section after warm-up */
static u32 prof_sum[8];
static u16 prof_n, prof_warm;
static u16 prof_last;
void ss2_prof_mark(u8 sec)
{
    u16 v = (u16)GET_VCOUNTER, d = (u16)(v >= prof_last ? v - prof_last : 0);
    if (prof_warm >= 120)
    {
        if (d > ss2_prof_sec[sec]) ss2_prof_sec[sec] = d;
        prof_sum[sec] += d;
    }
    prof_last = v;
}
static void prof_end(u32 t0, u16 v0)
{
    char buf[24];
    u16 v = (u16)GET_VCOUNTER, lines;
    if (prof_warm < 120) { prof_warm++; return; }
    prof_n++;
    if (vtimer != t0 || v >= 0xE0) { prof_over++; lines = 224; }
    else lines = (u16)(v - v0);
    if (lines > prof_max) prof_max = lines;
    if (++prof_frames >= 30)
    {
        sprintf(buf, "L%3u O%4u", prof_max, prof_over);
        VDP_drawTextBG(WINDOW, buf, 1, 0);
        {
            u8 k;
            for (k = 0; k < 8; k++)
            {
                sprintf(buf, "%u:%3u/%3u", k, (u16)(prof_sum[k] / (prof_n ? prof_n : 1)), ss2_prof_sec[k]);
                VDP_drawTextBG(WINDOW, buf, (u16)(1 + (k & 1) * 12), (u16)(1 + (k >> 1)));
            }
        }
        prof_frames = 0;
    }
}
#endif

/* GCC 16 compiles a plain copy loop (and the inlined LTO memcpy) into
 * move.b (a0)+,(0,a0,d1.l), which stores one byte too high on the 68000
 * (tools/check_autoinc_hazard.py). The empty asm keeps load and store as
 * separate instructions. */
static void load_bootstrap(void)
{
    const u32 *src = (const u32 *) gfx->ram;
    u32 *dst = (u32 *) ss2_ram;
    u16 i;
    for (i = 0; i < 0x2000 / 4; i++)
    {
        u32 v = src[i];
        __asm__ volatile ("" : "+d" (v));
        dst[i] = v;
    }
    ss2_mpr6 = 9;
    ss2_page[6] = ss2_banks[9];
}

void ss2_run(void)
{
#ifdef SS2_DBG_STAGE
    ss2_run_stage(SS2_DBG_STAGE);           /* diagnostic builds */
#else
    ss2_run_stage(0);
#endif
}

/* Start stage k (0-6) from its bootstrap state */
void ss2_run_stage(u8 k)
{
    u16 pad;

    gfx = &ss2_stage_gfx[k];
    load_bootstrap();
    /* the stage's own sprite sets ($774F) and staff ($79F7) give the cells
     * the static sprite tables were made from; replayed on a scratch copy
     * of the bootstrap RAM, which is then reloaded */
    memset(ss2_cell_bank, 0, sizeof ss2_cell_bank);
    memset(ss2_cell_src, 0, sizeof ss2_cell_src);
    ss2_sprset_stage();
    ss2_staff_art();
    memcpy(def_bank, ss2_cell_bank, sizeof def_bank);
    memcpy(def_src, ss2_cell_src, sizeof def_src);
    load_bootstrap();
#ifdef SS2_DBG_WX
    /* diagnostic builds (-D SS2_DBG_WX=.. -D SS2_DBG_WY=..): start with the
     * player at a world position, objects cleared (capture_vbl.py's
     * research injection) */
    {
        u16 cx = (u16)((SS2_DBG_WX - 0x70) & 0x7FF), cy = (u16)((SS2_DBG_WY - 0x80) & 0x7FF);
        u8 b, s;
        for (b = 0x45; b <= 0x4B; b = (u8)(b + 6))
        {
            ZP(b) = (u8)(cx >> 8); ZP(b + 1) = (u8)cx; ZP(b + 2) = 0;
            ZP(b + 3) = (u8)(cy >> 8); ZP(b + 4) = (u8)cy; ZP(b + 5) = 0;
        }
        R(0x2298) = 0; R(0x2299) = 0x70; R(0x229A) = 0;
        R(0x229B) = 0; R(0x229C) = 0x80; R(0x229D) = 0;
        for (s = 0; s < 0x20; s++) O(0x245A, s) = 0;
        s = 0;
        do { if (R(0x2B5A + s) != 0xFF) R(0x2B5A + s) = 0; } while (++s);
#ifdef SS2_DBG_LOCK7F
        ZP(0x7F) = 1;                       /* boss-room lock (boss door) */
#endif
    }
#endif
    ss2_mpr6 = 9;
    ss2_page[6] = ss2_banks[9];
    shown_frame = 0xFF;
    ss2_game_start();
    snd_shutdown();
    ss2_sound_init();
    ss2_sound_request(SS2_SND_MUS_STAGE(Z_STAGE));
    video_init();
    ss2_hud_life[0] = R(0x22BA); ss2_hud_life[1] = R(0x22B9);
    ss2_hud_magic[0] = R(0x22BD); ss2_hud_magic[1] = R(0x22BC);
    draw_hud(1);
    camera_from_state();
    redraw_view();
    apply_scroll();

    for (;;)
    {
#ifdef SS2_PROF
        u32 t0;
        u16 v0;
#endif
        SYS_doVBlankProcess();
#ifdef SS2_PROF
        /* measure inside active display, where the V counter is linear
         * (NTSC VBlank counts $E5-$EA twice) */
        while (GET_VCOUNTER != 0) { }
        t0 = vtimer;
        v0 = (u16)GET_VCOUNTER;
        prof_last = v0;
#endif
        pad = JOY_readJoypad(JOY_1);
#ifdef SS2_PROF
        /* fixed workload by logic tick (not wall time): walk right, attack
         * every 12 ticks, jump every 90; frozen after 1200 ticks */
        {
            static u16 tick;
            if (tick >= 1200) continue;
            pad = BUTTON_RIGHT;
            if ((tick % 12) < 3) pad |= BUTTON_B;
            if ((tick % 90) >= 30 && (tick % 90) < 39) pad |= BUTTON_C;
            tick++;
        }
#endif
        if ((pad & (BUTTON_START | BUTTON_A)) == (BUTTON_START | BUTTON_A))
            break;                              /* RUN+SELECT: the PCE reset combo */
        ss2_game_vblank(pce_pad(pad));
        PROF(0);
        sound_irq();
        PROF(0);
        ss2_game_frame();
        camera_from_state();
        if (ss2_redraw_request) { ss2_redraw_request = 0; redraw_view(); }   /* $EBE1 */
        else stream_view();
        bat_drain();
        apply_scroll();
        apply_fade();
        draw_sprites();
        draw_hud(0);
        PROF(7);
#ifdef SS2_PROF
        prof_end(t0, v0);
#endif
    }
    ss2_sound_shutdown();
    video_shutdown();
}

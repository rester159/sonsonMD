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
#include "ss2_shop_data.h"
#include "ss2_title_data.h"
#include "ss2_scene_data.h"
#include "ss2_ram.h"
#include "ss2_logic.h"
#include "ss2_task.h"
#include "ss2_sound.h"
#include "sound.h"

#if defined(SS2_PROF) || defined(SS2_TPROF)
void ss2_prof_mark(u8 sec);
#define PROF(n) ss2_prof_mark(n)
#else
#define PROF(n)
#endif
#ifdef SS2_TPROF
extern u16 ss2_tc_now[4];
#define TCOUNT(n) (ss2_tc_now[n]++)
#else
#define TCOUNT(n)
#endif

/* VRAM tile bases for the current stage (stage_gfx_load) */
#define BG_TILE      TILE_USER_INDEX
static u16 HUD_TILE, PLAYER_TILE, SPR_TILE;
static const Ss2StageGfx *gfx;
static void cache_reset(void);
static void spr_gen_bump(void);
static u8 pairs_ok;               /* pair_dirty[] is current (draw_sprites) */
/* cell sources of the stage's own sets (what the static sprite tables were
 * made from); the ending, which converts every sprite at run time, uses
 * the same RAM for more sprite cache entries (ss2_main.c cache) */
typedef struct { u8 b0, b1, key; u8 used; u16 s0, s1, pair, tile, last; } CacheEnt;
#define NEXTRA       64
static union
{
    struct { u8 bank[0x200]; u16 src[0x200]; } def;
    struct { CacheEnt e[NEXTRA]; u8 at[0x200]; } extra;    /* at[pair >> 1]: entry + 1 */
} ram_share;
#define def_bank (ram_share.def.bank)
#define def_src  (ram_share.def.src)
static void draw_hud(u8 all);
static void stage_gfx_load(u8 full);
static u8 shown_frame;
static u8 scene_shown;

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
/* $2201 bit 2 (raster split): on = HUD in the window plane over the
 * camera-scrolled playfield; off (title) = the whole BAT from (0,0), PCE
 * line g + 8 on Genesis line g */
static u8 split_shown = 1;

/* The ending picture and the text below it (BAT rows 0-63, 32 wide) use a
 * 32x64 plane; everything else a 64x32 ring. */
static u8 tall_plane;

static void plane_cell(u16 w, u16 x, u16 y)
{
    if (tall_plane) VDP_setTileMapXY(BG_A, w, (u16)(x & 31), (u16)(y & 63));
    else VDP_setTileMapXY(BG_A, w, (u16)(x & 63), (u16)(y & 31));
}

/* The ending's text band (bank 4 $B174: RCR $E0, PCE line 160 = Genesis
 * line 152): an H interrupt there switches plane A/B to the band's scroll,
 * the VBlank callback puts the picture's back. Both touch the VDP only
 * while the main loop waits for VBlank or runs the game logic (band_idle),
 * so they never break into its port accesses; a frame whose presentation
 * overruns line 152 shows no band. */
static volatile u8 band_on, band_idle;
static volatile u16 band_v, top_v;

/* plane A then plane B (VSRAM $00, $02): one command each, whatever the
 * auto-increment was left at */
static void vsram_ab(u16 v)
{
    *(vu32 *)VDP_CTRL_PORT = 0x40000010;
    *(vu16 *)VDP_DATA_PORT = v;
    *(vu32 *)VDP_CTRL_PORT = 0x40020010;
    *(vu16 *)VDP_DATA_PORT = v;
}

u16 ss2_band_hits, ss2_band_busy, ss2_loop_n;   /* diagnostics: H-ints applied / skipped, frames */

static HINTERRUPT_CALLBACK band_hint(void)
{
    if (!band_on) return;
    if (band_idle) { vsram_ab(band_v); ss2_band_hits++; }
    else ss2_band_busy++;
}

static void band_vblank(void)
{
    if (band_on) vsram_ab(top_v);
}

#ifdef SS2_TPROF_PC
/* PC sampling (-D SS2_TPROF -D SS2_TPROF_PC): an H-int every 4 lines
 * stores the interrupted PC in a 128-entry ring that the harness drains
 * each refresh (SS2_TRACE_PCRING). Replaces the raster band. */
__attribute__((used, externally_visible)) u32 ss2_pc_ring[128];
__attribute__((used, externally_visible)) volatile u16 ss2_pc_i;
void tp_hint(void);
__asm__(".globl tp_hint\n"
        "tp_hint:\n"
        "    movem.l %d0/%a0,-(%sp)\n"         /* SR at 8(sp), PC at 10(sp) */
        "    move.w ss2_pc_i,%d0\n"
        "    lea ss2_pc_ring,%a0\n"
        "    move.l 10(%sp),(%a0,%d0.w)\n"
        "    addq.w #4,%d0\n"
        "    and.w #0x1FC,%d0\n"
        "    move.w %d0,ss2_pc_i\n"
        "    movem.l (%sp)+,%d0/%a0\n"
        "    rte\n");
#endif

static void band_set(u8 on)
{
#ifdef SS2_TPROF_PC
    (void)on;
    SYS_setHIntCallback(tp_hint);
    VDP_setHIntCounter(3);
    VDP_setHInterrupt(TRUE);
    return;
#endif
    if (on == band_on) return;
    band_on = on;
    if (on)
    {
        SYS_setHIntCallback(band_hint);
        VDP_setHIntCounter(151);
        SYS_setVBlankCallback(band_vblank);
        VDP_setHInterrupt(TRUE);
    }
    else
    {
        VDP_setHInterrupt(FALSE);
        SYS_setVBlankCallback(NULL);
        SYS_setHIntCallback(NULL);
    }
}

/* scene ENDING after the tally: no HUD; the picture scrolls with the
 * camera ($6E set: $45/$46, $48/$49), the band with $51/$52, $54/$55 */
static void ending_scroll(void)
{
    u16 x = 0, y = 0;
    if (split_shown) { split_shown = 0; VDP_setWindowOff(); }
    if (ZP(0x6E)) { x = (u16)(((ZP(0x45) << 8) | ZP(0x46)) & 0x1FF); y = (u16)(((ZP(0x48) << 8) | ZP(0x49)) & 0x1FF); }
    top_v = (u16)(y + 8);
    VDP_setHorizontalScroll(BG_A, (s16)-x);
    VDP_setHorizontalScroll(BG_B, (s16)-x);
    VDP_setVerticalScroll(BG_A, (s16)top_v);
    VDP_setVerticalScroll(BG_B, (s16)top_v);
    band_v = (u16)((((ZP(0x54) << 8) | ZP(0x55)) - 152) & 0x1FF);
    band_set((u8)(ss2_ending_part == 2 && (R(0x2201) & 0x04)));
}

/* Both planes' scroll, applied in the next VBlank with the sprite table
 * and palette (DMA queue). Written at once, the registers changed wherever
 * the beam was when the frame's work ended (usually mid-screen): a tear,
 * and sprites a frame apart from the background. */
static void scroll_queue(s16 h, s16 v)
{
    /* plane A then B: adjacent words in the H-scroll table and in VSRAM,
     * so one transfer each (queue entries are not cheap); read by the DMA
     * at the VBlank, a RAM buffer that crosses no 128 KiB bank */
    static s16 hbuf[2], vbuf[2];
    hbuf[0] = hbuf[1] = h;
    vbuf[0] = vbuf[1] = v;
    DMA_queueDmaFast(DMA_VRAM, hbuf, VDP_HSCROLL_TABLE, 2, 2);
    DMA_queueDmaFast(DMA_VSRAM, vbuf, 0, 2, 2);
}

static void apply_scroll(void)
{
    u8 split = (u8)((R(0x2201) & 0x04) != 0);
    if (scene_shown == SS2_SCENE_ENDING) { ending_scroll(); return; }
    if (split != split_shown)
    {
        split_shown = split;
        if (split) VDP_setWindowOnTop(HUD_ROWS);
        else VDP_setWindowOff();
    }
    if (!split) scroll_queue(0, 8);
    else scroll_queue((s16)-camx, (s16)(camy - VIEW_TOP));
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
#define END_FONT_TILE (GLYPH_TILE + 32)  /* the ending's credits font (glyph slots 32-63) */
#define NGLYPH       0x200          /* glyph keys: $1180-$123F, $51A0-$523F, $F1A0-$F23F */
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
    if (ss2_scene == SS2_SCENE_TITLE)                       /* title scene words */
    {
        u16 lo = 0, hi = SS2_TITLE_NKEYS;
        while (lo < hi)
        {
            u16 mid = (u16)((lo + hi) >> 1);
            if (ss2_title_keys[mid] < w) lo = (u16)(mid + 1); else hi = mid;
        }
        if (lo < SS2_TITLE_NKEYS && ss2_title_keys[lo] == w)
            return TILE_ATTR_FULL(ss2_title_line[lo], 0, 0, 0, BG_TILE + lo);
        return 0;
    }
    if (ss2_scene >= SS2_SCENE_BG_FIRST)                    /* picture scenes */
    {
        const Ss2SceneBg *sc = &ss2_scene_bg[ss2_scene - SS2_SCENE_BG_FIRST];
        u16 lo = 0, hi = sc->nkeys;
        while (lo < hi)
        {
            u16 mid = (u16)((lo + hi) >> 1);
            if (sc->keys[mid] < w) lo = (u16)(mid + 1); else hi = mid;
        }
        if (lo < sc->nkeys && sc->keys[lo] == w)
            return TILE_ATTR_FULL(sc->line[lo], 0, 0, 0, BG_TILE + lo);
    }
    if (ss2_ending_part == 3 && w >= 0x51C0 && w < 0x51E0)  /* the credits font */
        return TILE_ATTR_FULL(ss2_end_font_line, 1, 0, 0, END_FONT_TILE + w - 0x51C0);
    if (ss2_shop_screen && w >= 0xF1B0 && w <= 0xF1B9)      /* shop price digits */
        return (u16)(ss2_shop_digit[w - 0xF1B0] + BG_TILE);
    if (w >= 0x1180 && w < 0x1240) k = (u16)(w - 0x1180);
    else if (w >= 0x51A0 && w < 0x5240) k = (u16)(0xC0 + w - 0x51A0);
    else if (w >= 0xF1A0 && w < 0xF240) k = (u16)(0x160 + w - 0xF1A0);
    else k = 0xFFFF;
    if (k != 0xFFFF)
    {
        if (!glyph_slot[k])
        {
            if (glyph_next >= (scene_shown == SS2_SCENE_ENDING ? END_FONT_TILE - GLYPH_TILE : GLYPH_SLOTS))
                glyph_next = 0;                                 /* full: reuse */
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

/* Genesis wording: the START button, where the PCE says RUN */
static const u8 title_text_gen[] = {
    0x06, 0x05, 0xF0, 'P','U','S','H',' ','S','T','A','R','T',' ','B','U','T','T','O','N', '/', '/',
    ' ',' ','H','I','S','C','O','R','E','/','/','/',
    ' ','@',' ','1','9','8','9',' ','N','E','C',' ','A','V','E','N','U','E','/','/',
    ' ','L','I','C','E','N','S','E','D',' ','B','Y',' ','C','A','P','C','O','M', 0x5C };

static void bat_cell(u16 a, u16 w)
{
    plane_cell(bat_word(w), (u16)(a & 63), (u16)(a >> 6));
}

/* $BB27 text table (BAT address, palette, characters, '/' next line, '\'
 * end) at BAT base + its address */
static void bat_text(u16 ptr, u16 base)
{
    u16 a = (u16)(base + (ss2_rom_byte(ptr) | (ss2_rom_byte((u16)(ptr + 1)) << 8)));
    u16 hi = ss2_rom_byte((u16)(ptr + 2)), line = a;
    const u8 *gen = ptr == 0xE7F7 ? title_text_gen : 0;
    u16 i = 3;
    for (;;)
    {
        u8 c = gen ? gen[i] : ss2_rom_byte((u16)(ptr + i));
        u16 lo = (u16)(c + 0x80);
        i++;
        if (c == 0x5C) return;
        if (c == 0x2F) { line = (u16)(line + 0x40); a = line; continue; }
        bat_cell(a, (u16)((u8)(hi + 1 + (lo >> 8)) << 8 | (lo & 0xFF)));   /* $BB62 */
        a++;
    }
}

/* $FBB1 picture: per source row two BAT rows (block tops, then +$10) */
static void bat_picture(u16 dst, u16 src, u8 w, u8 h)
{
    u8 r, y;
    for (r = 0; r < h; r++)
    {
        u16 a = (u16)(dst + r * 0x40), s = (u16)(src + (r >> 1) * w);
        for (y = 0; y < w; y = (u8)(y + 2))
        {
            u8 b0 = ss2_rom_byte((u16)(s + y)), b1 = ss2_rom_byte((u16)(s + y + 1));
            if (b0 == 0xFF) { bat_cell(a, 0x01A0); bat_cell((u16)(a + 1), 0x01A0); }
            else
            {
                u16 lo = (u16)(b0 + 0x80);
                u16 wd = (u16)((((2 + (lo >> 8)) | b1) << 8) | ((lo & 0xFF) | ((r & 1) << 4)));
                bat_cell(a, wd);
                bat_cell((u16)(a + 1), (u16)(wd + 1));
            }
            a = (u16)(a + 2);
        }
    }
}

/* bank 4 $A4CA box: 8 rows of (character, count) runs, $FF ends a row;
 * palette 15 font words */
static void bat_box(u16 dst, u16 src)
{
    u8 r;
    for (r = 0; r < 8; r++, dst = (u16)(dst + 0x40))
    {
        u16 a = dst;
        u8 c;
        while ((c = ss2_rom_byte(src++)) != 0xFF)
        {
            u8 n = ss2_rom_byte(src++);
            u16 lo = (u16)(c + 0x80);
            u16 w = (u16)(((0xF1 + (lo >> 8)) << 8) | (lo & 0xFF));
            while (n--) bat_cell(a++, w);
        }
    }
}

static u8 map_live = 1;                         /* stage map drawn: streaming valid */

static struct { u16 a, ptr; } hud_last[16];   /* table shown per BAT address */

static void bat_drain(void)
{
    while (ss2_bat_tail != ss2_bat_head)
    {
        const Ss2BatOp *o = &ss2_bat_queue[ss2_bat_tail & (SS2_BATQ - 1)];
        ss2_bat_tail++;
        if (o->kind == SS2_BAT_PUT)
            plane_cell(bat_word(o->w), o->x, o->y);
        else if (o->kind == SS2_BAT_RESTORE && scene_shown == SS2_SCENE_ENDING) { }   /* the picture is redrawn */
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
        else if (o->kind == SS2_BAT_CLEAR)
        {
            VDP_clearPlane(BG_A, TRUE);
            map_live = 0;
            glyphs_reset();
        }
        else if (o->kind == SS2_BAT_PICTURE)
            bat_picture(o->w, o->w2, o->x, o->y);
        else if (o->kind == SS2_BAT_TEXT)
            bat_text(o->w, o->w2);
        else if (o->kind == SS2_BAT_BOX)
            bat_box(o->w, o->w2);
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
static u8 shop_shown;                       /* shop scene drawn (shop_scene) */
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

/* the credits font (bank 4 $B2C1/$B387): glyphs A-Z ($1C1-$1DA) drawn
 * ss2_end_font_dx pixels off their cells' left edge, cut at the edges.
 * Written a tile at a time from the stack: work RAM above .bss is SGDK's
 * heap, which the front end's splashes need (a 1 KiB static buffer here
 * left them too little to boot). */
static void end_font(void)
{
    static s8 shown_dx = 0x7F;
    u32 t[8];
    u8 k, r;
    s8 dx = ss2_end_font_dx;
    if (ss2_ending_part != 3) { shown_dx = 0x7F; return; }
    if (dx == shown_dx) return;
    shown_dx = dx;
    for (k = 0; k < 32; k++)
    {
        for (r = 0; r < 8; r++)
        {
            u32 v = ss2_end_font[k * 8 + r];
            if (k >= 1 && k <= 0x1A)
                v = dx <= -8 || dx >= 8 ? 0 : dx < 0 ? v << (4 * -dx) : v >> (4 * dx);
            t[r] = v;
        }
        VDP_loadTileData(t, (u16)(END_FONT_TILE + k), 1, CPU);
    }
}

/* the title scene's tiles over the stage BG tiles (ss2_scene), and a stage
 * started by the game flow (ss2_stage_request: its sprite sets are the
 * defaults the static sprite tables were made from) */
static void scene_update(void)
{
    if (ss2_stage_request)
    {
        ss2_stage_request = 0;
        gfx = &ss2_stage_gfx[Z_STAGE < SS2_NSTAGES ? Z_STAGE : 0];
        memcpy(def_bank, ss2_cell_bank, sizeof def_bank);
        memcpy(def_src, ss2_cell_src, sizeof def_src);
        pairs_ok = 0;
        stage_gfx_load(0);
        draw_hud(1);
        shown_bg = 0xFF;
    }
    {
        static u8 intro_shown;
        if (ss2_pal_intro != intro_shown) { intro_shown = ss2_pal_intro; cache_reset(); }
    }
    end_font();
    if (ss2_scene == scene_shown) return;
    scene_shown = ss2_scene;
    if ((scene_shown == SS2_SCENE_ENDING) != tall_plane)
    {
        tall_plane = (u8)(scene_shown == SS2_SCENE_ENDING);
        VDP_setPlaneSize(tall_plane ? 32 : 64, tall_plane ? 64 : 32, FALSE);
        VDP_clearPlane(BG_A, TRUE);
        glyphs_reset();
    }
    if (scene_shown == SS2_SCENE_TITLE)
        VDP_loadTileData(ss2_title_tiles, BG_TILE, SS2_TITLE_NKEYS, CPU);
    else if (scene_shown >= SS2_SCENE_BG_FIRST && ss2_scene_bg[scene_shown - SS2_SCENE_BG_FIRST].nkeys)
    {
        const Ss2SceneBg *sc = &ss2_scene_bg[scene_shown - SS2_SCENE_BG_FIRST];
        VDP_loadTileData(sc->tiles, BG_TILE, sc->nkeys, CPU);
    }
    else
        VDP_loadTileData(gfx->bg_tiles, BG_TILE, gfx->bg_ntiles, CPU);
    cache_reset();
    shown_bg = 0xFF;
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
        const Ss2SceneBg *sc = scene_shown >= SS2_SCENE_BG_FIRST ? &ss2_scene_bg[scene_shown - SS2_SCENE_BG_FIRST] : 0;
        u8 pic = (u8)(sc && sc->nkeys);
        u8 title = (u8)(scene_shown == SS2_SCENE_TITLE || (sc && sc->spr_palette));   /* own sprite lines */
        u16 w = (i < 32 && shop_shown) ? ss2_shop_palette[i] : scene_shown == SS2_SCENE_TITLE ? ss2_title_palette[i]
              : (i < 32 && pic) ? sc->palette[i] : (sc && sc->spr_palette) ? sc->spr_palette[i - 32] : gfx->palette[i];
        if (i < 32) w = (cr & 0x80) ? fade_colour(w, shown_bg) : c100;
        else if (title || !(hud_keep[(i >> 4) - 2] & (1 << (i & 15)))) w = fade_colour(w, shown_sp);
        fade_pal[i] = w;
    }
    if (!(cr & 0x80)) fade_pal[0] = c100;           /* backdrop */
    PAL_setColors(0, fade_pal, 64, DMA_QUEUE);
}

/* ---- the shop scene (bank 3 $9E7B): its picture replaces the stage BG
 * (tiles and CRAM lines 0-1) while ss2_shop_screen is set; the camera is
 * at 0, so world tiles are the PCE BAT cells (tools/ss2/gen_shop.py) */
static void shop_scene(void)
{
    u16 r, c;
    if (ss2_shop_screen == shop_shown) return;
    shop_shown = ss2_shop_screen;
    if (shop_shown)
    {
        VDP_loadTileData(ss2_shop_tiles, BG_TILE, SS2_SHOP_NTILES, CPU);
        for (r = 0; r < SS2_SHOP_ROWS; r++)
        {
            for (c = 0; c < 32; c++)
                linebuf[c] = (u16)(ss2_shop_map[r * 32 + c] + BG_TILE);
            VDP_setTileMapDataRow(BG_A, linebuf, (u16)((SS2_SHOP_ROW0 + r) & 31), 0, 32, CPU);
        }
    }
    else
        VDP_loadTileData(gfx->bg_tiles, BG_TILE, gfx->bg_ntiles, CPU);
    shown_bg = 0xFF;                            /* palette lines 0-1 change */
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
    stage_gfx_load(1);
    ss2_bat_head = ss2_bat_tail = 0;
    VDP_setWindowOnTop(HUD_ROWS);
    split_shown = 1;
    VDP_setEnable(TRUE);
}

/* the current stage's graphics: tiles, palette, HUD, icons (also when the
 * game flow starts a stage: ss2_stage_request). full: power on, the
 * palette at full brightness; a stage the game flow starts keeps the
 * brightness its logic left (the intro requests stage 1 after the title has
 * faded to black and fades it in later: the HuCard never shows it bright
 * before that), and apply_fade() uploads it at the frame's end. */
static void stage_gfx_load(u8 full)
{
    HUD_TILE = (u16)(BG_TILE + gfx->bg_ntiles);
    PLAYER_TILE = (u16)(HUD_TILE + gfx->hud_ntiles);
    SPR_TILE = (u16)(PLAYER_TILE + 16);
    if (full)
    {
        PAL_setColors(0, gfx->palette, 64, CPU);
        ss2_fade_bg = ss2_fade_sp = 7;
        shown_bg = shown_sp = 7;
    }
    else shown_bg = shown_sp = 0xFF;
    spr_gen_bump();
    hud_colours();
    shown_cr = 0xC0;
    shown_c100 = 0;
    shop_shown = 0;
    ss2_shop_screen = 0;
    VDP_loadTileData(gfx->bg_tiles, BG_TILE, gfx->bg_ntiles, CPU);
    VDP_loadTileData(gfx->hud_tiles, HUD_TILE, gfx->hud_ntiles, CPU);
    VDP_loadTileData(gfx->spr_tiles, SPR_TILE, gfx->spr_ntiles, CPU);
    VDP_loadTileData(gfx->bat_tiles + NGLYPH * 8, ICON_TILE, SS2_BAT_NKEYS - NGLYPH, CPU);
    glyphs_reset();
    memset(hud_last, 0, sizeof hud_last);
    VDP_setTileMapDataRectEx(WINDOW, gfx->hud_map, HUD_TILE, 0, 0, 32, HUD_ROWS, 32, CPU);
    cache_reset();
    shown_frame = 0xFF;
    scene_shown = SS2_SCENE_PLAY;
}

static void video_shutdown(void)
{
    band_set(0);
    VDP_setEnable(FALSE);
    if (tall_plane) { tall_plane = 0; VDP_setPlaneSize(64, 32, FALSE); }
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

static const u16 cache_base[] = {
    1728, 1736, 1744, 1752, 1760, 1768, 1776, 1784,          /* $D800-$DFFF */
    1968, 1976, 1984, 1992, 2000, 2008, 2016, 2024, 2032, 2040 };  /* $F600- */
#define NCACHE (sizeof cache_base / sizeof cache_base[0])
static CacheEnt cache[NCACHE];
static u16 cache_epoch;         /* moves whenever an entry is (re)assigned */
static u8 cache_idx;            /* entry cache_tile() last returned */
static u8 cache_at[0x100];      /* cache_at[pair >> 1]: entry + 1 (the ending: extra.at) */
static u16 cache_clock;
static u8 conv_n;
static u32 conv_buf[4][64];

/* the ending: NEXTRA more blocks at tiles 640-1151 (its scene tiles end
 * below 640, the stage's are not shown), found through a pair index: a
 * linear search of every entry for every sprite cell costs more than a
 * frame */
#define EXTRA_TILE   640
#define ending_cache() (scene_shown == SS2_SCENE_ENDING)
#define ncache()     (ending_cache() ? NCACHE + NEXTRA : NCACHE)
#define cache_ent(i) ((i) < NCACHE ? &cache[i] : &ram_share.extra.e[(i) - NCACHE])

static void memo_reset(void);

static void cache_reset(void)
{
    cache_epoch++;
    pairs_ok = 0;
    spr_gen_bump();
    memset(cache, 0, sizeof cache);
    memo_reset();
    if (ending_cache()) memset(&ram_share.extra, 0, sizeof ram_share.extra);
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

/* a scene's own sprite colour map (title, ending picture), else 0 */
static const u8 *scene_cmap(void)
{
    if (scene_shown == SS2_SCENE_TITLE) return ss2_title_cmap;
    if (scene_shown >= SS2_SCENE_BG_FIRST) return ss2_scene_bg[scene_shown - SS2_SCENE_BG_FIRST].cmap;
    return 0;
}

/* tile of a dirty cell pair for palette key (0-15, 16+v = palette 13
 * interior variant v), 0 when it cannot be made this frame */
static u16 cache_tile(u16 c, u8 key, u8 *line)
{
    const u8 *scm = scene_cmap();
    const u8 *cm = scm ? scm + (key & 15) * 8
                 : key >= 16 ? gfx->spr_cmap13 + (key - 16) * 8
                 : (ss2_pal_intro && key >= 8) ? ss2_intro_cmap + ((gfx - ss2_stage_gfx) * 8 + key - 8) * 8
                 : gfx->spr_cmap + key * 8;
    u8 b0 = ss2_cell_bank[c], b1 = ss2_cell_bank[c + 1];
    u16 s0 = ss2_cell_src[c], s1 = ss2_cell_src[c + 1];
    u8 i, victim = 0xFF, n = ncache();
    u16 age = 0;
    TCOUNT(0);
    *line = (u8)(2 + (cm[1] >> 7));
    /* the pair's last converted entry first: a hit returns the one entry
     * the search below would find (an entry is only added after a full
     * miss), without visiting the others */
    i = ending_cache() ? ram_share.extra.at[c >> 1] : cache_at[c >> 1];
    if (i)
    {
        CacheEnt *e = cache_ent(i - 1);
        if (e->used && e->pair == c && e->key == key && e->b0 == b0 && e->b1 == b1 && e->s0 == s0 && e->s1 == s1)
        {
            e->last = cache_clock;
            TCOUNT(1);
            cache_idx = (u8)(i - 1);
            return e->tile;
        }
    }
    for (i = 0; i < n; i++)
    {
        CacheEnt *e = cache_ent(i);
        if (!e->used)
        {
            if (victim == 0xFF || cache_ent(victim)->used) victim = i;   /* prefer a free block */
            continue;
        }
        if (e->pair == c && e->key == key && e->b0 == b0 && e->b1 == b1 && e->s0 == s0 && e->s1 == s1)
        {
            e->last = cache_clock;
            TCOUNT(1);
            cache_idx = i;
            return e->tile;
        }
        if (e->last != cache_clock && (victim == 0xFF || (cache_ent(victim)->used && (u16)(cache_clock - e->last) > age)))
        {
            victim = i;
            age = (u16)(cache_clock - e->last);
        }
    }
    if (victim == 0xFF || conv_n >= 4) return 0;
    memo_reset();
    PROF(12);
    conv_cell(b0, s0, cm, conv_buf[conv_n]);
    conv_cell(b1, s1, cm, conv_buf[conv_n] + 32);
    PROF(13);
    {
        CacheEnt *e = cache_ent(victim);
        cache_epoch++;
        cache_idx = victim;
        e->used = 1; e->pair = c; e->key = key; e->b0 = b0; e->b1 = b1; e->s0 = s0; e->s1 = s1;
        e->tile = victim < NCACHE ? cache_base[victim] : (u16)(EXTRA_TILE + (victim - NCACHE) * 8);
        if (ending_cache()) ram_share.extra.at[c >> 1] = (u8)(victim + 1);
        else cache_at[c >> 1] = (u8)(victim + 1);
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

/* cell_dirty() per pair, rebuilt when the cell or default tables may have
 * changed: a sprite-set load (ss2_cells_dirty), new defaults, a cache reset,
 * and the ending (whose cache shares the defaults' RAM) */
static u8 pair_dirty[0x100];

/* Per SAT entry: the VDP rows its pattern/attribute words gave last time
 * (tile|attributes, size, y offset; cidx = the cache entry of a converted
 * row, refreshed on replay as a cache hit would, 0xFF for a static row);
 * valid while cache_epoch has not moved either. Reused while
 * the words and spr_gen are unchanged; spr_gen moves whenever an input of
 * that mapping may change (stage graphics, defaults/dirty pairs, cache
 * reset, scene colour map, intro palettes, palette 13 variant). */
typedef struct { u32 key; u16 gen, cepoch; u8 n; u8 size[4]; u8 yoff[4]; u8 cidx[4]; u16 attr[4]; } SprMemo;
static SprMemo spr_memo[64];
static u16 spr_gen = 1;
static u8 spr_nomemo;               /* SS2_SPRCHECK: the full path, for comparison */
#ifdef SS2_SPRCHECK
__attribute__((used, externally_visible)) volatile u32 ss2_sprchk[3];   /* passes, mismatches, skipped (conversions) */
#endif
static void spr_gen_bump(void)
{
    if (!++spr_gen) { memset(spr_memo, 0, sizeof spr_memo); spr_gen = 1; }
}

static void pairs_update(void)
{
    u16 c;
    if (ss2_cells_dirty) { ss2_cells_dirty = 0; pairs_ok = 0; }
    if (pairs_ok) return;
    for (c = 0; c < 0x200; c += 2) pair_dirty[c >> 1] = cell_dirty(c);
    pairs_ok = 1;
    spr_gen_bump();
}

/* the ending's sprites per SAT slot: the pattern/attribute words they were
 * resolved for and the tile (line << 14 | tile) of each 16-px row, reused
 * while the words stay (cleared whenever the cache converts, which may
 * evict a remembered block) */
static u32 memo_words[64];
static u16 memo_tile[64][4];

static void memo_reset(void)
{
    memset(memo_words, 0, sizeof memo_words);
}

/* the ending's cell: its pair-index entry when that still holds it (no
 * colour map lookups, no search), else cache_tile */
static u16 ending_tile(u16 cc, u8 key, u8 *line)
{
    u8 at = ram_share.extra.at[cc >> 1];
    if (at)
    {
        CacheEnt *e = cache_ent(at - 1);
        if (e->pair == cc && e->key == key && e->used && e->b0 == ss2_cell_bank[cc] && e->s0 == ss2_cell_src[cc]
            && e->b1 == ss2_cell_bank[cc + 1] && e->s1 == ss2_cell_src[cc + 1])
        {
            e->last = cache_clock;
            *line = (u8)(2 + (ss2_scene_bg[SS2_SCENE_ENDING - SS2_SCENE_BG_FIRST].cmap[key * 8 + 1] >> 7));
            return e->tile;
        }
    }
    return cache_tile(cc, key, line);
}

static void draw_sprites(void)
{
    u8 i, r;
    u8 ending = (u8)ending_cache();
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
    /* per-frame invariants of the static-table test below */
    const u8 *spr_line = gfx->spr_line;
    const u16 *spr_lut = gfx->spr_lut;
    u8 no_static = (u8)(scene_cmap() != 0), intro = (u8)(ss2_pal_intro != 0);
    u8 hi = ss2_sat_hi, last = 0;
    {
        static u8 m_static = 0xFF, m_intro = 0xFF, m_p13 = 0xFE;
        if (no_static != m_static || intro != m_intro || ss2_pal13_variant != m_p13)
        {
            m_static = no_static; m_intro = intro; m_p13 = ss2_pal13_variant;
            spr_gen_bump();
        }
    }
    if (ending) pairs_ok = 0;
    else pairs_update();
    for (i = 0; i < hi; i++)
    {
        const u16 *e = &ss2_sat[i * 4];
        s16 sx, sy;
        u16 attr = e[3], cell = (u16)((e[2] >> 1) & 0x3FF);
        u8 cw, ch, pal, line, key, memo, ok = 1, rec, ci;
        SprMemo *mm = 0;
        if (!(e[0] & 0x3FF)) continue;
        last = (u8)(i + 1);
        memo = (u8)(ending && memo_words[i] == (((u32)e[2] << 16) | attr));
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
        if (!ending && !spr_nomemo)
        {
            mm = &spr_memo[i];
            if (mm->gen == spr_gen && mm->cepoch == cache_epoch && mm->key == (((u32)e[2] << 16) | attr))
            {
                for (r = 0; r < mm->n; r++)
                {
                    if (mm->cidx[r] != 0xFF) cache_ent(mm->cidx[r])->last = cache_clock;
                    put_sprite(sx, (s16)(sy + mm->yoff[r]), mm->size[r], mm->attr[r]);
                }
                continue;
            }
            mm->gen = 0;
            mm->n = 0;
            rec = 1;
        }
        else rec = 0;
        pal = (u8)(attr & 15);
        key = (pal == 13 && ss2_pal13_variant < 8) ? (u8)(16 + ss2_pal13_variant) : pal;
        for (r = 0; r < ch; r++)
        {
            u16 c = (u16)(cell + r * 2), t;
            s16 y;
            u16 cc = (u16)(c & 0x3FE);
            if (ending)
            {
                if (memo)
                {
                    t = memo_tile[i][r];
                    if (!t) continue;
                    line = (u8)(t >> 14);
                    t &= 0x3FFF;
                    goto put;
                }
                if (cc >= 0x200 || !ss2_cell_bank[cc] || !(t = ending_tile(cc, (u8)(attr & 15), &line)))
                {
                    ss2_sprite_misses++;
                    ok = 0;                 /* (a conversion may come next frame) */
                    continue;
                }
                memo_tile[i][r] = (u16)((line << 14) | t);
                goto put;
            }
            line = spr_line[pal];
            t = (cc >= 0x200 || key >= 16 || no_static || (intro && key >= 8)
                 || pair_dirty[cc >> 1]) ? 0
                : spr_lut[pal * 256 + (cc >> 1)];
            ci = 0xFF;
            if (t && line <= 3)
                t = (u16)(SPR_TILE + t - 1);
            else
            {
                /* reloaded cells, interior palettes, and cells the static
                 * tables never saw (magic, shop): convert from the HuCard */
                if (cc >= 0x200 || !ss2_cell_bank[cc]) { ss2_sprite_misses++; rec = 0; continue; }
                t = cache_tile(cc, key, &line);
                if (!t) { ss2_sprite_misses++; rec = 0; continue; }
                ci = cache_idx;
            }
put:
            {
                u8 yo = (u8)(((attr & 0x8000) ? (ch - 1 - r) : r) * 16);
                u8 sz = cw == 2 ? SPRITE_SIZE(4, 2) : SPRITE_SIZE(2, 2);
                u16 ta = cw == 2
                    ? TILE_ATTR_FULL(line, 1, (attr & 0x8000) ? 1 : 0, (attr & 0x0800) ? 1 : 0, t)
                    : TILE_ATTR_FULL(line, 1, (attr & 0x8000) ? 1 : 0, (attr & 0x0800) ? 1 : 0, (u16)(t + (c & 1) * 4));
                y = (s16)(sy + yo);
                put_sprite(sx, y, sz, ta);
                if (rec) { mm->yoff[mm->n] = yo; mm->size[mm->n] = sz; mm->attr[mm->n] = ta; mm->cidx[mm->n] = ci; mm->n++; }
            }
        }
        if (rec) { mm->key = ((u32)e[2] << 16) | attr; mm->gen = spr_gen; mm->cepoch = cache_epoch; }
        if (ending && !memo) memo_words[i] = ok ? (((u32)e[2] << 16) | attr) : 0;
    }
    ss2_sat_hi = last;
    if (!nspr) put_sprite(-128, -128, SPRITE_SIZE(1, 1), 0);
    VDP_setSpriteLink((u16)(nspr - 1), 0);
    PROF(12);
    VDP_updateSprites(nspr, DMA_QUEUE);
    PROF(14);
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
 * START = RUN, C = SELECT, A = II (attack), B = I (jump): the PCE pad's
 * II I order left to right on A B */
static u8 pce_pad(u16 p)
{
    u8 v = 0;
    if (p & BUTTON_LEFT) v |= 0x80;
    if (p & BUTTON_DOWN) v |= 0x40;
    if (p & BUTTON_RIGHT) v |= 0x20;
    if (p & BUTTON_UP) v |= 0x10;
    if (p & BUTTON_START) v |= 0x08;
    if (p & BUTTON_C) v |= 0x04;
    if (p & BUTTON_A) v |= 0x02;
    if (p & BUTTON_B) v |= 0x01;
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


#ifdef SS2_TPROF
/* trace-profiling build (-D SS2_TPROF): scanlines spent per section in each
 * loop pass, read once per refresh by tools/ss2/gpgx_shot.c (SS2_TRACE_PROF).
 * ss2_prof_mark(n) charges the time since the previous mark to section n.
 * Time = vtimer (VBlank count) * 262 + lines since VBlank start; NTSC counts
 * $E5-$EA twice, so a mark there can be up to 6 lines early. No waits and
 * no drawing: the game's own timing is kept. ss2_tp_last[23] = whole pass. */
u16 ss2_tp_now[24];
volatile u16 ss2_tp_last[24];   /* read by the harness only */
/* event counts per pass (SS2_TRACE_COUNT): 0 cache_tile calls, 1 cache
 * hits, 2 sprite rows drawn, 3 SAT entries visited */
u16 ss2_tc_now[4];
volatile u16 ss2_tc_last[4];
static u32 tp_prev, tp_start;
static u32 tp_time(void)
{
    u16 v = (u16)GET_VCOUNTER;
    u32 t = vtimer;
    t = t * 262 + (v >= 0xE0 ? (u32)(v - 0xE0) : (u32)v + 38);
    return t < tp_prev ? tp_prev : t;     /* the repeated $E5-$EA lines */
}
void ss2_prof_mark(u8 sec)
{
    u32 t = tp_time();
    ss2_tp_now[sec] += (u16)(t - tp_prev);
    tp_prev = t;
}
static void tp_frame(void)
{
    u8 k;
    u32 t = tp_time();
    ss2_tp_now[23] = (u16)(t - tp_start);
    for (k = 0; k < 24; k++) { ss2_tp_last[k] = ss2_tp_now[k]; ss2_tp_now[k] = 0; }
    for (k = 0; k < 4; k++) { ss2_tc_last[k] = ss2_tc_now[k]; ss2_tc_now[k] = 0; }
    tp_start = tp_prev = t;
}
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

#ifdef SS2_DBG_NPC_X
/* diagnostic builds: research/npc/cap.py's two-phase warp to a townsperson
 * record (SS2_DBG_NPC_X/Y, world px): the camera first puts it just past the
 * right edge so it spawns, 8 frames later the player is placed at
 * SS2_DBG_P2X/Y (world px) and live objects keep their world places */
static void dbg_camera(u16 cx, u16 cy)
{
    u8 i;
    ZP(0x45) = (u8)(cx >> 8); ZP(0x46) = (u8)cx; ZP(0x47) = 0;
    ZP(0x48) = (u8)(cy >> 8); ZP(0x49) = (u8)cy; ZP(0x4A) = 0;
    for (i = 0; i < 6; i++) ZP(0x4B + i) = ZP(0x45 + i);
    R(0x2298) = 0; R(0x2299) = 0x80; R(0x229A) = 0;
    R(0x229B) = 0; R(0x229C) = 0xA0; R(0x229D) = 0;
    R(0x22A2) = R(0x22A3) = R(0x22A4) = R(0x22A5) = 0;
}

static void dbg_npc_phase1(void)
{
    u8 x;
    for (x = 0; x < 0x20; x++)
        if (O(0x245A, x))
        {
            u8 s = O(0x281A, x);
            if (s != 0xBF && R(0x2B5A + s) && R(0x2B5A + s) != 0xFF) R(0x2B5A + s)--;
            O(0x245A, x) = 0;
        }
    dbg_camera((u16)((SS2_DBG_NPC_X - 0x120) & 0x7FF), (u16)((SS2_DBG_NPC_Y - 0xA0) & 0x7FF));
}

static void dbg_npc_phase2(void)
{
    u16 ocx = (u16)((ZP(0x45) << 8) | ZP(0x46)), ocy = (u16)((ZP(0x48) << 8) | ZP(0x49));
    u16 cx = (u16)((SS2_DBG_P2X - 0x80) & 0x7FF), cy = (u16)((SS2_DBG_P2Y - 0xA0) & 0x7FF);
    s16 dx = (s16)(((cx - ocx + 0x400) & 0x7FF) - 0x400), dy = (s16)(((cy - ocy + 0x400) & 0x7FF) - 0x400);
    u8 x;
    for (x = 0; x < 0x20; x++)
        if (O(0x245A, x))
        {
            u16 v = (u16)(((O(0x247A, x) << 8) | O(0x249A, x)) - dx);
            O(0x247A, x) = (u8)(v >> 8); O(0x249A, x) = (u8)v;
            v = (u16)(((O(0x24DA, x) << 8) | O(0x24FA, x)) - dy);
            O(0x24DA, x) = (u8)(v >> 8); O(0x24FA, x) = (u8)v;
        }
    dbg_camera(cx, cy);
}
#endif

static void run_loop(void);

/* The cartridge: power on into the title, as the HuCard. Diagnostic builds
 * (-D SS2_DBG_*) start a stage from its research bootstrap instead. */
#if defined(SS2_DBG_STAGE) || defined(SS2_DBG_WX) || defined(SS2_DBG_NPC_X) || \
    defined(SS2_DBG_MAGIC) || defined(SS2_DBG_CLOUD) || defined(SS2_DBG_KEY)
#define SS2_BOOTSTRAP 1
#endif

#ifdef SS2_HEAPCHK
__attribute__((used, externally_visible)) volatile u16 ss2_heap_free[2];  /* free, largest block after the stacks */
#endif
void ss2_run(void)
{
    ss2_tasks_alloc();
#ifdef SS2_HEAPCHK
    ss2_heap_free[0] = MEM_getFree();
    ss2_heap_free[1] = MEM_getLargestFreeBlock();
#endif
#ifdef SS2_BOOTSTRAP
#ifdef SS2_DBG_STAGE
    ss2_run_stage(SS2_DBG_STAGE);
#else
    ss2_run_stage(0);
#endif
#else
    gfx = &ss2_stage_gfx[0];
    memset(def_bank, 0, sizeof def_bank);
    memset(def_src, 0, sizeof def_src);
    pairs_ok = 0;
    snd_shutdown();
    ss2_sound_init();
    ss2_scene = SS2_SCENE_PLAY;
    ss2_stage_request = 0;
    ss2_options_load();
    ss2_power_on();
    video_init();
    camera_from_state();
    apply_scroll();
    run_loop();
#endif
}

/* Start stage k (0-6) from its bootstrap state */
void ss2_run_stage(u8 k)
{

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
    pairs_ok = 0;
    load_bootstrap();
#ifdef SS2_DBG_NPC_X
    dbg_npc_phase1();
#endif
#ifdef SS2_DBG_MAGIC
    /* diagnostic builds: every magic owned, full magic, magic SS2_DBG_MAGIC
     * selected (1 fireball, 2 freeze, 3 pillar, 4 shield, 5 sweep) */
    R(0x22BF) = 0x3F;
    R(0x22BC) = 0x18;
    P_MAGIC = 0x18;
    R(0x22BD) = 0x18;
    R(0x22C0) = SS2_DBG_MAGIC;
#endif
#ifdef SS2_DBG_KEY
    R(0x22C1) |= 0x01;                      /* the boss-door key */
#endif
#ifdef SS2_DBG_CLOUD
    P_CLOUDREQ = 0x08;                      /* the cloud the stage-2 boss gives */
#endif
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

    run_loop();
}

/* Full-rate logic. A pass that ends after the next VBlank has gone by
 * (not inside it: SGDK then still processes that VBlank) has cost the game
 * a tick: the next tick runs at once, without waiting, and skips the
 * presentation (scroll, palette, sprites, HUD; scene/shop changes, map
 * streaming and the BAT queue still run). Then a presented pass follows,
 * so at most one tick in a row is not shown, and owed ticks beyond that
 * are dropped (the old slowdown, now only when even that cannot keep up). */
#ifndef SS2_MAX_SKIP
#define SS2_MAX_SKIP 2              /* ticks in a row that may go unshown */
#endif
static u8 catchup;                  /* the next tick runs at once, not shown */
static u8 skip_run;                 /* unshown ticks in a row so far */
static u32 tick_vt;                 /* VBlank count the current tick belongs to */
u16 ss2_present_n;                  /* presented ticks (ss2_loop_n: all ticks) */

/* the frame loop: VBlank, sound, logic, presentation; START+C leaves */
static void run_loop(void)
{
    u16 pad;
    u8 skip;
#ifdef SS2_TPROF_PC
    band_set(1);
#endif
    for (;;)
    {
#ifdef SS2_PROF
        u32 t0;
        u16 v0;
#endif
        /* a frame that ends a little into VBlank still gets this VBlank
         * (ON_VBLANK_START would wait a whole frame more) */
        skip = catchup;
        catchup = 0;
        if (skip)
        {
            skip_run++;
            tick_vt++;                          /* the VBlank this tick stands for */
            JOY_update();                       /* (SYS_doVBlankProcess does it otherwise) */
        }
        else
        {
            band_idle = 1;
            SYS_doVBlankProcessEx(ON_VBLANK);
            band_idle = 0;
            tick_vt = vtimer;
            skip_run = 0;
        }
        ss2_loop_n++;
#ifdef SS2_TPROF
        tp_frame();
#endif
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
            if ((tick % 12) < 3) pad |= BUTTON_A;
            if ((tick % 90) >= 30 && (tick % 90) < 39) pad |= BUTTON_B;
            tick++;
        }
#endif
        if ((pad & (BUTTON_START | BUTTON_C)) == (BUTTON_START | BUTTON_C))
            break;                              /* RUN+SELECT: the PCE reset combo */
#ifdef SS2_DBG_NPC_X
        {
            static u16 dbg_n;
            if (++dbg_n == 9) dbg_npc_phase2();
        }
#endif
#ifdef SS2_DBG_FLOW
        /* diagnostic builds: SS2_DBG_FLOW_AT frames after the HUD split is
         * first up (the stage intro included), start the stage clear (1), or
         * kill the player without (2) or with (3) the revive charm, or clear
         * stage 7 (4: the ending) */
        {
#ifndef SS2_DBG_FLOW_AT
#define SS2_DBG_FLOW_AT 120
#endif
            static u16 dbg_n;
            if ((dbg_n || (R(0x2201) & 0x04)) && ++dbg_n == SS2_DBG_FLOW_AT)
            {
                if (SS2_DBG_FLOW == 1) ss2_stage_clear_start();
                else if (SS2_DBG_FLOW == 4) { Z_STAGE = 6; ss2_stage_clear_start(); }   /* the last stage: the ending */
                else
                {
                    if (SS2_DBG_FLOW == 3) R(0x22C1) |= 0x04;
                    P_FLAGSC1 &= (u8)~0x02;
                    P_LIFE = 0; R(0x22BA) = 0; P_DEATH = 0; P_MODE = 0x40;
                }
            }
        }
#endif
        band_idle = 1;                          /* the logic touches no VDP port */
        ss2_game_vblank(pce_pad(pad));
        PROF(0);
#ifndef SS2_SOUND_LATE
        sound_irq();
#endif
        PROF(0);
        ss2_game_frame();
        PROF(8);
        band_idle = 0;
        camera_from_state();
        scene_update();
        shop_scene();
        PROF(9);
        if (ss2_redraw_request) { ss2_redraw_request = 0; redraw_view(); map_live = 1; }   /* $EBE1 */
        else if (!shop_shown && map_live && (R(0x2201) & 0x04)) stream_view();
        PROF(10);
        bat_drain();
        if (!skip)
        {
            apply_scroll();
            apply_fade();
            PROF(11);
#ifdef SS2_SPRCHECK
            {
                /* check build: the memoized rows must equal the full path's */
                static VDPSprite full[64];
                u8 n0;
                u8 conv;
                spr_nomemo = 1; draw_sprites(); n0 = nspr; conv = conv_n;
                memcpy(full, vdpSpriteCache, sizeof full);
                spr_nomemo = 0; draw_sprites();
                ss2_sprchk[0]++;
                /* a pass that converted cells: the second call has its own
                 * conversion budget, so the two can differ without the memo */
                if (conv | conv_n) ss2_sprchk[2]++;
                else
                {
                    const u8 *a = (const u8 *)full, *b = (const u8 *)vdpSpriteCache;
                    u16 k, bad = (u16)(n0 != nspr);
                    for (k = 0; k < (u16)n0 * sizeof(VDPSprite); k++) bad |= (u16)(a[k] != b[k]);
                    if (bad) ss2_sprchk[1]++;
                }
            }
#else
            draw_sprites();
#endif
            PROF(12);
            draw_hud(0);
            ss2_present_n++;
        }
        /* late? A VBlank after this tick's own has gone by (and we are not
         * still inside it, when it would be processed): the next tick runs
         * now, unshown, unless SS2_MAX_SKIP are already in a row */
        {
            u32 late = (u32)(vtimer - tick_vt);
            if (late && GET_VDP_STATUS(VDP_VBLANK_FLAG)) late--;
            catchup = (u8)(late && skip_run < SS2_MAX_SKIP);
        }
#ifdef SS2_SOUND_LATE
        sound_irq();                            /* profiling: sampled lines */
#endif
#ifdef SS2_STACKCHK
        { void ss2_stack_check(void); if (!(ss2_loop_n & 15)) ss2_stack_check(); }
#endif
        PROF(7);
#ifdef SS2_PROF
        prof_end(t0, v0);
#endif
    }
    ss2_sound_shutdown();
    video_shutdown();
    ss2_tasks_free();
}

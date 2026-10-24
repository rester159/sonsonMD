/* selector.c -- ARCADE / HOME mode selector (native boot-flow, Phase 8).
 *
 * A clean NATIVE re-creation of the sibling port's mode_selector() (home.c).
 * The original draws into the 6809 engine's videoram mirror (ram_lo) using the
 * game's own font tiles and the transliteration present path -- none of which
 * exist in this native build -- so this is a faithful-enough re-creation per
 * the Phase-8 directive: same screen content + layout (SONSON title up high,
 * ARCADE / HOME choice, the prompt, and the EXACT credit string), rendered with
 * SGDK's font on a text plane. No old-port engine state.
 *
 * Layout (H40 / 40-col text grid, owner-approved cover-art proposal):
 *   rows  1- 2   SONSON title banner
 *   rows  4-22   two cover images, IDENTICAL 16x19 tiles (128x152 px), on BG_B:
 *                ARCADE = arcade flyer at cols 3-18, HOME = PC Engine box at
 *                cols 21-36 (selector_art.c, baked by tools/gen_selector_art.py)
 *   row  24      ARCADE / HOME labels under each cover (selected = red + a red
 *                frame round its cover, else white)
 *   row  26      PRESS ANY BUTTON TO SELECT   (yellow)
 *   row  27      PORTED BY RESTER 159, 2026   (white)  <- exact shipped string
 *
 * Palettes: PAL0 = ARCADE cover, PAL1 = HOME cover (15 Genesis colours each,
 * index 15 forced white so white text draws with PAL0), PAL2 = red text +
 * frame, PAL3 = yellow text.
 *
 * Behaviour (matches home.c::mode_selector): Up/Down (or Left/Right) moves the
 * highlight (wraps between the 2 covers), any button (A/B/C/Start) confirms; edge-triggered
 * with the 3-read debounce + 20-frame settle that kill the SGDK 6-button
 * TH-detect phantom presses at power-on (both pads forced to 3-button mode).
 * Engine-off frame loop: wait a VBlank (vtimer ticks in SGDK's V-Int), update
 * the pads here, redraw, repeat.
 *
 * Returns MODE_ARCADE (0) or MODE_HOME (1).
 */
#include <genesis.h>
#include "frontend.h"
#include "selector_art.h"

/* Text colours are picked by switching VDP_setTextPalette() before each draw.
 * Index 0 transparent (the black backdrop shows through); the font draws in
 * index 15. White shares the ARCADE cover's line (its index 15 is white). */
#define PAL_ARCADE  PAL0
#define PAL_HOME    PAL1
#define PAL_WHITE   PAL0
#define PAL_RED     PAL2
#define PAL_YELLOW  PAL3

/* The exact credit string the owner shipped (home.c, owner request 2026-06-12).
 * Kept verbatim -- this is the load-bearing presentation requirement. */
#define CREDIT_STR  "PORTED BY RESTER 159, 2026"

#define COLS        40   /* H40 text grid width */
#define ROW_COVER   4    /* cover top row (19 rows tall)          */
#define ROW_LABEL   24
#define COL_ARCADE  3    /* cover left columns (16 cols wide)     */
#define COL_HOME    21

/* VRAM: ARCADE tiles, then HOME tiles, then the 8 frame tiles. */
#define VRAM_ARCADE TILE_USER_INDEX
#define VRAM_HOME   (VRAM_ARCADE + SELECTOR_ART_ARCADE_TILE_COUNT)
#define VRAM_FRAME  (VRAM_HOME + SELECTOR_ART_HOME_TILE_COUNT)

/* Selection frame: a 2px red line (colour 15) with a 1px gap round the cover,
 * drawn in the ring of cells just outside it. Order: TL T TR L R BL B BR. */
#define FL 0x00000FF0   /* left line   (px 5-6) */
#define FR 0x0FF00000   /* right line  (px 1-2) */
#define FA 0xFFFFFFFF   /* across      */
static const u32 frame_tiles[8 * 8] = {
    0, 0, 0, 0, 0, 0x00000FFF, 0x00000FFF, FL,      /* TL */
    0, 0, 0, 0, 0, FA, FA, 0,                       /* T  */
    0, 0, 0, 0, 0, 0xFFF00000, 0xFFF00000, FR,      /* TR */
    FL, FL, FL, FL, FL, FL, FL, FL,                 /* L  */
    FR, FR, FR, FR, FR, FR, FR, FR,                 /* R  */
    FL, 0x00000FFF, 0x00000FFF, 0, 0, 0, 0, 0,      /* BL */
    0, FA, FA, 0, 0, 0, 0, 0,                       /* B  */
    FR, 0xFFF00000, 0xFFF00000, 0, 0, 0, 0, 0,      /* BR */
};

/* ---- one-colour font palette: a flat colour in index 15, 0 transparent. */
static void set_font_pal(u16 palLine, u16 color)
{
    u16 pal[16];
    u16 i;
    for (i = 0; i < 16; i++) pal[i] = 0x0000;   /* transparent / black */
    pal[15] = color;                            /* the glyph colour    */
    PAL_setColors(palLine * 16, pal, 16, CPU);
}

/* ---- text centred on column cx (x2: cx2 = 2*centre, so half-columns work). */
static void draw_at(u16 row, u16 cx2, const char *s, u16 palLine)
{
    u16 len = 0;
    const char *p;
    for (p = s; *p; p++) len++;
    VDP_setTextPalette(palLine);
    VDP_drawTextBG(BG_A, s, (u16)((cx2 - len) / 2), row);
}

/* ---- centred text on the active text plane (40-col grid). */
static void draw_center(u16 row, const char *s, u16 palLine)
{
    draw_at(row, COLS, s, palLine);
}

/* ---- one cover image on BG_B at (col, ROW_COVER). */
static void draw_cover(const u16 *map, u16 vram, u16 pal, u16 col)
{
    u16 x, y;
    for (y = 0; y < SELECTOR_ART_TH; y++)
        for (x = 0; x < SELECTOR_ART_TW; x++)
            VDP_setTileMapXY(BG_B,
                TILE_ATTR_FULL(pal, 0, 0, 0, vram + map[y * SELECTOR_ART_TW + x]),
                col + x, ROW_COVER + y);
}

/* ---- load + draw both covers (same size, side by side). */
static void draw_covers(void)
{
    PAL_setColors(PAL_ARCADE * 16, selector_art_arcade_pal, 16, CPU);
    PAL_setColors(PAL_HOME   * 16, selector_art_home_pal,   16, CPU);
    VDP_loadTileData(selector_art_arcade_tiles, VRAM_ARCADE, SELECTOR_ART_ARCADE_TILE_COUNT, DMA);
    VDP_loadTileData(selector_art_home_tiles,   VRAM_HOME,   SELECTOR_ART_HOME_TILE_COUNT,   DMA);
    VDP_loadTileData(frame_tiles, VRAM_FRAME, 8, DMA);
    draw_cover(selector_art_arcade_map, VRAM_ARCADE, PAL_ARCADE, COL_ARCADE);
    draw_cover(selector_art_home_map,   VRAM_HOME,   PAL_HOME,   COL_HOME);
}

/* ---- the red selection frame round the cover at col (or clear it). */
static void draw_frame(u16 col, bool on)
{
    const u16 x0 = col - 1, x1 = col + SELECTOR_ART_TW;
    const u16 y0 = ROW_COVER - 1, y1 = ROW_COVER + SELECTOR_ART_TH;
    u16 i;
#define FT(n) (on ? TILE_ATTR_FULL(PAL_RED, 0, 0, 0, VRAM_FRAME + (n)) : 0)
    VDP_setTileMapXY(BG_A, FT(0), x0, y0);
    VDP_setTileMapXY(BG_A, FT(2), x1, y0);
    VDP_setTileMapXY(BG_A, FT(5), x0, y1);
    VDP_setTileMapXY(BG_A, FT(7), x1, y1);
    for (i = col; i < x1; i++)
    {
        VDP_setTileMapXY(BG_A, FT(1), i, y0);
        VDP_setTileMapXY(BG_A, FT(6), i, y1);
    }
    for (i = ROW_COVER; i < y1; i++)
    {
        VDP_setTileMapXY(BG_A, FT(3), x0, i);
        VDP_setTileMapXY(BG_A, FT(4), x1, i);
    }
#undef FT
}

/* ---- the SONSON title banner (rows 1-2). Drawn boldly with wide spacing +
 * a rule line so it reads as a title rather than a menu label. The native
 * baked arcade title tiles aren't usable here (they're keyed to the captured
 * gameplay scene's char set, not the title glyphs), so this is the
 * faithful-enough SGDK-font recreation the directive allows. */
static void draw_title(void)
{
    /* white = the covers' index 15 (PAL0 is the ARCADE cover line) */
    set_font_pal(PAL_RED,    0x000E);   /* red           */
    set_font_pal(PAL_YELLOW, 0x00EE);   /* yellow        */

    draw_center(1, "S O N S O N", PAL_WHITE);
    draw_center(2, "============", PAL_RED);
}

/* ---- redraw the two labels + the frame with the current highlight. */
static void draw_choices(u8 sel)
{
    draw_frame(COL_ARCADE, sel == MODE_ARCADE);
    draw_frame(COL_HOME,   sel == MODE_HOME);
    draw_at(ROW_LABEL, 2 * COL_ARCADE + SELECTOR_ART_TW, "ARCADE",
            sel == MODE_ARCADE ? PAL_RED : PAL_WHITE);
    draw_at(ROW_LABEL, 2 * COL_HOME + SELECTOR_ART_TW, "HOME",
            sel == MODE_HOME ? PAL_RED : PAL_WHITE);
}

/* ---- debounced edge reader (home.c parity): a pad bit must be seen in THREE
 * consecutive reads before it can edge -- the SGDK 6-button-detect TH
 * transients on power-up were observed operating the menu otherwise. */
static u16 edge_raw1, edge_raw2, edge_stab_prev;

static void edges_rearm(void)
{
    edge_raw1 = 0xFFFF;
    edge_raw2 = 0xFFFF;
    edge_stab_prev = 0xFFFF;
}

static u16 edges_read(void)
{
    u16 raw     = (u16)(JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2));
    u16 stable  = (u16)(raw & edge_raw1 & edge_raw2);
    u16 pressed = (u16)(stable & (u16)~edge_stab_prev);
    edge_raw2 = edge_raw1;
    edge_raw1 = raw;
    edge_stab_prev = stable;
    return pressed;
}

static void wait_vblank(void)
{
    vu32 t = vtimer;
    while (vtimer == t) { }
}

u8 selector_run(void)
{
    u8  sel = MODE_ARCADE;          /* initial highlight = ARCADE (home.c)   */
    u16 settle = 0;                 /* ignore the first reads after JOY_init */

    /* Force both pads to 3-button mode (kills the 6-button TH-detect phantom
     * presses at power-on -- FRONTEND_SPEC 3 phantom-press fix). JOY_init is
     * called once in main() before the front-end; re-asserting support here is
     * harmless and keeps the selector self-describing. */
    JOY_setSupport(PORT_1, JOY_SUPPORT_3BTN);
    JOY_setSupport(PORT_2, JOY_SUPPORT_3BTN);

    /* Fresh blank screen on BG_A (the splashes left it cleared, but be sure). */
    VDP_setTextPlane(BG_A);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setBackgroundColor(0);
    /* A returning pass comes back from the game's camera-scrolled BG_A. */
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_B, 0);

    draw_covers();
    draw_title();
    draw_center(26, "PRESS ANY BUTTON TO SELECT", PAL_YELLOW);
    draw_center(27, CREDIT_STR, PAL_WHITE);
    edges_rearm();
    draw_choices(sel);

    for (;;)
    {
        wait_vblank();
        JOY_update();               /* engine off: refresh pads here          */
        u16 pressed = edges_read();
        if (settle < 20) { settle++; pressed = 0; }

        if (pressed & (BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT))
        {
            sel ^= 1;               /* two covers -> toggle (wraps)           */
            draw_choices(sel);
        }
        if (pressed & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START))
            break;
    }

    /* Clear our screen so game_init starts from black (the cover palettes
     * too -- the next screen sets its own lines). */
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    PAL_setColors(0, palette_black, 32, CPU);
    return sel;
}

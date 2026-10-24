/* game_select.c -- SELECT GAME screen (native boot-flow, first session stage).
 *
 * Picks the game before anything else: the SONSON logo (arcade flyer) or the
 * SON SON II logo (PC Engine box). SONSON goes on to the ARCADE / HOME mode
 * selector (selector.c); SON SON II goes to its port (main.c).
 *
 * Layout (H40 / 40-col grid, owner-approved):
 *   row   7      SELECT GAME   (the game's own arcade font, title-screen glyphs)
 *   rows 10-19   two logos, IDENTICAL 18x10 tiles (144x80 px), on BG_B:
 *                SONSON at cols 1-18, SON SON II at cols 21-38
 *                (selector_art.c, baked by tools/gen_selector_art.py: keyed
 *                onto the black backdrop, white taken out)
 *   rows 9/20    a red frame round the selected logo (ring of cells outside it)
 *
 * Palettes: PAL0 = SONSON logo, PAL1 = SON SON II logo (15 Genesis colours
 * each), PAL2 = red frame (index 15), PAL3 = the arcade char palette for the
 * font glyphs.
 *
 * Behaviour (selector.c parity): Up/Down/Left/Right toggle the highlight, any
 * button (A/B/C/Start) confirms; edge-triggered with the 3-read debounce +
 * 20-frame settle that kill the SGDK 6-button TH-detect phantom presses.
 *
 * Returns GAME_SONSON (0) or GAME_SONSON2 (1).
 */
#include <genesis.h>
#include "frontend.h"
#include "selector_art.h"
#include "ss_title.h"

#define PAL_SONSON  PAL0
#define PAL_SONSON2 PAL1
#define PAL_FRAME   PAL2
#define PAL_FONT    PAL3

#define ROW_TEXT    7
#define ROW_LOGO    10   /* logo top row (10 rows tall)           */
#define COL_SONSON  1    /* logo left columns (18 cols wide)      */
#define COL_SONSON2 21
#define COLS        40   /* H40 text grid width */

/* Arcade font: char codes 0-9 digits, 16 space, 17-42 A-Z (ss_title.h). */
#define GLYPH_COUNT 43

/* VRAM: SONSON tiles, SON SON II tiles, the 8 frame tiles, the font. */
#define VRAM_SONSON  TILE_USER_INDEX
#define VRAM_SONSON2 (VRAM_SONSON + SELECTOR_ART_ARCADE_TILE_COUNT)
#define VRAM_FRAME   (VRAM_SONSON2 + SELECTOR_ART_HOME_TILE_COUNT)
#define VRAM_FONT    (VRAM_FRAME + 8)

/* Selection frame: a 2px red line (colour 15) with a 1px gap round the logo,
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

static const u16 pal_frame[16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x000E   /* 15 = red */
};

/* ---- centred text in the arcade font (white glyph block, title.c glyph()). */
static void draw_text_center(u16 row, const char *s)
{
    u16 i, len = 0, start;
    for (i = 0; s[i]; i++) len++;
    start = (u16)((COLS - len) / 2);
    for (i = 0; s[i]; i++)
    {
        const char ch = s[i];
        u16 code;
        if (ch >= '0' && ch <= '9')      code = (u16)(ch - '0');
        else if (ch >= 'A' && ch <= 'Z') code = (u16)(17 + (ch - 'A'));
        else                             continue;      /* space: leave blank */
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL_FONT, 0, 0, 0, VRAM_FONT + code),
                         start + i, row);
    }
}

/* ---- one logo on BG_B at (col, ROW_LOGO). */
static void draw_logo(const u16 *map, u16 vram, u16 pal, u16 col)
{
    u16 x, y;
    for (y = 0; y < SELECTOR_ART_TH; y++)
        for (x = 0; x < SELECTOR_ART_TW; x++)
            VDP_setTileMapXY(BG_B,
                TILE_ATTR_FULL(pal, 0, 0, 0, vram + map[y * SELECTOR_ART_TW + x]),
                col + x, ROW_LOGO + y);
}

/* ---- load palettes + tiles, draw both logos (same size, side by side). */
static void draw_screen(void)
{
    PAL_setColors(PAL_SONSON  * 16, selector_art_arcade_pal, 16, CPU);
    PAL_setColors(PAL_SONSON2 * 16, selector_art_home_pal,   16, CPU);
    PAL_setColors(PAL_FRAME   * 16, pal_frame,               16, CPU);
    PAL_setColors(PAL_FONT    * 16, ss_pal_char,             16, CPU);
    VDP_loadTileData(selector_art_arcade_tiles, VRAM_SONSON,  SELECTOR_ART_ARCADE_TILE_COUNT, DMA);
    VDP_loadTileData(selector_art_home_tiles,   VRAM_SONSON2, SELECTOR_ART_HOME_TILE_COUNT,   DMA);
    VDP_loadTileData(frame_tiles, VRAM_FRAME, 8, DMA);
    VDP_loadTileData(&ss_title_tiles[SS_TITLE_GLYPH_W * 8], VRAM_FONT, GLYPH_COUNT, DMA);
    draw_logo(selector_art_arcade_map, VRAM_SONSON,  PAL_SONSON,  COL_SONSON);
    draw_logo(selector_art_home_map,   VRAM_SONSON2, PAL_SONSON2, COL_SONSON2);
    draw_text_center(ROW_TEXT, "SELECT GAME");
}

/* ---- the red selection frame round the logo at col (or clear it). */
static void draw_frame(u16 col, bool on)
{
    const u16 x0 = col - 1, x1 = col + SELECTOR_ART_TW;
    const u16 y0 = ROW_LOGO - 1, y1 = ROW_LOGO + SELECTOR_ART_TH;
    u16 i;
#define FT(n) (on ? TILE_ATTR_FULL(PAL_FRAME, 0, 0, 0, VRAM_FRAME + (n)) : 0)
    VDP_setTileMapXY(BG_A, FT(0), x0, y0);
    VDP_setTileMapXY(BG_A, FT(2), x1, y0);
    VDP_setTileMapXY(BG_A, FT(5), x0, y1);
    VDP_setTileMapXY(BG_A, FT(7), x1, y1);
    for (i = col; i < x1; i++)
    {
        VDP_setTileMapXY(BG_A, FT(1), i, y0);
        VDP_setTileMapXY(BG_A, FT(6), i, y1);
    }
    for (i = ROW_LOGO; i < y1; i++)
    {
        VDP_setTileMapXY(BG_A, FT(3), x0, i);
        VDP_setTileMapXY(BG_A, FT(4), x1, i);
    }
#undef FT
}

static void draw_choice(u8 sel)
{
    draw_frame(COL_SONSON,  sel == GAME_SONSON);
    draw_frame(COL_SONSON2, sel == GAME_SONSON2);
}

/* ---- debounced edge reader (selector.c parity): a pad bit must be seen in
 * THREE consecutive reads before it can edge. */
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

u8 game_select_run(void)
{
    u8  sel = GAME_SONSON;
    u16 settle = 0;                 /* ignore the first reads after JOY_init */

    JOY_setSupport(PORT_1, JOY_SUPPORT_3BTN);
    JOY_setSupport(PORT_2, JOY_SUPPORT_3BTN);

    VDP_setTextPlane(BG_A);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setBackgroundColor(0);
    /* A returning pass comes back from the game's camera-scrolled BG_A. */
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_B, 0);

    draw_screen();
    edges_rearm();
    draw_choice(sel);

    for (;;)
    {
        wait_vblank();
        JOY_update();               /* engine off: refresh pads here          */
        u16 pressed = edges_read();
        if (settle < 20) { settle++; pressed = 0; }

        if (pressed & (BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT))
        {
            sel ^= 1;               /* two logos -> toggle (wraps)            */
            draw_choice(sel);
        }
        if (pressed & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START))
            break;
    }

    /* Clear our screen + palettes so the next stage starts from black. */
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    PAL_setColors(0, palette_black, 64, CPU);
    return sel;
}

/* dipswitch.c -- ARCADE DIP-switch / service screen (native boot-flow, 8b).
 *
 * The MODE_ARCADE branch of the front-end (main.c, after selector_run). It
 * presents SonSon's REAL arcade DIP bank as a scrollable service screen and
 * writes the chosen settings into g_config (game.h).
 *
 * AUTHENTIC SOURCE. Every switch, its value list, and the bit it occupies were
 * taken from MAME's machine definition --
 *     D:/portForge/tools/mame/mame.exe -listxml sonson
 * cross-checked with the verified sibling port's shared menu_label/menu_cycle
 * (../sonson/port/src/main.c). The two raw bytes g_config.dsw1 / .dsw2 mirror
 * the hardware DSW1 / DSW2 banks bit-for-bit, so this screen edits the real
 * switches; sync_from_dsw() then decodes them into the engine-facing scalar
 * fields. The full bank (10 switches):
 *
 *   DSW1: Coinage (0x0F) | Coinage Affects (0x10) | Demo Sounds (0x20) |
 *         Service Mode (0x40) | Flip Screen (0x80)
 *   DSW2: Lives (0x03) | 2 Players Game (0x04) | Bonus Life (0x18) |
 *         Difficulty (0x60) | Freeze (0x80)
 *
 * STYLE. Matches selector.c: SGDK font on a text plane (H40), per-line colour
 * via a flat one-colour font palette, the SAME 3-read debounce + 20-frame
 * settle that kill the power-on 6-button TH-detect phantom presses (both pads
 * forced to 3-button mode). The list is longer than the value column is tall,
 * so it SCROLLS: a fixed window of rows with the cursor clamped inside and the
 * list panning under it. Up/Down move the cursor, Left/Right change the
 * highlighted switch's value, Start/A/B/C confirm.
 *
 * Title: arcade service look ("DIP SWITCHES").
 */
#include <genesis.h>
#include "arcade_ui.h"
#include "frontend.h"
#include "game.h"
#include "save.h"

/* Font palette lines (selector.c parity): index 0 transparent, 15 = glyph. */
#define PAL_WHITE   PAL1
#define PAL_RED     PAL2
#define PAL_YELLOW  PAL3

#define COLS        32            /* H40 text grid width                       */

/* Layout: the DIP list scrolls inside a fixed row window. */
#define LIST_TOP    6             /* first on-screen list row                  */
#define VIS_ROWS    10            /* visible list rows (we show 10 of 10 here) */
#define NAME_COL    3             /* left column of the switch name            */
#define VAL_COL     19            /* left column of the value string           */

/* ---- DIP switch model ----------------------------------------------------
 * Each row is one hardware switch: which bank byte it lives in, a mask, a shift,
 * how many positions it cycles through, the value labels (indexed by the raw
 * bitfield value), and whether it is purely informational here. The labels are
 * indexed by the EXTRACTED field value ((byte & mask) >> shift) -- so they are
 * listed in raw-bitfield order, exactly matching MAME's <dipvalue value=...>. */
typedef struct
{
    const char *name;
    u8          bank;             /* 0 = dsw1, 1 = dsw2                         */
    u8          mask;             /* field mask within the bank byte           */
    u8          shift;            /* right-shift to extract the field          */
    u8          count;            /* number of cycle positions                 */
    const char *labels[16];       /* value text, indexed by extracted value    */
} DipRow;

/* Coinage (DSW1 0x0F): 16 settings; labels indexed by the raw nibble value
 * (MAME's <dipvalue value=...> attribute, verbatim, abbreviated "<coins>C /
 * <credits>C"). value 15 = "1 COIN / 1 CREDIT" (default). */
static const char *const COINAGE[16] =
{
    "FREE PLAY",        /*  0                          */
    "4C / 3C",          /*  1  (4 Coins / 3 Credits)   */
    "4C / 1C",          /*  2                          */
    "3C / 4C",          /*  3                          */
    "3C / 2C",          /*  4                          */
    "3C / 1C",          /*  5                          */
    "2C / 5C",          /*  6                          */
    "2C / 3C",          /*  7                          */
    "2C / 1C",          /*  8                          */
    "1C / 7C",          /*  9                          */
    "1C / 6C",          /* 10                          */
    "1C / 5C",          /* 11                          */
    "1C / 4C",          /* 12                          */
    "1C / 3C",          /* 13                          */
    "1C / 2C",          /* 14                          */
    "1C / 1C"           /* 15  (default)               */
};

static const DipRow DIPS[] =
{
    /* --- DSW1 --- */
    { "COINAGE",      0, 0x0F, 0, 16,
        { COINAGE[0],COINAGE[1],COINAGE[2],COINAGE[3],COINAGE[4],COINAGE[5],
          COINAGE[6],COINAGE[7],COINAGE[8],COINAGE[9],COINAGE[10],COINAGE[11],
          COINAGE[12],COINAGE[13],COINAGE[14],COINAGE[15] } },
    { "COIN SLOT", 0, 0x10, 4, 2, { "COIN B", "COIN A" } },
    { "DEMO SOUNDS",  0, 0x20, 5, 2, { "ON", "OFF" } },        /* 0=On default  */
    { "FLIP SCREEN",  0, 0x80, 7, 2, { "ON", "OFF" } },        /* 0=On,0x80=Off */
    /* --- DSW2 --- */
    { "LIVES",        1, 0x03, 0, 4, { "7", "5", "4", "3" } }, /* idx3=3 default*/
    { "2 PLAYERS", 1, 0x04, 2, 2, { "2 CREDITS", "1 CREDIT" } },
    { "BONUS LIFE",   1, 0x18, 3, 4,
        { "30K 90K 120K", "20K 80K 100K", "30K ONLY", "20K ONLY" } },
    { "LEVEL",   1, 0x60, 5, 4, { "VERY HARD", "HARD", "NORMAL", "EASY" } },
    { "FREEZE",       1, 0x80, 7, 2, { "ON", "OFF" } },        /* 0=On,0x80=Off */
    { "SERVICE", 0, 0x40, 6, 2, { "ON", "OFF" } },        /* 0=On,0x40=Off */
};
#define DIP_COUNT  (sizeof(DIPS) / sizeof(DIPS[0]))

/* ---- one-colour font palette (selector.c set_font_pal). ------------------- */
static void set_font_pal(u16 palLine, u16 color)
{
    u16 pal[16];
    u16 i;
    for (i = 0; i < 16; i++) pal[i] = 0x0000;
    pal[15] = color;
    PAL_setColors(palLine * 16, pal, 16, CPU);
}

static void draw_center(u16 row, const char *s, u16 palLine)
{
    u16 len = 0;
    const char *p;
    for (p = s; *p; p++) len++;
    VDP_setTextPalette(palLine);
    VDP_drawTextBG(BG_A, s, (u16)((COLS - len) / 2), row);
}

static void draw_at(u16 col, u16 row, const char *s, u16 palLine)
{
    VDP_setTextPalette(palLine);
    VDP_drawTextBG(BG_A, s, col, row);
}

/* ---- debounced edge reader (selector.c parity: 3 consecutive reads). ------ */
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

/* ---- DIP field access (operate directly on the g_config raw banks). ------- */
static u8 dip_get(const DipRow *d)
{
    u8 b = (d->bank == 0) ? g_config.dsw1 : g_config.dsw2;
    return (u8)((b & d->mask) >> d->shift);
}

static void dip_set(const DipRow *d, u8 field)
{
    u8 v = (u8)((field << d->shift) & d->mask);
    if (d->bank == 0)
        g_config.dsw1 = (u8)((g_config.dsw1 & (u8)~d->mask) | v);
    else
        g_config.dsw2 = (u8)((g_config.dsw2 & (u8)~d->mask) | v);
}

static void dip_cycle(const DipRow *d, s8 dir)
{
    u8 f = dip_get(d);
    if (dir > 0) f = (u8)((f + 1) % d->count);
    else         f = (u8)((f + d->count - 1) % d->count);
    dip_set(d, f);
}

/* ===========================================================================
 *  sync_from_dsw -- decode the two raw DIP banks into g_config's scalar view.
 * ===========================================================================
 *  Shared by both config screens (declared in game.h via the externs below).
 *  Lives  : DSW2 0x03 -> {7,5,4,3} by raw value (index 3 = 3, the default).
 *  Bonus  : DSW2 0x18>>3 -> table index 0..3 (kept raw; labels in BONUS row).
 *  Diff   : DSW2 0x60>>5 -> raw {0=VeryHard..3=Easy}; we store 0=Easy..3=VHard
 *           so the engine's "higher = harder" reads naturally (3 - raw).
 *  Demo   : DSW1 0x20 == 0 -> on.
 *  Flip   : DSW1 0x80 == 0 -> flipped.
 * ========================================================================= */
void sync_from_dsw(void)
{
    static const u8 LIVES_TBL[4] = { 7, 5, 4, 3 };   /* by raw DSW2&0x03        */
    g_config.lives      = LIVES_TBL[g_config.dsw2 & 0x03];
    g_config.bonusLife  = (u8)((g_config.dsw2 >> 3) & 0x03);
    g_config.difficulty = (u8)(3 - ((g_config.dsw2 >> 5) & 0x03)); /* 0=Easy..3 */
    g_config.demoSound  = (u8)((g_config.dsw1 & 0x20) ? 0 : 1);
    g_config.flipScreen = (u8)((g_config.dsw1 & 0x80) ? 0 : 1);
}

/* ---- draw one list row (name + value) at an on-screen row. ---------------- */
static void draw_row(u16 idx, u16 screenRow, u8 highlighted)
{
    const DipRow *d = &DIPS[idx];
    u16 pal = highlighted ? PAL_RED : PAL_WHITE;

    VDP_clearTextArea(0, screenRow, COLS, 1);
    /* cursor marker on the highlighted row */
    if (highlighted) draw_at(NAME_COL - 2, screenRow, ">", PAL_RED);
    draw_at(NAME_COL, screenRow, d->name, pal);
    draw_at(VAL_COL, screenRow, d->labels[dip_get(d)], highlighted ? PAL_YELLOW : PAL_WHITE);
}

/* ---- repaint the visible window for the current cursor / scroll origin. ---- */
static void draw_list(u16 cursor, u16 top)
{
    u16 i;
    for (i = 0; i < VIS_ROWS; i++)
    {
        u16 idx = (u16)(top + i);
        u16 row = (u16)(LIST_TOP + i*2);
        if (idx < DIP_COUNT) draw_row(idx, row, (u8)(idx == cursor));
        else                 VDP_clearTextArea(0, row, COLS, 1);
    }
}

void dip_screen_run(void)
{
    u16 cursor = 0;          /* index into DIPS                                 */
    u16 top    = 0;          /* first visible index (scroll origin)            */
    u16 settle = 0;          /* ignore the first reads after JOY_init           */

    /* 3-button mode: kills the 6-button TH-detect phantom presses at power-on
     * (FRONTEND_SPEC 3 phantom-press fix; selector.c parity). */
    JOY_setSupport(PORT_1, JOY_SUPPORT_3BTN);
    JOY_setSupport(PORT_2, JOY_SUPPORT_3BTN);

    VDP_setTextPlane(BG_A);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setBackgroundColor(0);

    arcade_menu_begin(0);
    set_font_pal(PAL_WHITE,  0x0EEE);   /* bright white                        */
    set_font_pal(PAL_RED,    0x000E);   /* red                                 */
    set_font_pal(PAL_YELLOW, 0x00EE);   /* yellow                              */

    draw_center(2, "DIP SWITCHES", PAL_YELLOW);


    draw_center(26, "START", PAL_YELLOW);

    edges_rearm();
    draw_list(cursor, top);

    for (;;)
    {
        wait_vblank();
        JOY_update();
        u16 pressed = edges_read();
        if (settle < 20) { settle++; pressed = 0; }

        if (pressed & BUTTON_UP)
        {
            cursor = (u16)((cursor + DIP_COUNT - 1) % DIP_COUNT);
        }
        else if (pressed & BUTTON_DOWN)
        {
            cursor = (u16)((cursor + 1) % DIP_COUNT);
        }

        /* keep the cursor inside the visible window (scroll the list) */
        if (cursor < top)                       top = cursor;
        else if (cursor >= (u16)(top + VIS_ROWS)) top = (u16)(cursor - VIS_ROWS + 1);

        if (pressed & BUTTON_LEFT)  dip_cycle(&DIPS[cursor], -1);
        if (pressed & BUTTON_RIGHT) dip_cycle(&DIPS[cursor], +1);

        /* repaint the window every frame an edge fired (cheap; engine off) */
        if (pressed & (BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT))
            draw_list(cursor, top);

        if (pressed & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START))
            break;
    }

    /* commit: derive the engine-facing scalar fields from the edited banks. */
    sync_from_dsw();
    save_capture_settings(MODE_ARCADE);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
}

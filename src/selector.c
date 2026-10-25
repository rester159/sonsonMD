/* Opening mode selector: original arcade logo and lettering. */
#include <genesis.h>
#include "arcade_ui.h"
#include "frontend.h"

/* Three font palette lines so individual text lines can be coloured by
 * switching VDP_setTextPalette() before each draw. Index 0 transparent (the
 * black backdrop shows through); the font draws in index 15. */
#define PAL_WHITE   PAL1
#define PAL_RED     PAL2
#define PAL_YELLOW  PAL3

/* The exact credit string the owner shipped (home.c, owner request 2026-06-12).
 * Kept verbatim -- this is the load-bearing presentation requirement. */
#define CREDIT_STR  "RESTER159, 2026"

#define ROW_ARCADE  18
#define ROW_HOME    20
#define COLS        32   /* H40 text grid width */

/* ---- one-colour font palette: a flat colour in index 15, 0 transparent. */
static void set_font_pal(u16 palLine, u16 color)
{
    u16 pal[16];
    u16 i;
    for (i = 0; i < 16; i++) pal[i] = 0x0000;   /* transparent / black */
    pal[15] = color;                            /* the glyph colour    */
    PAL_setColors(palLine * 16, pal, 16, CPU);
}

/* ---- centred text on the active text plane (40-col grid). */
static void draw_center(u16 row, const char *s, u16 palLine)
{
    u16 len = 0;
    const char *p;
    for (p = s; *p; p++) len++;
    VDP_setTextPalette(palLine);
    VDP_drawTextBG(BG_A, s, (u16)((COLS - len) / 2), row);
}

/* ---- the SONSON title banner (rows 7-9). Drawn boldly with wide spacing +
 * a rule line so it reads as a title rather than a menu label. The native
 * baked arcade title tiles aren't usable here (they're keyed to the captured
 * gameplay scene's char set, not the title glyphs), so this is the
 * faithful-enough SGDK-font recreation the directive allows. */
static void draw_title(void)
{
    set_font_pal(PAL_WHITE,  0x0EEE);   /* bright white  */
    set_font_pal(PAL_RED,    0x000E);   /* red           */
    set_font_pal(PAL_YELLOW, 0x00EE);   /* yellow        */

    
    
}

/* ---- redraw the two choice rows with the current highlight. */
static void draw_choices(u8 sel)
{
    /* clear the two rows first (in case lengths differ) */
    VDP_clearTextArea(0, ROW_ARCADE, COLS, 1);
    VDP_clearTextArea(0, ROW_HOME,   COLS, 1);
    draw_center(ROW_ARCADE, "ARCADE", sel == MODE_ARCADE ? PAL_RED : PAL_WHITE);
    draw_center(ROW_HOME,   "HOME",   sel == MODE_HOME   ? PAL_RED : PAL_WHITE);
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

    arcade_menu_begin(1);
    VDP_clearTextArea(0,0,32,2);
    draw_title();

    draw_center(26, CREDIT_STR, PAL_WHITE);
    VDP_setTextPalette(PAL_WHITE);
    /* under the credit's end, as with the longer credit (cols 27-30) */
    VDP_drawTextBG(BG_A, "v1.0", (u16)((COLS - (sizeof CREDIT_STR - 1)) / 2 + (sizeof CREDIT_STR - 1) - 5), 27);
    edges_rearm();
    draw_choices(sel);

    for (;;)
    {
        wait_vblank();
        JOY_update();               /* engine off: refresh pads here          */
        u16 pressed = edges_read();
        if (settle < 20) { settle++; pressed = 0; }

        if (pressed & (BUTTON_UP | BUTTON_DOWN))
        {
            sel ^= 1;               /* two rows -> toggle (wraps)             */
            draw_choices(sel);
        }
        if (pressed & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START))
            break;
    }

    /* Clear our screen so game_init starts from black. */
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    return sel;
}

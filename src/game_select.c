/* Two-game cartridge chooser. Placeholder text presentation; the owner's
 * selection images replace draw_panels() once converted. */
#include <genesis.h>
#include "game_select.h"
#include "arcade_ui.h"

#define COLS        32
#define ROW_TITLE   6
#define ROW_FIRST   13
#define ROW_SECOND  16

static void set_font_pal(u16 line, u16 colour)
{
    u16 pal[16];
    u16 i;
    for (i = 0; i < 16; i++) pal[i] = 0;
    pal[15] = colour;
    PAL_setColors(line * 16, pal, 16, CPU);
}

static void draw_center(u16 row, const char *s, u16 line)
{
    u16 len = 0;
    const char *p;
    for (p = s; *p; p++) len++;
    VDP_setTextPalette(line);
    VDP_drawTextBG(BG_A, s, (u16)((COLS - len) / 2), row);
}

static void draw_panels(u8 sel)
{
    VDP_clearTextArea(0, ROW_FIRST, COLS, 1);
    VDP_clearTextArea(0, ROW_SECOND, COLS, 1);
    draw_center(ROW_FIRST,  sel == GAME_SONSON  ? "> SON SON <"    : "SON SON",    sel == GAME_SONSON  ? PAL2 : PAL1);
    draw_center(ROW_SECOND, sel == GAME_SONSON2 ? "> SON SON II <" : "SON SON II", sel == GAME_SONSON2 ? PAL2 : PAL1);
}

/* A pad bit must be stable for three reads before it can edge: the
 * 6-button detect handshake produces phantom presses at power-on (the same
 * rule the SonSon selector uses). */
static u16 raw1, raw2, stable_prev;

static u16 read_edges(void)
{
    u16 raw = (u16)(JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2));
    u16 stable = (u16)(raw & raw1 & raw2);
    u16 edges = (u16)(stable & ~stable_prev);
    raw2 = raw1;
    raw1 = raw;
    stable_prev = stable;
    return edges;
}

u8 game_select_run(void)
{
    u8 sel = GAME_SONSON;
    u16 e;
    u16 settle = 0;

    arcade_menu_begin(0);
    set_font_pal(PAL1, 0x0EEE);
    set_font_pal(PAL2, 0x00EE);
    set_font_pal(PAL3, 0x0AAA);

    draw_center(ROW_TITLE, "CAPCOM COLLECTION", PAL1);
    draw_center(ROW_TITLE + 2, "SELECT A GAME", PAL3);
    draw_panels(sel);
    draw_center(25, "PORTED BY RESTER159, 2026", PAL3);

    JOY_setSupport(PORT_1, JOY_SUPPORT_3BTN);
    JOY_setSupport(PORT_2, JOY_SUPPORT_3BTN);
    raw1 = raw2 = stable_prev = 0xFFFF;
    for (;;)
    {
        vu32 t = vtimer;
        while (vtimer == t) { }
        JOY_update();
        e = read_edges();
        if (settle < 20) { settle++; e = 0; }
        if (e & (BUTTON_UP | BUTTON_DOWN))
        {
            sel ^= 1;
            draw_panels(sel);
        }
        if (e & (BUTTON_START | BUTTON_A | BUTTON_C))
            break;
    }
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    return sel;
}

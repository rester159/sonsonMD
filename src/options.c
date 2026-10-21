/* Home options: arcade lettering, compact choices, independent continue credits. */
#include <genesis.h>
#include "arcade_ui.h"
#include "frontend.h"
#include "game.h"
#include "save.h"

#define PAL_WHITE   PAL1
#define PAL_RED     PAL2
#define PAL_YELLOW  PAL3

#define COLS        32

#define ROW_TOP     9            /* first option row                          */
#define ROW_STEP    2             /* rows between options                       */
#define NAME_COL    3            /* option name left column                   */
#define VAL_COL     19            /* option value left column                  */

/* ---- Home options, the separate continue-credit setting, and EXIT. Each non-EXIT row edits a DSW bitfield
 * (bank 1 = dsw2 except DEMO SOUNDS which is dsw1 0x20). Value labels are
 * indexed by the raw extracted bitfield value, identical ordering to the arcade
 * DIP screen and to the sibling's menu_label so the two screens agree. */
typedef struct
{
    const char *name;
    u8          bank;             /* 0 = dsw1, 1 = dsw2; 0xFF = EXIT (no field) */
    u8          mask;
    u8          shift;
    u8          count;
    const char *labels[4];
} OptRow;

static const OptRow OPTS[] =
{
    { "LIVES",       1, 0x03, 0, 4, { "7", "5", "4", "3" } },         /* idx3=3 */
    { "BONUS",       1, 0x18, 3, 4,
        { "30K 90K 120K", "20K 80K 100K", "30K ONLY", "20K ONLY" } },
    { "LEVEL",  1, 0x60, 5, 4, { "VERY HARD", "HARD", "NORMAL", "EASY" } },
    { "DEMO SOUND", 0, 0x20, 5, 2, { "ON", "OFF", 0, 0 } },          /* 0=On   */
    { "CREDITS",     2, 0,    0, 4, { "0", "3", "5", "9" } },
    { "RESET", 0xFD, 0, 0, 0, {0,0,0,0} },
    { "ERASE SCORES", 0xFE, 0, 0, 0, {0,0,0,0} },
    { "EXIT",     0xFF, 0,    0, 0, { 0, 0, 0, 0 } },
};
#define OPT_COUNT  (sizeof(OPTS) / sizeof(OPTS[0]))
#define ROW_EXIT   (OPT_COUNT - 1)

/* ---- shared selector-style helpers (font palette / text / debounce). ------ */
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

/* ---- option field access on the g_config raw banks. ----------------------- */
static u8 opt_get(const OptRow *o)
{
    if (o->bank == 2)
    {
        if (g_config.homeCredits == 0) return 0;
        if (g_config.homeCredits == 5) return 2;
        if (g_config.homeCredits == 9) return 3;
        return 1;
    }
    u8 b = (o->bank == 0) ? g_config.dsw1 : g_config.dsw2;
    return (u8)((b & o->mask) >> o->shift);
}

static void opt_cycle(const OptRow *o, s8 dir)
{
    u8 f = opt_get(o);
    if (dir > 0) f = (u8)((f + 1) % o->count);
    else         f = (u8)((f + o->count - 1) % o->count);
    u8 v = (u8)((f << o->shift) & o->mask);
    if (o->bank == 2)
    {
        static const u8 values[4] = {0, 3, 5, 9};
        g_config.homeCredits = values[f];
        return;
    }
    if (o->bank == 0)
        g_config.dsw1 = (u8)((g_config.dsw1 & (u8)~o->mask) | v);
    else
        g_config.dsw2 = (u8)((g_config.dsw2 & (u8)~o->mask) | v);
}

/* ---- the SONSON title banner (selector.c parity). ------------------------- */
static void draw_title(void)
{


    draw_center(4, "OPTIONS", PAL_WHITE);
}

/* ---- draw one option row (name + value, highlighted in red/yellow). ------- */
static void draw_opt(u16 idx, u8 highlighted)
{
    const OptRow *o = &OPTS[idx];
    u16 row = (u16)(ROW_TOP + idx * ROW_STEP);

    VDP_clearTextArea(0, row, COLS, 1);
    if (o->bank >= 0xFD)
    {
        /* EXIT is centred like a button */
        draw_center(row, idx == ROW_EXIT ? "RETURN" : o->name, highlighted ? PAL_RED : PAL_WHITE);
        return;
    }
    if (highlighted) draw_at(NAME_COL - 2, row, ">", PAL_RED);
    draw_at(NAME_COL, row, o->name, highlighted ? PAL_RED : PAL_WHITE);
    draw_at(VAL_COL, row, o->labels[opt_get(o)], highlighted ? PAL_YELLOW : PAL_WHITE);
}

static void draw_menu(u16 sel)
{
    u16 i;
    for (i = 0; i < OPT_COUNT; i++) draw_opt(i, (u8)(i == sel));
}

void options_run(void)
{
    u16 sel    = 0;
    u16 settle = 0;
    u16 pending = 0xFFFF;

    JOY_setSupport(PORT_1, JOY_SUPPORT_3BTN);
    JOY_setSupport(PORT_2, JOY_SUPPORT_3BTN);

    VDP_setTextPlane(BG_A);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setBackgroundColor(0);
    VDP_resetSprites();
    VDP_setSprite(0,0,0,SPRITE_SIZE(1,1),0);
    VDP_updateSprites(1,DMA);

    arcade_menu_begin(0);
    set_font_pal(PAL_WHITE,  0x0EEE);
    set_font_pal(PAL_RED,    0x000E);
    set_font_pal(PAL_YELLOW, 0x00EE);

    draw_title();

    draw_center(27, " ", PAL_YELLOW);

    edges_rearm();
    draw_menu(sel);

    for (;;)
    {
        wait_vblank();
        JOY_update();
        u16 pressed = edges_read();
        if (settle < 20) { settle++; pressed = 0; }

        if (pressed & BUTTON_UP)
            sel = (u16)((sel + OPT_COUNT - 1) % OPT_COUNT);
        else if (pressed & BUTTON_DOWN)
            sel = (u16)((sel + 1) % OPT_COUNT);

        /* Left/Right change the value of a value-bearing row (not EXIT). */
        if (sel < 5)
        {
            if (pressed & BUTTON_LEFT)  opt_cycle(&OPTS[sel], -1);
            if (pressed & BUTTON_RIGHT) opt_cycle(&OPTS[sel], +1);
        }

        if (pressed & (BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT))
        {
            pending=0xFFFF;
            draw_center(27," ",PAL_YELLOW);
            draw_menu(sel);
        }

        /* START anywhere confirms; A/B/C confirms only on the START GAME row
         * (so a button on a value row doesn't accidentally launch). This mirrors
         * home.c, where the value rows cycle on button and EXIT leaves. */
        if (pressed & BUTTON_START) break;
        if ((pressed & (BUTTON_A | BUTTON_B | BUTTON_C)) && sel == ROW_EXIT) break;
        /* button on a value row = cycle it forward (home.c parity) */
        if ((pressed & (BUTTON_A | BUTTON_B | BUTTON_C)) && sel >= 5 && sel != ROW_EXIT)
        {
            if(pending == sel)
            {
                if(OPTS[sel].bank == 0xFD)save_reset_options();else save_clear_scores();
                pending=0xFFFF;draw_menu(sel);draw_center(27,"DONE",PAL_YELLOW);
            }
            else {pending=sel;draw_center(27,"SURE?",PAL_RED);}
        }
        if ((pressed & (BUTTON_A | BUTTON_B | BUTTON_C)) && sel < 5)
        {
            opt_cycle(&OPTS[sel], +1);
            draw_menu(sel);
        }
    }

    sync_from_dsw();
    save_capture_settings(MODE_HOME);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
}

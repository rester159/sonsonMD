/* title.c -- the arcade SonSon TITLE screen + ATTRACT loop (native).
 *
 * Reproduces the arcade attract sequence (RE'd from the real machine, mame
 * sonson, tools/arc_attract.lua): the TITLE screen ("SONSON" logo + score/HIGH
 * header + "PLEASE DEPOSIT COIN" + flanking characters + "(c) CAPCOM 1984")
 * alternating with a self-playing DEMO. The title BG + sprites are baked
 * byte-faithfully from the arcade title frame (tools/gen_title.py -> ss_title.*).
 *
 * The arcade screen is 256x224 (H32), so the title runs in the game's H32 video
 * mode (the 32-col baked nametable fills the screen exactly). Returns when the
 * player buys a game with C/Start in Arcade or selects a game in Home; the
 * caller then starts a real game. While idle it runs the attract demo.
 */
#include <genesis.h>
#include "frontend.h"
#include "game.h"
#include "ss_title.h"
#include "mode_controls.h"
#include "survival.h"
#include "save.h"

/* Build a raw SAT word set from the baked title sprites and DMA it to the VDP
 * sprite list. Each SSSprite is a 16x16 (SPRITE_SIZE(2,2)); screen coords add the
 * +0x80 SAT bias. Parked arcade sprites (raw y == 248) resolve off-screen. */
static void title_sprites(void)
{
    u16 i;
    u16 sat[SS_TITLE_SPRITE_COUNT * 4];
    u16 n = 0;
    for (i = 0; i < SS_TITLE_SPRITE_COUNT; i++)
    {
        const SSSprite *s = &ss_title_sprites[i];
        /* title sprite y: undo the gameplay -24 playfield bias (+24) then apply the
         * same 1-row (8px) up-shift as the BG below -> +16. Skip parked slots. */
        s16 ty = (s16)(s->y + 16);
        if (s->y >= 200 || s->y < -16) continue;
        sat[n * 4 + 0] = (u16)((0x80 + ty) & 0x3FF);                  /* y        */
        sat[n * 4 + 1] = (u16)((SPRITE_SIZE(2, 2) << 8) | (n + 1));   /* size+link*/
        sat[n * 4 + 2] = (u16)(TILE_ATTR_FULL(PAL1, 1, s->flipV, s->flipH, s->tile));
        sat[n * 4 + 3] = (u16)((0x80 + s->x) & 0x1FF);               /* x        */
        n++;
    }
    if (n == 0) { VDP_setSprite(0, 0, 0, SPRITE_SIZE(1,1), 0); return; }
    sat[(n - 1) * 4 + 1] = (u16)((SPRITE_SIZE(2, 2) << 8) | 0);       /* link end */
    /* engine off in the front-end -> the DMA queue is never flushed; do it now. */
    DMA_doDma(DMA_VRAM, sat, VDP_getSpriteListAddress(), (u16)(n * 4), 2);
}

/* Render the baked title BG (logo + text + score header) to BG_A. */
static void title_high(void);
static void title_draw(void)
{
    u16 c, r;
    u16 colbuf[32];

    PAL_setColors(PAL0 * 16, ss_pal_char,   16, CPU);
    PAL_setColors(PAL1 * 16, ss_pal_sprite, 16, CPU);
    VDP_setBackgroundColor(0);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_clearPlane(BG_B, TRUE);

    VDP_loadTileData(ss_title_tiles, SS_TITLE_TILE_BASE, SS_TITLE_TILE_COUNT, DMA);

    /* 32x32 row-major nametable -> BG_A via column writes (the proven game path).
     * Shift UP by one row: the arcade title spans rows 1..30 (header..CREDIT) but
     * the native H32 shows only 28 rows, so map source row r+1 -> dest row r. That
     * fits the header (src 1-2) through the "(c) CAPCOM 1984" line (src 28) into the
     * visible area (CREDIT at src 30 falls just below the bottom edge). */
    for (c = 0; c < 32; c++)
    {
        for (r = 0; r < 32; r++)
            colbuf[r] = ss_title_nametable[(((r + 1) & 31)) * 32 + c];
        VDP_setTileMapDataColumnFast(BG_A, colbuf, c, 0, 32, DMA);
    }

    title_sprites();
    title_high();
}

#define TITLE_IDLE_FRAMES  900      /* ~15s on the title with no input -> attract demo */
#define GO_BTN  (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C)

/* ---- game-font text helpers (use the baked white/red arcade-font glyphs) ----
 * char -> arcade char code: digit 0-9 -> 0-9, A-Z -> 17+(ch-'A'), else space (16);
 * tile = SS_TITLE_GLYPH_{W,R} + code. */
static u16 glyph(char ch, u8 red)
{
    u8 code;
    if (ch >= '0' && ch <= '9')      code = (u8)(ch - '0');
    else if (ch >= 'A' && ch <= 'Z') code = (u8)(17 + (ch - 'A'));
    else                             code = 16;
    return (u16)((red ? SS_TITLE_GLYPH_R : SS_TITLE_GLYPH_W) + code);
}

static void clear_row(u16 row)
{
    const u16 blank = ss_title_nametable[0];
    u16 i;
    for (i = 0; i < 32; i++) VDP_setTileMapXY(BG_A, blank, i, row);
}

static void text_center(u16 row, const char *s, u8 red)
{
    u16 i, len = 0, start;
    for (i = 0; s[i]; i++) len++;
    start = (u16)((32 - len) / 2);
    clear_row(row);
    for (i = 0; s[i]; i++)
        if (s[i] != ' ')
            VDP_setTileMapXY(BG_A, (u16)TILE_ATTR_FULL(PAL0, 0, 0, 0, glyph(s[i], red)),
                             (u16)(start + i), row);
}

static void title_high(void)
{
    char digits[12]; u16 i;
    intToStr((s32)save_mode_high(g_mode), digits, 6);
    for (i = 0; i < 6; i++)
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0,0,0,0,glyph(digits[i],0)),12+i,1);
}

static void home_choices(u8 selected)
{
    static const char *const names[5] = {"1 PLAYER", "2 PLAYERS", "INSTADEATH 1P", "INSTADEATH 2P", "OPTIONS"};
    u8 i;
    for(i=0;i<5;i++) text_center((u16)(16+i*2),names[i],selected==i);
    if(selected==2 || selected==3)
    {
        u32 seconds=save_best_ticks(save_run_key(MODE_HOME,(selected&1)+1,1))/300;
        char label[]="BEST 000 00";char n[12];
        if(seconds>59999)seconds=59999;
        intToStr(seconds/60,n,3);label[5]=n[0];label[6]=n[1];label[7]=n[2];
        intToStr(seconds%60,n,2);label[9]=n[0];label[10]=n[1];
        text_center(12,label,0);
    }
    else clear_row(12);
}

/* ---- 3-read debounced edge reader (front-end has no engine input path) ------ */
static u16 e_r1 = 0xFFFF, e_r2 = 0xFFFF, e_prev = 0xFFFF;
static void edges_rearm(void) { e_r1 = e_r2 = e_prev = 0xFFFF; }
static u16  edges_read(void)
{
    u16 raw     = (u16)(JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2));
    u16 stable  = (u16)(raw & e_r1 & e_r2);
    u16 pressed = (u16)(stable & (u16)~e_prev);
    e_r2 = e_r1; e_r1 = raw; e_prev = stable;
    return pressed;
}

/* Run the self-playing attract demo (game_run with the autopilot). Returns TRUE
 * if the player pressed a button during it (or it ended) -- the caller decides
 * what that means per mode. */
static u8 run_demo(void)
{
    g_attract = 1;
    g_attract_started = 0;
    game_run();
    g_attract = 0;
    return g_attract_started;
}

/* C deposits a coin; each controller's Start buys its own player. */
static void arcade_attract(void)
{
    for (;;)
    {
        u16 settle = 0;
        u32 idle = 0;
        u8 shown = 255;
        title_draw();
        clear_row(24);
        clear_row(25);
        mode_input_rearm();
        for (;;)
        {
            u8 starts, selected = 0;
            VDP_waitVSync(); JOY_update();
            starts = mode_input_poll();
            if (shown != g_arcadeCredits)
            {
                char line[] = "CREDIT 00";
                shown = g_arcadeCredits;
                line[7] = (char)('0' + shown / 10);
                line[8] = (char)('0' + shown % 10);
                text_center(26, (g_config.dsw1 & 15) ? line : "FREE PLAY", 0);
                idle = 0;
            }
            if (settle < 20) { settle++; continue; }
            if (starts == 3 && (g_config.dsw2 & 4))
            {
                if (arcade_take_credit()) selected = 3;
            }
            else
            {
                if ((starts & 1) && arcade_take_credit()) selected |= 1;
                if ((starts & 2) && arcade_take_credit()) selected |= 2;
            }
            if (selected)
            {
                g_survivalChoice = 0;
                g_startPlayers = selected;
                g_numPlayers = (selected & 2) ? 2 : 1;
                return;
            }
            if (starts) idle = 0;
            if (++idle >= TITLE_IDLE_FRAMES) break;
        }
        (void)run_demo();
    }
}

/* HOME attract (follows the deprecated port's console front-end, ../sonson home.c):
 * a menu over the SONSON title -- 1 PLAYER / 2 PLAYERS / OPTIONS, up/down to move,
 * any button selects. 1P/2P begin a game; OPTIONS opens the console options then
 * returns to the menu. Idle -> attract demo; any button during the demo returns to
 * the menu (NOT a game). The arcade deposit-coin line is scrubbed (no coin slot).
 * Both players run simultaneously in a two-player game. */
static void home_attract(void)
{
    u8 sel = 0;
    for (;;)
    {
        u16 settle = 0;
        u32 idle = 0;
        u8  toDemo = 0;

        title_draw();
        clear_row(12);                          /* scrub the deposit-coin line     */
        home_choices(sel);
        edges_rearm();

        for (;;)
        {
            u16 pressed;
            VDP_waitVSync(); JOY_update();
            pressed = edges_read();
            if (settle < 20) { settle++; continue; }

            if (pressed & BUTTON_UP)   { sel = (u8)((sel + 4) % 5); idle = 0; }
            if (pressed & BUTTON_DOWN) { sel = (u8)((sel + 1) % 5); idle = 0; }
            if (pressed & (BUTTON_UP | BUTTON_DOWN))
            {
                home_choices(sel);
            }

            if (pressed & GO_BTN)
            {
                if (sel < 4)                   /* 1 PLAYER / 2 PLAYERS -> game     */
                {
                    g_survivalChoice = (u8)(sel >= 2);
                    g_numPlayers = (u8)((sel & 1) ? 2 : 1);
                    g_startPlayers = (sel & 1) ? 3 : 1;
                    return;
                }
                /* OPTIONS: the console options screen runs in the front-end H40 text
                 * mode; switch out of the game's H32, run it, switch back, redraw.  */
                VDP_setScreenWidth320(); VDP_setPlaneSize(64, 32, TRUE);
                options_run();
                VDP_setScreenWidth256(); VDP_setPlaneSize(64, 32, TRUE);
                break;                          /* -> redraw title + menu           */
            }
            if (++idle >= TITLE_IDLE_FRAMES) { toDemo = 1; break; }
        }

        /* demo: any button (or its end) just returns to the home menu */
        if (toDemo) (void)run_demo();
    }
}

void title_attract_run(u8 mode)
{
    /* 3-button mode (kill the 6-button TH phantom presses, as the selector does). */
    JOY_setSupport(PORT_1, JOY_SUPPORT_3BTN);
    JOY_setSupport(PORT_2, JOY_SUPPORT_3BTN);

    if (mode == MODE_HOME) home_attract();
    else                   arcade_attract();
}

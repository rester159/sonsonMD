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

#define BG_TILE      TILE_USER_INDEX
#define HUD_TILE     (BG_TILE + SS2_S0_NTILES)
#define PLAYER_TILE  (HUD_TILE + SS2_HUD_NTILES)

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
 * PCE line g + 8. Stage 1 opens at cam (0, $700). */
#define START_X      0
#define START_Y      (0x700 + 61)

static s16 camx, camy;
static s16 drawn_tx, drawn_ty;
static u16 linebuf[64];

static u16 world_tile(u16 tx, u16 ty)
{
    u8 m;
    tx &= 255;
    ty &= 255;
    m = ss2_s0_map[(((ty >> 5) << 3) | (tx >> 5)) * 64 + ((ty >> 2) & 7) * 8 + ((tx >> 2) & 7)];
    return (u16)(ss2_s0_meta[m * 16 + (ty & 3) * 4 + (tx & 3)] + BG_TILE);
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

static void apply_scroll(void)
{
    VDP_setHorizontalScroll(BG_A, (s16)-camx);
    VDP_setVerticalScroll(BG_A, (s16)(camy - VIEW_TOP));
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
    VDP_setBackgroundColor(0);
    VDP_resetSprites();
    VDP_updateSprites(1, CPU);
    PAL_setColors(0, ss2_s0_palette, 64, CPU);
    VDP_loadTileData(ss2_s0_tiles, BG_TILE, SS2_S0_NTILES, CPU);
    VDP_loadTileData(ss2_hud_tiles, HUD_TILE, SS2_HUD_NTILES, CPU);
    VDP_setTileMapDataRectEx(WINDOW, ss2_hud_map, HUD_TILE, 0, 0, 32, HUD_ROWS, 32, CPU);
    VDP_setWindowOnTop(HUD_ROWS);
    VDP_setEnable(TRUE);
}

static void video_shutdown(void)
{
    VDP_setEnable(FALSE);
    VDP_setWindowOff();
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(WINDOW, TRUE);
    VDP_resetSprites();
    VDP_updateSprites(1, CPU);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_A, 0);
    PAL_setColors(0, palette_black, 64, CPU);
    VDP_setEnable(TRUE);
}

/* Player stand-in until the bank-9 player logic is rebuilt: the camera follows
 * the pad and Goku's walk cycle (anim set 0) plays while moving. */
static u8 shown_frame = 0xFF;

static void show_player(s16 x, s16 y, u8 frame, u8 attr)
{
    if (frame != shown_frame)
    {
        VDP_loadTileData(ss2_player_tiles + (u16)frame * 128, PLAYER_TILE, 16, DMA_QUEUE);
        shown_frame = frame;
    }
    VDP_setSpriteFull(0, x, y, SPRITE_SIZE(4, 4),
                      TILE_ATTR_FULL(SS2_PLAYER_PAL, 1, 0, (attr & 0x08) ? 1 : 0, PLAYER_TILE), 0);
    VDP_updateSprites(1, DMA_QUEUE);
}

void ss2_run(void)
{
    u16 pad, step = 0;
    u8 state = 2, frame, attr;
    const u8 *a;

    video_init();
    camx = START_X;
    camy = START_Y;
    redraw_view();
    apply_scroll();

    for (;;)
    {
        SYS_doVBlankProcess();
        pad = JOY_readJoypad(JOY_1);
        if ((pad & (BUTTON_START | BUTTON_A | BUTTON_C)) == (BUTTON_START | BUTTON_A | BUTTON_C))
            break;
        if (pad & BUTTON_RIGHT) { camx += 2; state = 4; }
        else if (pad & BUTTON_LEFT) { camx -= 2; state = 0; }
        else state = (state == 0) ? 1 : (state == 4 ? 2 : state);
        if (pad & BUTTON_UP) camy -= 2;
        if (pad & BUTTON_DOWN) camy += 2;
        camx &= 2047;
        camy &= 2047;
        stream_view();
        apply_scroll();
        step++;
        a = &ss2_player_anim[((0 * 6 + state) * 4 + ((step >> 3) & 3)) * 2];
        frame = a[0];
        attr = a[1];
        if (frame != 0xFF)
            show_player(112, VIEW_TOP + 104, frame, attr);
    }
    video_shutdown();
}

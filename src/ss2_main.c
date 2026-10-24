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

#define BG_TILE      TILE_USER_INDEX
#define HUD_TILE     (BG_TILE + SS2_S0_NTILES)
#define PLAYER_TILE  (HUD_TILE + SS2_HUD_NTILES)
#define SPR_TILE     (PLAYER_TILE + 16)

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
    VDP_loadTileData(ss2_spr_tiles, SPR_TILE, SS2_SPR_NTILES, CPU);
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
        VDP_loadTileData(ss2_player_tiles + (u16)frame * 128, PLAYER_TILE, 16, DMA_QUEUE);
        shown_frame = frame;
    }
    put_sprite(x, y, SPRITE_SIZE(4, 4), TILE_ATTR_FULL(SS2_PLAYER_PAL, 1, 0, (attr & 0x0800) ? 1 : 0, PLAYER_TILE));
}

static void draw_sprites(void)
{
    u8 i, r;
    nspr = 0;
    for (i = 0; i < 64; i++)
    {
        const u16 *e = &ss2_sat[i * 4];
        s16 sx, sy;
        u16 attr = e[3], cell = (u16)((e[2] >> 1) & 0x3FF);
        u8 cw, ch, pal, line;
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
        line = ss2_spr_line[pal];
        for (r = 0; r < ch; r++)
        {
            u16 c = (u16)(cell + r * 2), t = ss2_spr_lut[pal * 256 + (c >> 1)];
            s16 y;
            if (!t || line > 3) { ss2_sprite_misses++; continue; }
            t = (u16)(SPR_TILE + t - 1);
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
        VDP_setTileMapXY(WINDOW, (u16)(ss2_hud_digit[R(src + i) % 10] + HUD_TILE), (u16)(col + i), row);
}

/* bank 2 $76B3: bar of max/2 cells, shown/2 full cells, an odd half cell */
static void hud_bar(const u8 *v, u16 table, u16 row)
{
    u8 x = 0, max = (u8)(v[1] >> 1), full = (u8)(v[0] >> 1);
    for (; x < full; x++)
        VDP_setTileMapXY(WINDOW, (u16)(ss2_hud_bar[ss2_rom_byte((u16)(table + x)) & 63] + HUD_TILE), (u16)(7 + x), row);
    if (v[0] & 1)
    {
        VDP_setTileMapXY(WINDOW, (u16)(ss2_hud_bar[(ss2_rom_byte((u16)(table + x)) + 1) & 63] + HUD_TILE), (u16)(7 + x), row);
        x++;
    }
    for (; x < max; x++)
        VDP_setTileMapXY(WINDOW, (u16)(ss2_hud_bar[0x3C] + HUD_TILE), (u16)(7 + x), row);
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

void ss2_run(void)
{
    u16 pad;
    u16 i;

    /* GCC 16 compiles a plain copy loop (and the inlined LTO memcpy) into
     * move.b (a0)+,(0,a0,d1.l), which stores one byte too high on the 68000
     * (tools/check_autoinc_hazard.py). The empty asm keeps load and store as
     * separate instructions. */
    {
        const u32 *src = (const u32 *) ss2_s0_ram;
        u32 *dst = (u32 *) ss2_ram;
        u16 i;
        for (i = 0; i < 0x2000 / 4; i++)
        {
            u32 v = src[i];
            __asm__ volatile ("" : "+d" (v));
            dst[i] = v;
        }
    }
    ss2_mpr6 = 9;
    ss2_page[6] = ss2_banks[9];
    shown_frame = 0xFF;
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
        ss2_game_tick();
        camera_from_state();
        stream_view();
        apply_scroll();
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

/* SON SON II ending, rebuilt from HuCard bank 4 $AD5A-$B4D2 (entered from
 * the stage clear $F617 when stage 7 is cleared), with bank 2 $72DD (a
 * sprite fade) and $7A9F (the ending picture's blocks).
 *
 * Three parts, each a slot-0 task step as on the HuCard:
 *   1. ($AD5A) the stage-7 map at camera ($200,$160) with the SCORE / ZENNY
 *      text: the zenny left is counted into the score (1 zenny = 100
 *      points, 8 per frame), Son Son walks in, two messages;
 *   2. ($AF2B) the ending picture ($7A9F blocks, $FBB1 map $B7F8) with
 *      Son Son and friends, falling petals (objects, handler $B3E9), a
 *      message, the picture scrolling up and the ending text scrolling in a
 *      raster-split band below it (zp $6E = $88: the VBlank IRQ takes the
 *      top band's scroll from the camera $45/$46, $48/$49; the RCR IRQ at
 *      line $E0 the band's from $51/$52, $54/$55);
 *   3. ($B1B2) the credits: seven pages of BAT text whose font (tiles
 *      $1C0-$1DF, from bank $18 $C600) slides into its cells ($B2C1) and
 *      out again ($B387); the last page (THE END) stays.
 *
 * BAT/VRAM work is presentation: map writes go to ss2_bat_queue, tile
 * loads are RAM side only, and the font slide is published in
 * ss2_end_font_dx for the renderer. RAM effects and waits follow the
 * source.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"
#include "ss2_task.h"

u8 ss2_ending_part;                          /* 0 none, 1 tally, 2 picture, 3 credits */
s8 ss2_end_font_dx = -8;                     /* credits font: glyph x offset in its cell
                                                (-8 / 8 = blank, 0 = in place) */

static void zero(u16 a, u16 n)
{
    while (n--) R(a++) = 0;
}

static u8 upd(u8 x)
{
    ss2_obj_camera();
    ss2_obj_next = x;
    ss2_obj_update(x);
    return ss2_obj_next;
}

/* objects 0..n-1 as "CLX / JSR $EE01 / INX / JSR $EE01 ..." */
static void upd_n(u8 n)
{
    u8 x = upd(0);
    while (--n) x = upd((u8)(x + 1));
}

static void seconds(u8 n)                    /* $B9D8 / $B9E4 */
{
    while (n--) ss2_wait(0x3C);
}

static void set_script(u8 x, u16 p)
{
    O(0x295A, x) = (u8)p;
    O(0x293A, x) = (u8)(p >> 8);
}

/* $BAE5: the camera's BAT address */
static u16 cam_base(void)
{
    u16 b = (u16)(((((Z_CAMX_HI & 1) << 8) | Z_CAMX_LO) >> 3) & 0x3F);
    return (u16)(b + ((Z_CAMY_LO & 0xF8) << 3));
}

static void put(u16 a, u16 w)
{
    ss2_bat_put((u8)(a & 63), (u8)((a >> 6) & 63), w);
}

/* $B46C / $B48D: score ($2203) / zenny ($220B), 8 digits, leading zeros
 * blank, at the camera's BAT address + off (palette 1) */
static void digits(u16 off, u16 src)
{
    u16 a = (u16)(cam_base() + off);
    u8 y = 0;
    for (; y < 8 && !R(src + y); y++) put((u16)(a + y), 0x11A0);
    for (; y < 8; y++) put((u16)(a + y), (u16)(0x11B0 + R(src + y)));
}

static void kill_task(u8 n)                  /* $E1F3 */
{
    ZP(4 * n) = 0;
    ZP(4 * n + 1) = 0;
}

static void message(u8 col, u8 row, u16 text)
{
    ZP(0x8F) = col;
    ZP(0x91) = row;
    ss2_msg_open();                          /* $6997 */
    ss2_wait(30);                            /* $B9D4 */
    ZP(0x93) = (u8)text; ZP(0x94) = (u8)(text >> 8);
    ss2_msg_text();                          /* $69EE */
}

/* bank 2 $7A9F: the ending picture's blocks (bank $1E $DE00 -> VRAM
 * $2800, bank $1F $C5A0 x $14 -> $2820); MPR6 back to $8E */
static void blocks_ending(void)
{
    R(0x2FA0) = 0;
    ZP(0x8B) = 0x02;
    ss2_page[6] = ss2_banks[ZP(0x8E) & 0x1F];
    ss2_mpr6 = ZP(0x8E);
    ss2_scene = SS2_SCENE_ENDING;
}

/* ---- the petals ---------------------------------------------------------- */

/* $B3E9 (object handler): fall faster each frame; gone below slot 0 */
void ss2_end_petal(u8 x)
{
    u16 t = (u16)(O(0x261A, x) + 0x18);
    O(0x261A, x) = (u8)t;
    O(0x25FA, x) = (u8)(O(0x25FA, x) + (t >> 8));
    if (!O(0x24DA, x) && R(0x24FA) < O(0x24FA, x))
    {
        O(0x245A, x) = 0;
        return;
    }
    ss2_obj_common(x);                       /* JMP $EE67 */
}

/* $B414: a new petal in slot x above the screen, drifting from slot 5's or
 * slot 6's x */
static void petal_new(u8 x)
{
    ss2_obj_init(x, 0xB6B8);
    if (ZP(0x8A))
    {
        set_script(x, 0xB6F9);
        O(0x255A, x) = 0x84;
    }
    O(0x24FA, x) = 0xF0;
    O(0x24DA, x) = 0xFF;
    O(0x261A, x) = 0;
    O(0x25FA, x) = 0;
    O(0x25DA, x) = ZP(0x8D);
    ss2_random();                            /* $FDB4 */
    if (!(ZP(0x8D) & 1))
    {
        O(0x249A, x) = R(0x24A0);
        O(0x247A, x) = 0;
        O(0x25BA, x) = 0xFF;
    }
    else
    {
        O(0x249A, x) = R(0x249F);
        O(0x247A, x) = 0;
        O(0x25BA, x) = 0;
    }
}

/* $B3D1: every 8th step a petal ($B410: the top free slot, $8436), then
 * every live object */
static void scene_step(void)
{
    u8 x;
    if (!(ZP(0x37) & 7))
    {
        x = ss2_obj_alloc_top();
        if (x != 0xFF) petal_new(x);
    }
    for (x = 0; x < 0x20; x++)
        if (O(0x245A, x)) x = upd(x);
}

/* one frame of the picture scene, the step count $37 kept on the stack */
static void scene_frame(void)
{
    u8 c = ZP(0x37);
    scene_step();
    ss2_sat_update();                        /* $8732 */
    ss2_wait(1);                             /* $B9B8 */
    ZP(0x37) = (u8)(c + 1);
}

static void scene_frames(u16 n)
{
    while (n--) scene_frame();
}

/* ---- the credits font ------------------------------------------------- */

/* $B2C1: the 32 glyphs (bank $18 $C600, 3bpp) written to VRAM $1C00 in 8
 * steps of 3 frames: step k (mask $E0 = 2^k-1, shift $E1 = 8-k) shows the
 * glyph's right k columns at the cell's left edge */
static void font_in(void)
{
    ZP(0xE0) = 1;
    ZP(0xE1) = 7;
    for (;;)
    {
        ss2_map6(0x18);                      /* $BB83 */
        ZP(0x8B) = 0x02;                     /* MAWR $1C00, then VWR */
        ss2_end_font_dx = (s8)(0 - ZP(0xE1));
        if (ZP(0xE1) == 7) ss2_busy(1);      /* the first (7-bit shift) pass starts
                                                after the page's text: it crosses a
                                                VBlank (measured) */
        ss2_wait(3);                         /* $B9C0 */
        ZP(0xE0) = (u8)((ZP(0xE0) << 1) | 1);
        if ((s8)--ZP(0xE1) < 0) break;
    }
}

/* $B387: VRAM $1C10-$1DAF (glyphs A-Z) shifted right a pixel, 8 times,
 * every 2 frames */
static void font_out(void)
{
    u8 x;
    for (x = 8; x; x--)
    {
        ZP(0x8B) = 0x02;                     /* MARR, MAWR, then VRR/VWR ($B3A6) */
        ss2_end_font_dx = (s8)(9 - x);
        ss2_wait(2);                         /* $B9BC */
    }
}

/* ---- slot 0: the ending ($AD5A) --------------------------------------- */

void ss2_ending_task(void)
{
    u8 i;
    ss2_ending_part = 1;
    kill_task(2);
    ss2_sfx(0xF0);                           /* $BBEB */
    ss2_fade_out_all();                      /* $7297 */
    ZP(0x8B) = 0x02;                         /* $BAAF */
    ss2_sprset_5548();                       /* $77D0 */
    ss2_busy(3);                             /* (measured) */
    zero(0x245A, 0x700);
    zero(0x2297, 0x1B0);
    ZP(0x45) = ZP(0x4B) = 2;
    ZP(0x46) = ZP(0x4C) = 0;
    ZP(0x48) = ZP(0x4E) = 1;
    ZP(0x49) = ZP(0x4F) = 0x60;
    ss2_redraw_request = 1;                  /* $EBE1 */
    ss2_busy(5);
    ZP(0x8B) = 0x02;
    ss2_page[6] = ss2_banks[ZP(0x8E) & 0x1F];
    ss2_mpr6 = ZP(0x8E);
    ss2_bat_text(0xB753);                    /* SCORE / ZENNY */
    digits(0x020F, 0x2203);
    digits(0x028F, 0x220B);
    ss2_fade_in_all();                       /* $7274 */
    for (i = 0; i < 0x80; i++) ss2_vce[0x180 + i] = 0;   /* sprite palettes 8-15 black */
    ss2_vce_dirty = 1;
    seconds(1);
    /* Son Son walks in from the left; his friend (slot 1) waits */
    ss2_obj_init(0, 0xB64A);
    O(0x249A, 0) = 0xE0; O(0x247A, 0) = 0xFF;
    O(0x24FA, 0) = 0xA0; O(0x24DA, 0) = 0;
    O(0x25DA, 0) = 0x40; O(0x25BA, 0) = 0;
    ss2_obj_init(1, 0xB660);
    O(0x249A, 1) = 0x80; O(0x247A, 1) = 0;
    O(0x24FA, 1) = 0xA0; O(0x24DA, 1) = 0;
    ss2_sfx(0x1B);
    ZP(0x36) = 0;
    for (;;)                                 /* $AE19: zenny -> score */
    {
        u8 c, z = 0;
        digits(0x020F, 0x2203);
        digits(0x028F, 0x220B);
        for (i = 0; i < 8; i++) z |= R(0x220B + i);
        if (!z) break;
        ZP(0x36)++;
        c = 0;
        for (i = 8; i--; )
        {
            u8 a = (u8)(ss2_rom_byte((u16)(0x4BFC + i)) + R(0x2203 + i) + c);
            c = (u8)(a >= 10);
            if (c) a = (u8)(a - 10);
            R(0x2203 + i) = a;
        }
        ss2_money_sub(0x4BEC);               /* $BA52 */
        if (ZP(0x36) & 7) continue;
        c = ZP(0x36);
        upd_n(2);
        ss2_sat_update();
        if (R(0x249A) == 0x60)
        {
            R(0x25BA) = 0; R(0x25DA) = 0;
            R(0x267A) = 1;
            set_script(0, 0xB6D9);
        }
        ZP(0x36) = c;
        ss2_wait(1);
    }
    put((u16)(cam_base() + 0x296), 0x11B0);  /* zenny 0 */
    ss2_sfx(0xF1);                           /* $BBEF */
    for (;;)                                 /* $AEB6 */
    {
        upd_n(2);
        ss2_sat_update();
        if (R(0x249A) == 0x60) break;
        ss2_wait(1);
    }
    R(0x25BA) = 0; R(0x25DA) = 0;
    R(0x267A) = 1;
    set_script(0, 0xB6D9);
    upd(0);
    ss2_sat_update();
    ss2_score_award(0);                      /* $BB89 */
    seconds(1);
    kill_task(3);                            /* the score task */
    message(4, 0x0C, 0x612E);
    R(0x255B) = 0x88;
    ss2_sat_update();
    ss2_fade_in_spr_alt();                   /* bank 2 $72DD */
    ss2_pal_intro = 1;                       /* sprite palettes 8-15 = $4A6C */
    ss2_wait(30);
    ZP(0x93) = 0x44; ZP(0x94) = 0x61;
    ss2_msg_text();
    seconds(1);

    /* ---- the picture ($AF2B) ---- */
    ss2_bg_off();
    ss2_spr_off();
    ss2_cr_set((u8)(R(0x2201) & 0xFB));      /* raster split off */
    ZP(0x6C) = 0;
    ss2_fade_out_all();                      /* $7297 */
    ss2_bat_clear();                         /* $BA91 */
    blocks_ending();                         /* $7A9F */
    ss2_ending_part = 2;
    zero(0x2045, 0x12);
    ZP(0x6E) = 0x88;
    seconds(1);
    ss2_spr_on();
    ss2_bg_on();
    ss2_bat_picture(0x0000, 0xB7F8, 0x20, 0x1C);    /* $FBB1 */
    R(0x267B) = 1;
    set_script(1, 0xB6E3);
    ss2_obj_init(2, 0xB676);
    O(0x249A, 2) = 0x40; O(0x247A, 2) = 0; O(0x24FA, 2) = 0xA0; O(0x24DA, 2) = 0;
    ss2_obj_init(3, 0xB68C);
    O(0x249A, 3) = 0x20; O(0x247A, 3) = 0; O(0x24FA, 3) = 0xA0; O(0x24DA, 3) = 0;
    ss2_obj_init(4, 0xB6A2);
    O(0x249A, 4) = 0xCB; O(0x247A, 4) = 0; O(0x24FA, 4) = 0xA0; O(0x24DA, 4) = 0;
    if (ZP(0x8A))
    {
        set_script(4, 0x68F3);
        O(0x27DA, 4) = 0xE0; O(0x27BA, 4) = 0x2D;
        O(0x255A, 4) = 0x8F;
        O(0x24FA, 4) = 0x98;
    }
    ss2_obj_init(5, 0xB6B8);
    ss2_obj_init(6, 0xB6B8);
    R(0x24FF) = R(0x2500) = 0xF0;
    R(0x24DF) = R(0x24E0) = 0xFF;
    R(0x249F) = 0x20; R(0x247F) = 0;
    R(0x24A0) = 0x80; R(0x2480) = 0;
    if (ZP(0x8A))
    {
        set_script(5, 0xB6F9);
        set_script(6, 0xB6F9);
        R(0x255F) = R(0x2560) = 0x84;
    }
    upd_n(5);
    ss2_sat_update();
    seconds(1);
    ss2_fade_in_title();                     /* $722E */
    seconds(1);
    message(4, 9, 0x618D);
    seconds(1);
    ss2_sfx(0x10);                           /* ending music */
    ss2_bat_picture(0x0000, 0xB7F8, 0x20, 0x1C);    /* (the window gone) */
    ss2_bat_text(0xB787);                    /* the ending text, BAT rows 29- */
    ZP(0x37) = 0;
    scene_frames(0x400);
    for (;;)                                 /* $B0F3: the picture scrolls up */
    {
        u8 c = ZP(0x37);
        u16 t = (u16)(ZP(0x4A) + 0x40);
        ZP(0x4A) = (u8)t;
        ZP(0x49) = (u8)(ZP(0x49) + (t >> 8));
        scene_step();
        ss2_sat_update();
        ss2_wait(1);
        ZP(0x4E) = ZP(0x48);
        ZP(0x4F) = ZP(0x49);
        if (ZP(0x49) == 0x40) break;         /* (the count stays on the stack) */
        ZP(0x37) = (u8)(c + 1);
    }
    ZP(0x37) = 0;                            /* as $8732's piece loop leaves it */
    scene_frames(0x400);
    zero(0x2051, 6);
    {
        u16 t = (u16)(ZP(0x55) + 0xE0);
        ZP(0x55) = (u8)t;
        ZP(0x54) = (u8)(ZP(0x54) + (t >> 8));
    }
    ZP(0x8B) = 0x06;                         /* RCR = $E0 */
    ss2_cr_set((u8)(R(0x2201) | 0x04));      /* raster split on */
    ZP(0x6D) = 0x88;
    do                                       /* $B174: the text band scrolls */
    {
        u8 c = ZP(0x37);
        u16 t = (u16)(ZP(0x56) + 0x40);
        ZP(0x56) = (u8)t;
        t = (u16)(ZP(0x55) + (t >> 8));
        ZP(0x55) = (u8)t;
        ZP(0x54) = (u8)(ZP(0x54) + (t >> 8));
        scene_step();
        ss2_sat_update();
        ss2_wait(1);
        ZP(0x37) = (u8)(c + 1);
    } while (ZP(0x55) != 0x5A);
    scene_frames(0x100);
    ss2_fade_out_title();                    /* $7251 */
    seconds(1);

    /* ---- the credits ($B1B8) ---- */
    ZP(0x6D) = 0;
    ss2_cr_set((u8)(R(0x2201) & 0xFB));      /* raster split off */
    zero(0x2045, 6);
    ss2_bat_clear();
    ZP(0x8B) = 0x02;                         /* $BAAF */
    ss2_vce_reset();                         /* $FF46 */
    ss2_ending_part = 3;
    ss2_end_font_dx = -8;                    /* VRAM $1C10-$1DAF cleared */
    ZP(0x8B) = 0x02;
    ss2_sfx(0x13);
    {
        static const u16 page[6] = { 0xB4D3, 0xB506, 0xB538, 0xB58B, 0xB5BA, 0xB5EB };
        for (i = 0; i < 6; i++)
        {
            ss2_bat_text(page[i]);
            font_in();
            seconds(7);
            font_out();
            seconds(1);
            ss2_bat_clear();
        }
    }
    ss2_bat_text(0xB60E);                    /* THE END / PRESENTED BY NEC AVENUE */
    font_in();
    for (;;) ss2_wait(1);                    /* $B2BB */
}

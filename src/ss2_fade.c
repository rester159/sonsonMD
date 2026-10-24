/* SON SON II palette fades, rebuilt from HuCard bank 2 $71BA-$75F9.
 *
 * A fade step recomputes palettes from their ROM sources at brightness
 * level 0-7 (component table $75FA) into the RAM palette shadow
 * $2EA0-$2F9F (16 palettes x 8 colours) and uploads ranges of it to the
 * VCE. Uploads land in ss2_vce[], a native mirror of the PCE colour RAM
 * from which the renderer derives Genesis CRAM.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"
#include "ss2_task.h"

u16 ss2_vce[512];
u8 ss2_vce_dirty;

/* $7396: count bytes of colours from src, faded to level, into dst */
static void fade_block(u16 src, u16 dst, u8 count, u8 level)
{
    u8 i;
    ZP(0x37) = count;
    for (i = 0; i != count; i = (u8)(i + 2))
    {
        u8 lo = ss2_rom_byte((u16)(src + i)), hi = ss2_rom_byte((u16)(src + i + 1));
        u8 b = (u8)(lo & 7), r = (u8)((lo >> 3) & 7);
        u8 g = (u8)(((hi & 1) << 2) | (lo >> 6));
        b = ss2_rom_byte((u16)(0x75FA + (u8)((level << 3) | b)));
        r = ss2_rom_byte((u16)(0x75FA + (u8)((level << 3) | r)));
        g = ss2_rom_byte((u16)(0x75FA + (u8)((level << 3) | g)));
        R(dst + i) = (u8)(((g & 3) << 6) | (r << 3) | b);
        R(dst + i + 1) = (u8)(g >> 2);
        ZP(0x36) = (u8)(i + 2);
    }
}

static u16 stage_ptr(u16 table)
{
    u8 y = (u8)(Z_STAGE << 1);
    return (u16)(ss2_rom_byte((u16)(table + y)) | (ss2_rom_byte((u16)(table + y + 1)) << 8));
}

static void f72F1(u8 l) { fade_block(0x4000, 0x2EA0, 0x50, l); }             /* BG 0-4 common   */
static void f7308(u8 l) { fade_block(stage_ptr(0x40D0), 0x2EF0, 0xB0, l); }  /* BG 5-15 stage   */
static void f7324(u8 l) { fade_block(0x49BC, 0x2EF0, 0xB0, l); }             /* BG 5-15 alt     */
static void f733A(u8 l) { fade_block(0x4050, 0x2EA0, 0x10, l); }             /* sprite 0        */
static void f7350(u8 l) { fade_block(0x4060, 0x2EB0, 0x70, l); }             /* sprite 1-7      */
static void f7366(u8 l) { fade_block(stage_ptr(0x45AE), 0x2F20, 0x80, l); }  /* sprite 8-15     */

/* VCE uploads of shadow palettes first..15 (8 colours each) */
static void upload(u16 vce_base, u8 first)
{
    u8 p, c;
    for (p = first; p < 16; p++)
        for (c = 0; c < 8; c++)
        {
            u16 a = (u16)(0x2EA0 + p * 16 + c * 2);
            ss2_vce[vce_base + p * 16 + c] = (u16)(R(a) | ((R(a + 1) & 1) << 8));
        }
    ss2_vce_dirty = 1;
}

static void up_bg(void)       { upload(0x000, 0); }    /* $73FC */
static void up_bg_stage(void) { upload(0x000, 5); }    /* $7445 */
static void up_sp(void)       { upload(0x100, 0); }    /* $74EB */
static void up_sp_stage(void) { upload(0x100, 1); }    /* $74FA */

/* $71BA / $71D7: stage palettes in (levels 1-7) / out (6-0) */
static void fade_stage_step(u8 l)
{
    f7308(l);
    ss2_wait(2);
    up_bg_stage();
    f7350(l);
    f7366(l);
    ss2_wait(1);
    up_sp_stage();
}

void ss2_fade_in(void)
{
    u8 l;
    for (l = 1; l != 8; l++) fade_stage_step(l);
}

void ss2_fade_out(void)
{
    s8 l;
    for (l = 6; l != -1; l--) fade_stage_step((u8)l);
}

/* $7274 / $7297: every palette, used while $64 (interior scroll mode) */
static void fade_all_step(u8 l)
{
    f72F1(l);
    f7308(l);
    ss2_wait(2);
    up_bg();
    f733A(l);
    f7350(l);
    f7366(l);
    ss2_wait(1);
    up_sp();
}

void ss2_fade_in_all(void)
{
    u8 l;
    for (l = 1; l != 8; l++) fade_all_step(l);
}

void ss2_fade_out_all(void)
{
    s8 l;
    for (l = 6; l != -1; l--) fade_all_step((u8)l);
}

/* $71F4 / $7211: as $71BA/$71D7 with the alternate BG set ($49BC) */
static void fade_alt_step(u8 l)
{
    f7324(l);
    ss2_wait(2);
    up_bg_stage();
    f7350(l);
    f7366(l);
    ss2_wait(1);
    up_sp_stage();
}

void ss2_fade_in_alt(void)
{
    u8 l;
    for (l = 1; l != 8; l++) fade_alt_step(l);
}

void ss2_fade_out_alt(void)
{
    s8 l;
    for (l = 6; l != -1; l--) fade_alt_step((u8)l);
}

/* Unfaded VCE contents for the current stage (what the stage load sets) */
void ss2_vce_reset(void)
{
    u16 i, bg = stage_ptr(0x40D0), sp = stage_ptr(0x45AE);
    for (i = 0; i < 512; i++)
    {
        u16 a;
        u8 p = (u8)((i >> 4) & 15), c = (u8)(i & 15);
        if (i < 256) a = p < 5 ? (u16)(0x4000 + p * 16 + c * 2) : (u16)(bg + (p - 5) * 16 + c * 2);
        else a = p < 8 ? (u16)(0x4050 + p * 16 + c * 2) : (u16)(sp + (p - 8) * 16 + c * 2);
        ss2_vce[i] = (u16)(ss2_rom_byte(a) | ((ss2_rom_byte((u16)(a + 1)) & 1) << 8));
    }
    ss2_vce_dirty = 1;
}

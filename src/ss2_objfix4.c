/* SON SON II objects in fixed bank 4 ($A000-$BFFF), rebuilt from bank 4
 * $A89C-$ACE6: the striker (stages 4-7).
 *
 * Four slots: x (body, record $ACE7 / $ACFD) and x+1..x+3 (the weapon
 * pieces, record $AD13). The pieces' own handler ($ACE1) removes them each
 * frame; the body re-creates and places them on its update.
 * Fields: $297A mode (1 walk, 2 stand, 3 strike), $299A player to the
 * right, $29BA facing (1 = right), $29DA mode timer, $29FA strike repeat
 * timer, $2A1A strikes left, $2A3A strike animation step.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"

#define OX16(x)     ((u16)((O(0x247A, x) << 8) | O(0x249A, x)))
#define OY16(x)     ((u16)((O(0x24DA, x) << 8) | O(0x24FA, x)))

extern u8 ss2_obj_xout;                     /* src/ss2_obj17.c */

static void set_vx(u8 x, u16 v) { O(0x25BA, x) = (u8)(v >> 8); O(0x25DA, x) = (u8)v; }
static void set_script(u8 x, u16 p) { O(0x295A, x) = (u8)p; O(0x293A, x) = (u8)(p >> 8); }
static void set_handler(u8 x, u16 h) { O(0x28DA, x) = (u8)h; O(0x28BA, x) = (u8)(h >> 8); }
static void set_x16(u8 x, u16 v) { O(0x247A, x) = (u8)(v >> 8); O(0x249A, x) = (u8)v; }
static void set_y16(u8 x, u16 v) { O(0x24DA, x) = (u8)(v >> 8); O(0x24FA, x) = (u8)v; }

/* $AC7E/$AC9F/$ACC0: walk / stand / strike scripts by facing */
static void anim(u8 x, u16 left, u16 right)
{
    set_script(x, O(0x29BA, x) ? right : left);
    O(0x267A, x) = 1;
}
static void anim_walk(u8 x)   { anim(x, 0xAD29, 0xAD30); }
static void anim_stand(u8 x)  { anim(x, 0xAD37, 0xAD3C); }
static void anim_strike(u8 x) { anim(x, 0xAD41, 0xAD46); }

/* $AAD7: run the body, then lay out the weapon pieces */
static void finish(u8 x)
{
    u16 o1, o2, o3;
    u8 a, i;
    ss2_obj_common(x);
    if (!O(0x245A, x))
    {
        for (i = 1; i < 4; i++) O(0x245A, x + i) = 0;
        ss2_obj_xout = (u8)(x + 3);
        return;
    }
    if ((u8)(O(0x245B, x) | O(0x245C, x) | O(0x245D, x)) != 0x80)
    {
        O(0x245B, x) = 0x80;
        O(0x245C, x) = 0x80;
        O(0x245D, x) = 0x80;
        O(0x2A3A, x) = 0;
    }
    a = O(0x2A3A, x);
    if (!a)
    {
        /* $AB1B: weapon down: pieces parked at y 0 */
        if (O(0x297A, x) == 3) anim_stand(x);
        for (i = 1; i < 4; i++) set_y16((u8)(x + i), 0);
        o1 = 0;
    }
    else
    {
        u16 y8 = (u16)(OY16(x) + 8);
        O(0x2A3A, x)--;
        if (a == 6)
        {
            set_y16((u8)(x + 1), y8);
            set_y16((u8)(x + 2), 0);
            set_y16((u8)(x + 3), 0);
            o1 = O(0x29BA, x) ? 0x0020 : 0xFFF0;
        }
        else if (a == 5)
        {
            set_y16((u8)(x + 1), y8);
            set_y16((u8)(x + 3), y8);
            set_y16((u8)(x + 2), 0);
            o1 = O(0x29BA, x) ? 0x0030 : 0xFFE0;
        }
        else
        {
            for (i = 1; i < 4; i++) set_y16((u8)(x + i), y8);
            o1 = O(0x29BA, x) ? 0x0040 : 0xFFD0;
        }
    }
    /* $ABDF */
    O(0x267B, x) = 1;
    if (!O(0x29BA, x)) { set_script((u8)(x + 1), 0xAD4B); o2 = 0xFFE0; o3 = 0xFFF0; }
    else { set_script((u8)(x + 1), 0xAD50); o2 = 0x0030; o3 = 0x0020; }
    set_x16((u8)(x + 1), (u16)(OX16(x) + o1));
    set_x16((u8)(x + 2), (u16)(OX16(x) + o2));
    set_x16((u8)(x + 3), (u16)(OX16(x) + o3));
    ss2_obj_common((u8)(x + 1));
    ss2_obj_common((u8)(x + 2));
    ss2_obj_common((u8)(x + 3));
    ss2_obj_xout = (u8)(x + 3);              /* returns with X on the last piece */
}

/* $AA96: walking: turn at ledges, stop at walls */
static void walk(u8 x)
{
    O(0x263A, x) = (O(0x25BA, x) & 0x80) ? 0x06 : 0x19;
    O(0x265A, x) = 0x20;
    if (!ss2_obj_probe(x))
    {
        set_vx(x, (u16)(0 - (u16)((O(0x25BA, x) << 8) | O(0x25DA, x))));
        O(0x29DA, x) = 0x12;
    }
    else
    {
        O(0x265A, x) = 0x1F;
        if (ss2_obj_probe(x)) set_vx(x, 0);
    }
    finish(x);
}

/* $AA58: one strike */
static void strike(u8 x)
{
    O(0x2A1A, x)--;
    O(0x29FA, x) = 0x0A;
    anim_strike(x);
    O(0x2A3A, x) = 6;
    finish(x);
}

/* $A9B4 */
static void think(u8 x)
{
    u16 d = (u16)(((u8)(O(0x247A, x) + 1) << 8 | O(0x249A, x)) - (((u8)(P_XH + 1) << 8) | P_XL));
    O(0x299A, x) = 0;
    if ((u16)(((u8)(O(0x247A, x) + 1) << 8) | O(0x249A, x)) < (u16)(((u8)(P_XH + 1) << 8) | P_XL))
    {
        O(0x299A, x)++;
        d = (u16)(0 - d);
    }
    if (O(0x29DA, x))
    {
        O(0x29DA, x)--;
        if (O(0x297A, x) == 3)
        {
            /* $AA4E */
            if (!O(0x2A1A, x) || --O(0x29FA, x)) { finish(x); return; }
            strike(x);
            return;
        }
        if (O(0x297A, x) == 1) { walk(x); return; }
        finish(x);
        return;
    }
    if (d < 0x60)
    {
        /* $AA31: close: three strikes */
        O(0x297A, x) = 3;
        O(0x29DA, x) = 0x78;
        set_vx(x, 0);
        O(0x29BA, x) = O(0x299A, x);
        O(0x2A1A, x) = 3;
        strike(x);
        return;
    }
    ss2_random();
    if (!(ZP(0x8D) & 0x80))
    {
        /* $AA6B: walk toward the player */
        O(0x29BA, x) = O(0x299A, x);
        set_vx(x, O(0x299A, x) ? 0x01C0 : 0xFE40);
        anim_walk(x);
        O(0x297A, x) = 1;
        O(0x29DA, x) = 0x12;
        walk(x);
        return;
    }
    O(0x297A, x) = 2;
    O(0x29DA, x) = 0x3C;
    set_vx(x, 0);
    O(0x29BA, x) = O(0x299A, x);
    anim_stand(x);
    finish(x);
}

/* $A99A: wait until on screen (x < $E0) */
static void enter(u8 x)
{
    if (OX16(x) >= 0xE0) { finish(x); return; }
    set_handler(x, 0xA9B4);
    think(x);
}

/* $A8AE */
static void spawn(u16 rec)
{
    u8 s = ZP(0x44), x, i;
    if (O(0x2B5A, s) | O(0x2C1A, s)) return;
    if (ss2_spawn_visible()) return;
    for (x = 0; O(0x245A, x) | O(0x245A, x + 1) | O(0x245A, x + 2) | O(0x245A, x + 3); )
        if (++x == 0x1D) return;
    ss2_obj_init(x, rec);
    for (i = 1; i < 4; i++) ss2_obj_init((u8)(x + i), 0xAD13);
    O(0x247A, x) = ZP(0x40);
    O(0x249A, x) = ZP(0x41);
    O(0x24DA, x) = ZP(0x42);
    O(0x24FA, x) = ZP(0x43);
    for (i = 0; i < 4; i++)
    {
        O(0x25BA, x + i) = 0;
        O(0x25DA, x + i) = 0;
        O(0x25FA, x + i) = 0;
        O(0x261A, x + i) = 0;
    }
    for (i = 0; i < 4; i++) O(0x287A, x + i) = 1;
    for (i = 1; i < 4; i++) { O(0x2B1A, x + i) = 0; O(0x2B3A, x + i) = 0; }
    O(0x29BA, x) = 0;
    if (O(0x247A, x) & 0x80) O(0x29BA, x)++;
    anim_stand(x);
    O(0x297A, x) = 2;
    O(0x29DA, x) = 0;
    O(0x29FA, x) = 0;
    O(0x2A3A, x) = 0;
    for (i = 1; i < 4; i++) O(0x281A, x + i) = 0xBF;
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    if (!O(0x2CDA, s)) O(0x2CDA, s) = (u8)(O(0x289A, x) & 0x0F);
    else O(0x289A, x) = (u8)((O(0x289A, x) & 0xF0) | O(0x2CDA, s));
    enter(x);
}

/* $AC5E */
static void hit(u8 x)
{
    if (O(0x245A, x) == 0x40)
    {
        u8 s = O(0x281A, x);
        O(0x2B5A, s)--;
        O(0x2C1A, s) = 0x64;
        ss2_obj_death(x);
        return;
    }
    O(0x245A, x) = 0x80;
    think(x);
}

u8 ss2_objfix4_handler(u16 addr, u8 x)
{
    switch (addr)
    {
        case 0xA99A: enter(x); return 1;
        case 0xA9B4: think(x); return 1;
        case 0xAC5E: hit(x); return 1;
        case 0xACE1: O(0x245A, x) = 0; return 1;     /* weapon piece */
    }
    return 0;
}

u8 ss2_spawnfix4_handler(u16 addr)
{
    switch (addr)
    {
        case 0xA89C: spawn(0xACE7); return 1;
        case 0xA8A6: spawn(0xACFD); return 1;
    }
    return 0;
}

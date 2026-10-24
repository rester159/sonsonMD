/* SON SON II enemy death and item drops, rebuilt from HuCard bank 2
 * $6E47-$6F36 and bank 3 $9072-$91EB, $9428-$9515, $98B3-$9904.
 *
 * A defeated enemy turns into a short explosion object; if its spawner
 * carries a drop table ($289A: high nibble table group, low nibble table),
 * a weighted random kind is rolled from bank 1 $51DA and an item object is
 * created. Item kinds 3-9 use record $9204, kinds $0A+ record $95B4.
 * Extra fields: $26BA item kind, $2A1A "stays put" flag, $297A bounced,
 * $29DA:$29FA landing script.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"

#define OX16(x)     ((u16)((O(0x247A, x) << 8) | O(0x249A, x)))
#define OY16(x)     ((u16)((O(0x24DA, x) << 8) | O(0x24FA, x)))

static void set_ox16(u8 x, u16 v) { O(0x247A, x) = (u8)(v >> 8); O(0x249A, x) = (u8)v; }
static void set_oy16(u8 x, u16 v) { O(0x24DA, x) = (u8)(v >> 8); O(0x24FA, x) = (u8)v; }

/* $9125/$94DB: item kind setup from the bank 3 tables */
static void item_kind(u8 x, u8 kind)
{
    O(0x2A1A, x) = (u8)(kind & 0x80);
    kind &= 0x7F;
    O(0x26BA, x) = kind;
    if (kind < 0x0A)
    {
        u8 i = (u8)(kind - 3), t = (u8)(i * 6);
        ZP(0x35) = (u8)(i << 1);
        O(0x295A, x) = ss2_rom_byte((u16)(0x921A + t));
        O(0x293A, x) = ss2_rom_byte((u16)(0x921B + t));
        O(0x29BA, x) = ss2_rom_byte((u16)(0x921C + t));
        O(0x299A, x) = ss2_rom_byte((u16)(0x921D + t));
        O(0x29FA, x) = ss2_rom_byte((u16)(0x921E + t));
        O(0x29DA, x) = ss2_rom_byte((u16)(0x921F + t));
        O(0x255A, x) |= ss2_rom_byte((u16)(0x9244 + i));
    }
    else
    {
        u8 i = (u8)(kind - 0x0A), t = (u8)(i << 2);
        O(0x295A, x) = ss2_rom_byte((u16)(0x95CA + t));
        O(0x293A, x) = ss2_rom_byte((u16)(0x95CB + t));
        O(0x29FA, x) = ss2_rom_byte((u16)(0x95CC + t));
        O(0x29DA, x) = ss2_rom_byte((u16)(0x95CD + t));
        O(0x255A, x) |= ss2_rom_byte((u16)(0x9616 + i));
    }
}

/* $9072/$9428: drop item ZP($35) out of object p. Returns the new slot or
 * $20 when no slot was free (the source leaves X = $20). */
static u8 drop_item(u8 p)
{
    u8 x = ss2_obj_alloc(), o;
    u8 kind = ZP(0x35);
    if (x == 0xFF) return 0x20;
    ss2_obj_init(x, (kind & 0x7F) < 0x0A ? 0x9204 : 0x95B4);
    O(0x25BA, x) = 0; O(0x25DA, x) = 0;
    O(0x25FA, x) = 0xFA; O(0x261A, x) = 0;
    o = 0;
    if (!(kind & 0x80)) { o = 8; O(0x269A, x) = 3; }
    ZP(0x30) = o;
    set_ox16(x, (u16)(OX16(p) + o));
    set_oy16(x, (u16)(OY16(p) + o));
    O(0x297A, x) = 0;
    O(0x287A, x) = 0;
    item_kind(x, kind);
    return x;
}

/* bank 2 $6E47: become the explosion and roll the drop */
void ss2_obj_death(u8 x)
{
    u8 g, t;
    if (!(O(0x253A, x) & 1)) set_ox16(x, (u16)(OX16(x) - 8));
    set_oy16(x, (u16)(OY16(x) + ((O(0x253A, x) & 0x30) >> 1)));
    set_oy16(x, (u16)(OY16(x) - 8));
    O(0x25DA, x) = 0; O(0x25BA, x) = 0;
    O(0x261A, x) = 0; O(0x25FA, x) = 0;
    O(0x255A, x) = 0x84;
    O(0x253A, x) = 0x11;
    O(0x267A, x) = 1;
    O(0x295A, x) = 0x39; O(0x293A, x) = 0x6F;
    O(0x27DA, x) = 0xB2; O(0x27BA, x) = 0x2D;
    O(0x291A, x) = 0x67; O(0x28FA, x) = 0xEE;
    g = (u8)(O(0x289A, x) & 0xF0);
    if (g)
    {
        ZP(0x30) = (u8)((u8)(g - 0x10) >> 1);
        t = (u8)(O(0x289A, x) & 0x0F);
        if (t)
        {
            u16 tab, i;
            u8 c, sum, roll, kind;
            i = (u8)(((u8)(t - 1) << 1) | ZP(0x30));
            tab = (u16)(ss2_rom_byte((u16)(0x51DA + i)) | (ss2_rom_byte((u16)(0x51DB + i)) << 8));
            ZP(0x2E) = (u8)tab; ZP(0x2F) = (u8)(tab >> 8);
            c = ss2_random();
            roll = (u8)((ZP(0x8D) & 0x1F) + 1);
            ZP(0x30) = roll;
            sum = 0;
            for (i = 0; ; i = (u16)(i + 2))
            {
                u16 v = (u16)(sum + ss2_rom_byte((u16)(tab + (u8)i)) + c);
                sum = (u8)v;
                if (sum >= roll) break;
                c = 0;
            }
            ZP(0x31) = sum;
            kind = ss2_rom_byte((u16)(tab + (u8)(i + 1)));
            if (kind)
            {
                u8 n, s;
                ZP(0x35) = kind;
                n = drop_item(x);
                if (n != 0x20) O(0x281A, n) = 0xBF;
                s = O(0x281A, x);
                t = (u8)(O(0x2CDA, s) & 0x0F);
                if (t && t != 1) O(0x2CDA, s)--;
            }
        }
    }
    ss2_sfx(0x17);
    ss2_obj_common(x);
}

/* $9173: falling item: bounce once, then switch to its landing script */
static void item_fall(u8 x)
{
    O(0x263A, x) = 0x08;
    O(0x265A, x) = (s8)O(0x25FA, x) < 0 ? 0x00 : 0x10;
    if (!ss2_obj_probe(x))
    {
        u16 t = (u16)(O(0x261A, x) + 0x40);
        O(0x261A, x) = (u8)t;
        O(0x25FA, x) = (u8)(O(0x25FA, x) + (t >> 8));
    }
    else if ((s8)O(0x25FA, x) < 0)
    {
        O(0x25FA, x) = 0; O(0x261A, x) = 0;
    }
    else if (!O(0x297A, x))
    {
        O(0x261A, x) = 0; O(0x25FA, x) = 0xFC;
        O(0x297A, x) = 1;
    }
    else
    {
        O(0x25FA, x) = 0; O(0x261A, x) = 0;
        if (!O(0x2A1A, x))
        {
            O(0x267A, x) = 1;
            O(0x293A, x) = O(0x29DA, x);
            O(0x295A, x) = O(0x29FA, x);
        }
        O(0x28DA, x) = 0x67; O(0x28BA, x) = 0xEE;
    }
    ss2_obj_common(x);
}

/* $98B3: breakable container: first hit opens it, then it drops its item */
static void container_hit(u8 x)
{
    u8 s, a, n;
    if (!O(0x297A, x))
    {
        O(0x245A, x) = 0x80;
        O(0x295A, x) = 0x7A; O(0x293A, x) = 0x99;
        O(0x297A, x) = 1;
        O(0x267A, x) = 1;
        O(0x26DA, x) = 1;
        ss2_obj_common(x);
        return;
    }
    s = O(0x281A, x);
    O(0x2C1A, s) = 1;
    a = R(0x2223 + s);
    if (a & 0x80) { ss2_unported(0x9907); return; }
    ZP(0x35) = (u8)(a | 0x80);
    n = drop_item(x);
    if (n != 0x20) O(0x281A, n) = O(0x281A, x);
    ss2_obj_death(x);
}

/* $9002-$9026: an item placed in the level (kind 3-9) at ($40-$43) */
static void place_item(u8 kind)
{
    u8 s = ZP(0x44), x;
    ZP(0x35) = kind;
    if (O(0x2B5A, s)) return;
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0x91EE);
    O(0x25BA, x) = 0; O(0x25DA, x) = 0;
    O(0x25FA, x) = 0; O(0x261A, x) = 0;
    O(0x247A, x) = ZP(0x40); O(0x249A, x) = ZP(0x41);
    O(0x24DA, x) = ZP(0x42); O(0x24FA, x) = ZP(0x43);
    O(0x287A, x) = 0;
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    item_kind(x, kind);
}

u8 ss2_item_spawn_handler(u16 addr)
{
    if (addr >= 0x9002 && addr <= 0x9026 && !((addr - 0x9002) % 6))
    {
        place_item((u8)(3 + (addr - 0x9002) / 6));
        return 1;
    }
    return 0;
}

u8 ss2_item_handler(u16 addr, u8 x)
{
    switch (addr)
    {
        case 0x9173: item_fall(x); return 1;
        case 0x98B3: container_hit(x); return 1;
    }
    return 0;
}

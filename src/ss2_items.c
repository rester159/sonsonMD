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

/* field a of slot x through o = &ss2_ram[x] (a fixed displacement) */
#define OF(a) (o[(u16)((a) - 0x2000)])

#define OX16(x)     ((u16)((O(0x247A, x) << 8) | O(0x249A, x)))
#define OY16(x)     ((u16)((O(0x24DA, x) << 8) | O(0x24FA, x)))

static void set_ox16(u8 x, u16 v) { O(0x247A, x) = (u8)(v >> 8); O(0x249A, x) = (u8)v; }
static void set_oy16(u8 x, u16 v) { O(0x24DA, x) = (u8)(v >> 8); O(0x24FA, x) = (u8)v; }

/* $9125/$94DB: item kind setup from the bank 3 tables */
static void item_kind(u8 x, u8 kind)
{
    u8 *const o = &ss2_ram[x];
    OF(0x2A1A) = (u8)(kind & 0x80);
    kind &= 0x7F;
    OF(0x26BA) = kind;
    if (kind < 0x0A)
    {
        u8 i = (u8)(kind - 3), t = (u8)(i * 6);
        ZP(0x35) = (u8)(i << 1);
        OF(0x295A) = ss2_rom_byte((u16)(0x921A + t));
        OF(0x293A) = ss2_rom_byte((u16)(0x921B + t));
        OF(0x29BA) = ss2_rom_byte((u16)(0x921C + t));
        OF(0x299A) = ss2_rom_byte((u16)(0x921D + t));
        OF(0x29FA) = ss2_rom_byte((u16)(0x921E + t));
        OF(0x29DA) = ss2_rom_byte((u16)(0x921F + t));
        OF(0x255A) |= ss2_rom_byte((u16)(0x9244 + i));
    }
    else
    {
        u8 i = (u8)(kind - 0x0A), t = (u8)(i << 2);
        OF(0x295A) = ss2_rom_byte((u16)(0x95CA + t));
        OF(0x293A) = ss2_rom_byte((u16)(0x95CB + t));
        OF(0x29FA) = ss2_rom_byte((u16)(0x95CC + t));
        OF(0x29DA) = ss2_rom_byte((u16)(0x95CD + t));
        OF(0x255A) |= ss2_rom_byte((u16)(0x9616 + i));
    }
}

/* $9072/$9428: drop item ZP($35) out of object p. Returns the new slot or
 * $20 when no slot was free (the source leaves X = $20). */
u8 ss2_drop_item(u8 p)
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
    u8 *const o = &ss2_ram[x];
    u8 g, t;
    if (!(OF(0x253A) & 1)) set_ox16(x, (u16)(OX16(x) - 8));
    set_oy16(x, (u16)(OY16(x) + ((OF(0x253A) & 0x30) >> 1)));
    set_oy16(x, (u16)(OY16(x) - 8));
    OF(0x25DA) = 0; OF(0x25BA) = 0;
    OF(0x261A) = 0; OF(0x25FA) = 0;
    OF(0x255A) = 0x84;
    OF(0x253A) = 0x11;
    OF(0x267A) = 1;
    OF(0x295A) = 0x39; OF(0x293A) = 0x6F;
    OF(0x27DA) = 0xB2; OF(0x27BA) = 0x2D;
    OF(0x291A) = 0x67; OF(0x28FA) = 0xEE;
    g = (u8)(OF(0x289A) & 0xF0);
    if (g)
    {
        ZP(0x30) = (u8)((u8)(g - 0x10) >> 1);
        t = (u8)(OF(0x289A) & 0x0F);
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
                n = ss2_drop_item(x);
                if (n != 0x20) O(0x281A, n) = 0xBF;
                s = OF(0x281A);
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
    u8 *const o = &ss2_ram[x];
    OF(0x263A) = 0x08;
    OF(0x265A) = (s8)OF(0x25FA) < 0 ? 0x00 : 0x10;
    if (!ss2_obj_probe(x))
    {
        u16 t = (u16)(OF(0x261A) + 0x40);
        OF(0x261A) = (u8)t;
        OF(0x25FA) = (u8)(OF(0x25FA) + (t >> 8));
    }
    else if ((s8)OF(0x25FA) < 0)
    {
        OF(0x25FA) = 0; OF(0x261A) = 0;
    }
    else if (!OF(0x297A))
    {
        OF(0x261A) = 0; OF(0x25FA) = 0xFC;
        OF(0x297A) = 1;
    }
    else
    {
        OF(0x25FA) = 0; OF(0x261A) = 0;
        if (!OF(0x2A1A))
        {
            OF(0x267A) = 1;
            OF(0x293A) = OF(0x29DA);
            OF(0x295A) = OF(0x29FA);
        }
        OF(0x28DA) = 0x67; OF(0x28BA) = 0xEE;
    }
    ss2_obj_common(x);
}

/* $98B3: breakable container: first hit opens it, then it drops its item */
static void container_hit(u8 x)
{
    u8 *const o = &ss2_ram[x];
    u8 s, a, n;
    if (!OF(0x297A))
    {
        OF(0x245A) = 0x80;
        OF(0x295A) = 0x7A; OF(0x293A) = 0x99;
        OF(0x297A) = 1;
        OF(0x267A) = 1;
        OF(0x26DA) = 1;
        ss2_obj_common(x);
        return;
    }
    s = OF(0x281A);
    O(0x2C1A, s) = 1;
    a = R(0x2223 + s);
    if (a & 0x80) { ss2_unported(0x9907); return; }
    ZP(0x35) = (u8)(a | 0x80);
    n = ss2_drop_item(x);
    if (n != 0x20) O(0x281A, n) = OF(0x281A);
    ss2_obj_death(x);
}

/* $9032/$93D6: an item at ($40-$43) for spawner $44 (record $91EE for
 * kinds below $0A, $959E above) */
static void spawn_item(u8 kind)
{
    u8 s = ZP(0x44), x;
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_obj_init(x, (kind & 0x7F) < 0x0A ? 0x91EE : 0x959E);
    O(0x25BA, x) = 0; O(0x25DA, x) = 0;
    O(0x25FA, x) = 0; O(0x261A, x) = 0;
    O(0x247A, x) = ZP(0x40); O(0x249A, x) = ZP(0x41);
    O(0x24DA, x) = ZP(0x42); O(0x24FA, x) = ZP(0x43);
    O(0x287A, x) = 0;
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    item_kind(x, kind);
}

/* $9002-$9026: an item placed in the level (kind 3-9) */
static void place_item(u8 kind)
{
    ZP(0x35) = kind;
    if (O(0x2B5A, ZP(0x44))) return;
    spawn_item(kind);
}

/* $983E: closed container, or its item once the spawner was opened */
static void spawn_container(void)
{
    u8 s = ZP(0x44), x;
    if (O(0x2B5A, s)) return;
    if (O(0x2C1A, s))
    {
        ZP(0x35) = R(0x2223 + s);
        spawn_item(ZP(0x35));
        return;
    }
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0x989D);
    O(0x25BA, x) = 0; O(0x25DA, x) = 0;
    O(0x25FA, x) = 0; O(0x261A, x) = 0;
    O(0x247A, x) = ZP(0x40); O(0x249A, x) = ZP(0x41);
    O(0x24DA, x) = ZP(0x42); O(0x24FA, x) = ZP(0x43);
    O(0x297A, x) = 0;
    O(0x287A, x) = 0;
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    ss2_obj_common(x);
}

u8 ss2_item_spawn_handler(u16 addr)
{
    if (addr >= 0x9002 && addr <= 0x9026 && !((addr - 0x9002) % 6))
    {
        place_item((u8)(3 + (addr - 0x9002) / 6));
        return 1;
    }
    if (addr == 0x983E) { spawn_container(); return 1; }
    return 0;
}

/* ---- pickups (bank 2 $7D65, handlers in bank 3) ------------------------ */

/* $8289: common: pickup sound, switch to the item's collected handler */
static void collected(u8 x)
{
    u8 *const o = &ss2_ram[x];
    ss2_sfx(0x28);
    OF(0x267A) = 1;
    OF(0x28DA) = OF(0x291A);
    OF(0x28BA) = OF(0x28FA);
}

/* $80C8: money bag: award, then float up with its collected script.
 * Returns 1: the source drops collide's return and re-enters $EE67. */
static u8 money(u8 x, u8 award)
{
    u8 *const o = &ss2_ram[x];
    ss2_score_award(award);
    ss2_sfx(0x28);
    OF(0x269A) |= 0x24;
    OF(0x267A) = 1;
    OF(0x293A) = OF(0x299A);
    OF(0x295A) = OF(0x29BA);
    OF(0x28DA) = 0x67; OF(0x28BA) = 0xEE;
    OF(0x25FA) = 0xFF; OF(0x261A) = 0;
    OF(0x25BA) = 0; OF(0x25DA) = 0;
    return 1;
}

static u8 min_add(u8 v, u8 add, u8 cap)
{
    u8 t = (u8)(v + add);
    return t < cap ? t : cap;
}

/* bank 4 $BA52: 8-digit BCD subtract of the table at a from the money */
void ss2_money_sub(u16 a)
{
    s8 y;
    u8 c = 1;
    for (y = 7; y >= 0; y--)
    {
        s16 d = (s16)(R(0x220B + y) - ss2_rom_byte((u16)(a + y)) - (1 - c));
        c = 1;
        if (d < 0) { d += 0x0A; c = 0; }
        R(0x220B + y) = (u8)d;
    }
}

/* $82A0: trap item: hurt the player once ($86 latches) */
static void trap(u8 x)
{
    if (Z_DEMOFX) return;
    if ((u8)(R(0x22E9) | R(0x22EA) | R(0x22EB)) == 0x80)
    {
        u8 m = P_MAGIC, d = O(0x26FA, x);
        P_MAGIC = (u8)(m - d);
        R(0x22BD) = P_MAGIC;
        if (m <= d) { P_MAGIC = 0; R(0x22BD) = 0; }
    }
    else if (!ZP(0x65))
    {
        u8 l = P_LIFE, d = O(0x26FA, x);
        P_LIFE = (u8)(l - d);
        R(0x22BA) = P_LIFE;
        if (l <= d)
        {
            P_HITOBJ = (u8)(x + 1);
            P_LIFE = 0; R(0x22BA) = 0;
            P_MODE = 0x40;
            return;
        }
    }
    P_MODE = 0x20;
    P_HITOBJ = (u8)(x + 1);
    P_HURT = 0x5A;
    Z_DEMOFX = 1;
}

/* $7F74: an item the player can stand on (kind 2): land on its top */
static void stand_on(u8 x)
{
    u8 *const o = &ss2_ram[x];
    u8 a;
    if (R(0x22A4) & 0x80) return;
    if (R(0x22B1) && R(0x22C6) < 8) return;
    if (OF(0x299A) == 0x88)
    {
        if (ss2_probe(0x14, 0x20)) return;
        if (ss2_probe(0x0C, 0x20)) return;
    }
    a = (u8)(R(0x229C) + 0x20 - OF(0x24FA));
    if (a >= 8) return;
    ZP(0x35) = a;
    R(0x229C) = (u8)(R(0x229C) - a);
    R(0x22B0) = 1;
    R(0x22C5) = (u8)(x + 1);
    OF(0x251A) = ZP(0x4A);
}

/* bank 2 $7FD8: the object the player stands on ($22C5, from $7F74)
 * moves the camera with it: a stand-on item ($299A = $88) by its own
 * fall speed, kind 1 by its negated velocity, others by their velocity */
static void cam_add(u8 lo, u8 hi, u16 frac, u16 low, u16 top)
{
    u8 sx = (u8)((hi & 0x80) ? 0xFF : 0);
    u16 t = (u16)(lo + ZP(frac));
    ZP(frac) = (u8)t;
    t = (u16)(hi + ZP(low) + (t >> 8));
    ZP(low) = (u8)t;
    ZP(top) = (u8)((sx + ZP(top) + (t >> 8)) & 7);
}

void ss2_platform_carry(void)
{
    u8 x = (u8)(P_PUSHOBJ - 1), k = O(0x299A, x);
    if (k == 0x88)
    {
        u16 t;
        R(0x2C1A + O(0x281A, x)) = 0x64;
        O(0x261A, x) |= 1;
        if (O(0x25FA, x) == 5) { ZP(0x28) = O(0x261A, x); ZP(0x29) = O(0x25FA, x); }
        else
        {
            t = (u16)(0x20 + O(0x261A, x));
            ZP(0x28) = (u8)t;
            ZP(0x29) = (u8)(O(0x25FA, x) + (t >> 8));
        }
        t = (u16)(ZP(0x28) + ZP(0x4A));
        ZP(0x4A) = (u8)t;
        t = (u16)(ZP(0x29) + ZP(0x49) + (t >> 8));
        ZP(0x49) = (u8)t;
        ZP(0x48) = (u8)((ZP(0x48) + (t >> 8)) & 7);
        return;
    }
    if (k == 1)
    {
        u16 v = (u16)-(s16)((O(0x25BA, x) << 8) | O(0x25DA, x));
        u16 w = (u16)-(s16)((O(0x25FA, x) << 8) | O(0x261A, x));
        ZP(0x28) = (u8)v; ZP(0x29) = (u8)(v >> 8);
        ZP(0x2A) = (u8)w; ZP(0x2B) = (u8)(w >> 8);
    }
    else
    {
        ZP(0x29) = O(0x25BA, x); ZP(0x28) = O(0x25DA, x);
        ZP(0x2B) = O(0x25FA, x); ZP(0x2A) = O(0x261A, x);
    }
    ZP(0x37) = (u8)((ZP(0x29) & 0x80) ? 0xFF : 0);
    cam_add(ZP(0x28), ZP(0x29), 0x47, 0x46, 0x45);
    ZP(0x37) = (u8)((ZP(0x2B) & 0x80) ? 0xFF : 0);
    cam_add(ZP(0x2A), ZP(0x2B), 0x4A, 0x49, 0x48);
}

/* $8104-$818D: key-type items: a $22C1 bit and their item-box icon ($819D) */
static void key_item(u8 x, u8 bit, u16 icon)
{
    collected(x);
    R(0x22C1) |= bit;
    ss2_sfx(0x28);
    ss2_hud_text(icon);
}

u8 ss2_item_pickup(u8 x)
{
    static const u8 money_award[7] = { 0x20, 0x21, 0x22, 0x23, 0x24, 0x26, 0x28 };
    static const u8 food_award[4] = { 0x01, 0x02, 0x04, 0x06 };
    u8 k = O(0x26BA, x);
    switch (k)
    {
        case 1:                             /* $7DB5 */
            ss2_door(x);
            return 0;
        case 2:                             /* $7F74 */
            stand_on(x);
            return 0;
        case 10: key_item(x, 0x04, 0x4E47); return 0;   /* $8104 */
        case 12: key_item(x, 0x02, 0x4E3E); return 0;   /* $8117 */
        case 13: key_item(x, 0x08, 0x4E50); return 0;   /* $812A */
        case 14: key_item(x, 0x01, 0x4E35); return 0;   /* $813D */
        case 17:                                        /* $8150 */
            collected(x);
            R(0x22C1) |= 0x10;
            ZP(0x76) = 5;
            ss2_sfx(0x28);
            ss2_hud_text(0x4E59);
            return 0;
        case 26: key_item(x, 0x20, 0x4E6B); return 0;   /* $8167 */
        case 27: key_item(x, 0x40, 0x4E74); return 0;   /* $817A */
        case 28: key_item(x, 0x80, 0x4E7D); return 0;   /* $818D */
        case 3: case 4: case 5: case 6: case 7: case 8: case 9:
            return money(x, money_award[k - 3]);
        case 15:                            /* $81BB: life vessel */
        {
            u8 s;
            collected(x);
            R(0x22B9) = min_add(R(0x22B9), 2, 0x18);
            P_LIFE = min_add(P_LIFE, 4, R(0x22B9));
            for (s = 1, k = Z_STAGE; k; k--) s = (u8)(s << 1);
            ZP(0x37) = s;
            ZP(0x7C) |= s;
            return 0;
        }
        case 16:                            /* $81F2: magic +2 */
            collected(x);
            P_MAGIC = min_add(P_MAGIC, 2, R(0x22BC));
            return 0;
        case 18:                            /* $8207: lose money */
            collected(x);
            if (!(R(0x220B) | R(0x220C) | R(0x220D) | R(0x220E) | R(0x220F) | R(0x2210)))
            {
                R(0x2211) = 0;
                R(0x2212) = 0;
            }
            else ss2_money_sub(0x4C0C);
            ss2_hud_dirty |= 2;
            return 0;
        case 19: case 20: case 21: case 22: /* $8237: food points */
            ss2_score_award(food_award[k - 19]);
            collected(x);
            return 0;
        case 23: case 24:                   /* $8257/$825D: food life */
            ZP(0x35) = k == 23 ? 2 : 5;
            collected(x);
            P_LIFE = min_add(P_LIFE, ZP(0x35), R(0x22B9));
            return 0;
        case 25:                            /* $8276: staff upgrade */
        {
            u8 t = ss2_rom_byte((u16)(0x4CAB + Z_STAGE));
            collected(x);
            if (t >= Z_STAFFPAL) Z_STAFFPAL = t;
            ss2_staff_art();                /* $79F7 */
            return 0;
        }
        case 11:                            /* $81A6: clears the screen */
            collected(x);
            ss2_page[6] = ss2_banks[9];     /* TAM #$40 = 9 (no $8E) */
            ss2_mpr6 = 9;
            ZP(0x35) = 0x18;
            ss2_magic_damage_all(0x18);     /* bank 9 $D86A */
            ss2_page[6] = ss2_banks[ZP(0x8E) & 0x1F];
            ss2_mpr6 = ZP(0x8E);
            return 0;
        case 29:                            /* bank 3 $997F: townspeople, shops */
            ss2_npc(x);
            return 0;
        case 30:
            trap(x);
            return 0;
    }
    ss2_unported((u16)(0x7D00 | k));        /* doors, shops, message items */
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

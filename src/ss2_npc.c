/* SON SON II townspeople, fortune tellers and shops, rebuilt from HuCard
 * bank 3 $997F-$9E7A (item kind 29) and the shop interior bank 3 $9E7B -
 * bank 4 $A665.
 *
 * Touching a townsperson (spawned by bank $17 $DEC2-$DEEE with $297A = type
 * 0/1/2) opens a dialogue window. Type 0 talks, or for spawners $48/$49 is a
 * shopkeeper; types 1 and 2 are per-stage characters that sell, swap or
 * give items, guarded by the per-stage bits $7A (type 1) and $7B (type 2).
 * Some messages end in a choice whose continuation address the text names
 * (bank 2 $6C12 -> ss2_npc_continue). The item to hand over is left in $7E;
 * afterwards the townsperson vanishes ($6E47) and drops it ($9E41).
 *
 * The shop is a full-screen scene inside the main task: the stage fades
 * out, the shop picture is drawn (presentation: BAT/VRAM, ss2_shop_screen
 * for the renderer), four goods and an exit are browsed with the d-pad and
 * bought with I/II; prices at $98-$B7 (zp) rise after some purchases. On
 * exit the stage is redrawn and faded back in.
 *
 * Money is 8 BCD digits at $220B; prices are 8-digit tables in bank 1/2
 * ($4Cxx) or the zp price counters.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"
#include "ss2_task.h"

u8 ss2_shop_screen, ss2_shop_type;
u16 ss2_shop_goods;

static u8 npc_bank;                          /* $8E pushed at $999B */

static void text_at(u16 p)
{
    ZP(0x93) = (u8)p; ZP(0x94) = (u8)(p >> 8);
}

/* $BA4C-based VDC traffic (presentation): VWR is selected last */
static void vdc(void)
{
    ZP(0x8B) = 0x02;
}

/* bank 4 $BBC2: money digits redrawn */
static void hud_money(void)
{
    ss2_hud_dirty |= 2;
    vdc();
}

/* bank 4 $BA66: compare the price at p (8 digits, most significant first)
 * with the money ($28 = $220B). 0: price below the money (RTS); 1: equal
 * (return +4, JMP ($26)); 2: price above (return +7). jsr is the address
 * of the JSR, for the $26/$27 the source leaves. */
static u8 price_cmp(u16 p, u16 jsr)
{
    u8 y;
    ZP(0x28) = 0x0B; ZP(0x29) = 0x22;
    ZP(0x2A) = (u8)p; ZP(0x2B) = (u8)(p >> 8);
    for (y = 0; y < 8; y++)
    {
        u8 a = ss2_rom_byte((u16)(p + y)), b = R(0x220B + y);
        if (a < b) return 0;
        if (a != b)
        {
            u16 t = (u16)(jsr + 9);
            ZP(0x26) = (u8)t; ZP(0x27) = (u8)(t >> 8);
            return 2;
        }
    }
    {
        u16 t = (u16)(jsr + 6);
        ZP(0x26) = (u8)t; ZP(0x27) = (u8)(t >> 8);
    }
    return 1;
}

/* $BA52 with ($28) = p */
static void pay(u16 p)
{
    ZP(0x28) = (u8)p; ZP(0x29) = (u8)(p >> 8);
    ss2_money_sub(p);
}

/* $9C45/$9C96: STZ $220B, TII $220B,$220C,7 */
static void money_clear(void)
{
    u8 i;
    for (i = 0; i < 8; i++) R(0x220B + i) = 0;
}

/* $9E2F: the stage's bit into $30 (and $37) */
static void stage_bit(void)
{
    u8 b = 1, i;
    for (i = Z_STAGE; i; i--) b = (u8)(b << 1);
    ZP(0x37) = b;
    ZP(0x30) |= b;
}

static void mark_7a(void) { ZP(0x30) = ZP(0x7A); stage_bit(); ZP(0x7A) = ZP(0x30); }  /* $9E19 */
static void mark_7b(void) { ZP(0x30) = ZP(0x7B); stage_bit(); ZP(0x7B) = ZP(0x30); }  /* $9E24 */

/* $9E0D: the message at ($93) in a window at (6, 10) */
static void say(void)
{
    ZP(0x8F) = 0x06;
    ZP(0x91) = 0x0A;
    ss2_message();                           /* $6984 */
}

/* $9E41: hand over item $7E, then vanish */
static void give(u8 x)
{
    u8 n;
    ZP(0x35) = (u8)(ZP(0x7E) | 0x80);        /* $9072 (< $0A) / $9428 */
    n = ss2_drop_item(x);
    if (n != 0x20)
    {
        O(0x281A, n) = 0xBF;
        O(0x249A, n) = (u8)(O(0x249A, n) + 8);
        O(0x24FA, n) = (u8)(O(0x24FA, n) + 8);
    }
    ss2_obj_death(x);                        /* $6E47 */
}

/* $9DFA: say ($93), restore the bank, vanish (with item $7E) */
static void finish(void)
{
    u8 x;
    say();
    ss2_map6(npc_bank);
    x = ZP(0xB8);
    if (!ZP(0x7E)) { ss2_obj_death(x); return; }
    give(x);                                 /* $9E41 */
}

/* ---- the shop ($9E7B - bank 4 $A665) ------------------------------------ */

/* $A488: the shop's goods table: stage * 32 + spawner bit * 16 from $A666
 * for a type-0 shopkeeper, else $A746 (type 1) / $A756 */
static u16 goods(void)
{
    u8 x = ZP(0xB8), t = O(0x297A, x);
    u16 p;
    ZP(0x28) = (u8)(Z_STAGE << 5);
    if (!t)
    {
        ZP(0x28) |= (u8)(((u8)(O(0x281A, x) - 0x48) & 1) << 4);
        p = (u16)(ZP(0x28) + 0xA666);
    }
    else p = t == 1 ? 0xA746 : 0xA756;
    ZP(0x28) = (u8)p; ZP(0x29) = (u8)(p >> 8);
    return p;
}

/* $A5FA: the four prices: digits 3-7 of each price, leading zeros
 * skipped, from the BAT address in $A863 (PCE tile $1B0+d, palette 15) */
static void draw_prices(void)
{
    u16 g = goods();
    u8 i;
    for (i = 0; i < 4; i++)
    {
        u16 a = (u16)(ss2_rom_byte((u16)(0xA863 + i * 2)) | (ss2_rom_byte((u16)(0xA864 + i * 2)) << 8));
        u16 p = (u16)(ss2_rom_byte((u16)(g + i * 4 + 2)) | (ss2_rom_byte((u16)(g + i * 4 + 3)) << 8));
        u8 y = 3, x = (u8)(a & 63);
        while (y < 8 && !ss2_rom_byte((u16)(p + y))) y++;
        for (; y < 8; y++, x++)
            ss2_bat_put(x, (u8)(a >> 6), (u16)(0xF1B0 + ss2_rom_byte((u16)(p + y))));
    }
    vdc();
}

/* $9F89: an item of kind 0 (the staff) shows the next staff level */
static void staff_digit(void)
{
    u16 g = goods();
    u8 i;
    for (i = 0; i < 4; i++)
        if (!ss2_rom_byte((u16)(g + i * 4 + 1)))
        {
            ss2_bat_put(0x05, 0x17, (u16)(0xF1B0 + (u8)(ss2_rom_byte((u16)(0x4CA4 + Z_STAGE)) + 1)));
            return;
        }
}

/* $A43E: enough money for the price at ($2A)? Else PLA PLA, "not enough"
 * ($638D) and back to $A460. */
static u8 afford(u16 price)
{
    if (price_cmp(price, 0xA446) != 2) return 1;
    text_at(0x638D);
    ss2_msg_text();
    return 0;
}

/* $A472 "you have it already" / $A47D "thank you" */
static void have_it(void) { text_at(0x639F); ss2_msg_text(); }
static void thanks(void)  { text_at(0x63AE); ss2_msg_text(); }

/* $A121 etc.: BCD add the table at t to the price counter at d (digits 7-1) */
static void price_rise(u16 d, u16 t)
{
    u8 y, c = 0;
    for (y = 7; y; y--)
    {
        u8 a = (u8)(ss2_rom_byte((u16)(t + y)) + R(d + y) + c);
        c = a >= 0x0A;
        if (c) a = (u8)(a - 0x0A);
        R(d + y) = a;
    }
}

static u8 add_cap(u8 v, u8 add, u8 cap)
{
    u8 t = (u8)(v + add);
    return t < cap ? t : cap;
}

/* the goods ($A09A jump table); each ends at $A460 */
static void buy(u8 kind, u16 price)
{
    static const u16 key_icon[4] = { 0x4E47, 0x4E50, 0x4E3E, 0 };
    u8 b;
    switch (kind)
    {
        case 0:                              /* $A0B6: staff upgrade */
        {
            u8 t = ss2_rom_byte((u16)(0x4CA4 + Z_STAGE));
            if (t <= Z_STAFFPAL) { have_it(); return; }
            if (!afford(price)) return;
            ZP(0x28) = (u8)price; ZP(0x29) = (u8)(price >> 8);
            Z_STAFFPAL = ss2_rom_byte((u16)(0x4CA4 + Z_STAGE));
            ss2_money_sub(price);
            hud_money();
            thanks();
            ss2_staff_art();                 /* $79F7 */
            ss2_hud_dirty |= 4;              /* $FE5A weapon box */
            vdc();
            ss2_sfx(0x28);
            return;
        }
        case 1:                              /* $A0F2: magic +5 */
            if (!afford(price)) return;
            ZP(0x28) = (u8)price; ZP(0x29) = (u8)(price >> 8);
            ss2_sfx(0x28);
            P_MAGIC = add_cap(P_MAGIC, 5, R(0x22BC));
            ss2_money_sub(price);
            hud_money();
            thanks();
            price_rise(0x2098, 0x4C14);
            draw_prices();
            return;
        case 2:                              /* $A13A: the item $76 = 5 */
            if (ZP(0x76) >= 5) { have_it(); return; }
            if (!afford(price)) return;
            ZP(0x28) = (u8)price; ZP(0x29) = (u8)(price >> 8);
            R(0x22C1) |= 0x10;
            ZP(0x77) |= 0x10;
            ZP(0x76) = 5;
            ss2_money_sub(price);
            hud_money();
            thanks();
            ss2_sfx(0x28);
            ss2_hud_text(0x4E59);
            return;
        case 3: case 4: case 5:              /* $A178/$A1B3/$A1EE: keys */
            b = kind == 3 ? 0x04 : kind == 4 ? 0x08 : 0x02;
            if (R(0x22C1) & b) { have_it(); return; }
            if (!afford(price)) return;
            ZP(0x28) = (u8)price; ZP(0x29) = (u8)(price >> 8);
            R(0x22C1) |= b;
            ZP(0x77) |= b;
            ss2_money_sub(price);
            hud_money();
            thanks();
            if (kind == 5) price_rise(0x20A0, 0x4C3C);
            ss2_sfx(0x28);
            ss2_hud_text(key_icon[kind - 3]);
            if (kind == 5) draw_prices();
            return;
        case 6: case 7:                      /* $A244/$A28C: life +2 / +5 */
            if (!afford(price)) return;
            ZP(0x28) = (u8)price; ZP(0x29) = (u8)(price >> 8);
            P_LIFE = add_cap(P_LIFE, kind == 6 ? 2 : 5, R(0x22B9));
            ss2_money_sub(price);
            hud_money();
            thanks();
            if (kind == 6) price_rise(0x20A8, 0x4C0C);
            else price_rise(0x20B0, 0x4C1C);
            draw_prices();
            ss2_sfx(0x28);
            return;
        case 8:                              /* $A2D4: life vessel, once */
            if (ZP(0x7D)) { text_at(0x63BF); ss2_msg_text(); return; }
            if (!afford(price)) return;
            ZP(0x28) = (u8)price; ZP(0x29) = (u8)(price >> 8);
            R(0x22B9) = add_cap(R(0x22B9), 2, 0x18);
            P_LIFE = add_cap(P_LIFE, 4, R(0x22B9));
            ZP(0x7D)++;
            ss2_money_sub(price);
            hud_money();
            ss2_sfx(0x28);
            thanks();
            return;
        case 9: case 10: case 11: case 12: case 13:   /* $A324-$A3E4: $22BF */
            b = (u8)(0x02 << (kind - 9));
            if (R(0x22BF) & b) { have_it(); return; }
            if (!afford(price)) return;
            ZP(0x28) = (u8)price; ZP(0x29) = (u8)(price >> 8);
            R(0x22BF) |= b;
            ZP(0x78) |= b;
            ss2_money_sub(price);
            hud_money();
            ss2_sfx(0x28);
            thanks();
            return;
    }
    ss2_unported((u16)(0xA09A + (u8)(kind << 1)));
}

/* $A087: bars, one frame */
static void shop_frame(void)
{
    ss2_hud_bars();                          /* $7649, $7688 */
    vdc();
    ss2_wait(1);                             /* $B9B8 */
}

/* $A414: leave: the stage back, fade in, RTS to the pickup dispatch */
static void shop_exit(void)
{
    u8 i;
    ZP(0x97) = 0;
    ss2_fade_out_alt();                      /* $7211 */
    for (i = 0; i < 6; i++) ZP(0x45 + i) = ZP(0x57 + i);
    vdc();                                   /* $A649 BAT, $BAAF SATB cleared */
    ss2_shop_screen = 0;
    /* $7882: the stage's BG blocks back into VRAM (presentation; MPR6 is
     * reset by the $BB83 below). It holds the CPU across 3 VBlanks ($2FA0
     * counts the blocks of a record meanwhile; not modelled). */
    ss2_busy(3);
    R(0x2FA0) = 0;
    /* $EBE1: the redraw ends within a few hundred cycles of a VBlank, so
     * in the tracer it crosses 5 or 6 VBlanks (9 of 17 exits measured: 5)
     * depending on the interrupt load; 5 as for doors */
    ss2_redraw_request = 1;
    ss2_busy(5);
    ss2_sfx((u8)(3 + Z_STAGE));              /* stage music */
    ss2_fade_in();                           /* $71BA */
    ss2_map6(npc_bank);
}

/* $9E7B: the shop interior */
static void shop(void)
{
    u8 i, x, s;
    for (i = 0; i < 6; i++) ZP(0x57 + i) = ZP(0x45 + i);
    ss2_fade_out();                          /* $71D7 */
    vdc();                                   /* $A649, $BAAF */
    for (i = 0; i < 6; i++) ZP(0x45 + i) = 0;
    /* $7A5E: shop BG blocks from banks $1E/$1F (presentation) */
    R(0x2FA0) = 0;
    ss2_page[6] = ss2_banks[ZP(0x8E) & 0x1F];
    ss2_mpr6 = ZP(0x8E);
    ss2_shop_screen = 1;
    /* $FBB1 picture, $A4CA x2 frames (presentation); the BAT clear, the
     * block load and these cross one VBlank (during the second $A4CA) */
    ss2_busy(1);
    x = ZP(0xB8);
    O(0x245A, x) = 0;
    s = O(0x281A, x);
    O(0x2B5A, s) = 0;
    O(0x2C1A, s) = 0xE1;
    /* $9EF0-$9FA9: shopkeeper sprite and goods icons (renderer, from
     * ss2_shop_type / ss2_shop_goods) */
    ss2_shop_type = O(0x297A, x);
    ss2_shop_goods = goods();
    staff_digit();
    draw_prices();                           /* $A5FA */
    ss2_sfx(0x01);                           /* shop music */
    ss2_fade_in_alt();                       /* $71F4 */
    ss2_map6(8);
    ZP(0x8F) = 0x12;
    ZP(0x91) = 0x0A;
    text_at(0x6375);
    ss2_msg_text();                          /* $69EE */
    ss2_wait(0x3C);                          /* $B9D8 */
    ZP(0x97) = 0x58;
    ZP(0xB9) = 0;
    for (;;)
    {
        u16 g;
        /* $9FD5: cursor (presentation $A514), the description */
        vdc();
        g = goods();
        if (ZP(0xB9) == 4) text_at(0xA89B);
        else
        {
            u8 k = ss2_rom_byte((u16)(g + (u8)((ZP(0xB9) << 2) + 1)));
            text_at((u16)(ss2_rom_byte((u16)(0xA87F + (u8)(k << 1)))
                        | (ss2_rom_byte((u16)(0xA880 + (u8)(k << 1))) << 8)));
        }
        ss2_msg_text();
        for (;;)                             /* $A000 */
        {
            u8 p = Z_PRESS, b = ZP(0xB9), moved = 0;
            ZP(0x37) = p;
            if (p & 0xF0)
            {
                if (p == 0x20) { if (b != 1 && b != 4) { ZP(0xB9)++; moved = 1; } }
                else if (p == 0x80) { if (b != 0 && b != 2) { ZP(0xB9)--; moved = 1; } }
                else if (p == 0x10) { if (b != 0 && b != 1 && b != 4) { ZP(0xB9) = (u8)(b - 2); moved = 1; } }
                else if (p == 0x40) { if (b < 2) { ZP(0xB9) = (u8)(b + 2); moved = 1; } }
                if (moved) break;            /* $9FD5 */
            }
            else if (p & 0x03)
            {
                u8 y;
                u16 price;
                if (b == 4) { shop_exit(); return; }
                g = goods();
                y = (u8)((b << 2) + 1);
                ZP(0x37) = ss2_rom_byte((u16)(g + y));
                price = (u16)(ss2_rom_byte((u16)(g + (u8)(y + 1)))
                            | (ss2_rom_byte((u16)(g + (u8)(y + 2))) << 8));
                ZP(0x2A) = (u8)price; ZP(0x2B) = (u8)(price >> 8);
                buy(ZP(0x37), price);
                do shop_frame(); while (!(Z_PRESS & 0xF3));   /* $A460 */
                continue;                    /* JMP $A000 */
            }
            shop_frame();                    /* $A087 */
        }
    }
}

/* ---- type 0: townspeople and shopkeepers ($99AE, table $99B5) ---------- */

static void type0(void)
{
    static const u16 talk[7][2] = {
        { 0x5A13, 0x5A13 }, { 0x5A2A, 0x5A2A }, { 0x5A3A, 0x5A52 }, { 0x5A74, 0x5A74 },
        { 0x5A8B, 0x5A8B }, { 0x5AA5, 0x5AC4 }, { 0x5ADE, 0x5AFF } };
    u8 s = O(0x281A, ZP(0xB8)), st = Z_STAGE;
    if (st > 6) { ss2_unported(0x99B2); return; }
    if (s < 0x4A) { shop(); return; }        /* $9E7B */
    text_at(talk[st][s != 0x4A]);
    finish();
}

/* ---- type 1 ($9A8B, table $9A92) ---------------------------------------- */

static void type1(void)
{
    u8 z;
    switch (Z_STAGE)
    {
        case 0:                              /* $9AA0 */
            mark_7a();
            text_at(0x5B2A);
            finish();
            return;
        case 1:                              /* $9AAE: a gift, later a sale */
            z = !ZP(0x7A);
            mark_7a();
            text_at(0x5B6F);
            ZP(0x7E) = 7;
            if (!z) { ZP(0x7E) = 0; text_at(0x5B97); }
            finish();
            return;
        case 2:                              /* $9B20 */
            mark_7a();
            text_at(0x5B97);
            if (ss2_rom_byte((u16)(0x4CAB + (u8)(Z_STAGE - 1))) <= Z_STAFFPAL)
                text_at(0x5C44);
            finish();
            return;
        case 3:                              /* $9B67: trades the $40 item */
            z = !ZP(0x7A);
            mark_7a();
            if (R(0x22C1) & 0x40)
            {
                text_at(z ? 0x5C99 : 0x5D15);
                say();
                R(0x22C1) &= (u8)~0x40;
                ss2_hud_text(0x4EB3);
                ZP(0x7E) = 0x1A;
                text_at(z ? 0x5CD1 : 0x5D35);
            }
            else if (z) { ZP(0x7E) = 8; text_at(0x5CE5); }
            else text_at(0x5D49);
            finish();
            return;
        case 4:                              /* $9BEA: summons the boss */
        {
            u8 x;
            mark_7a();
            text_at(0x5D7C);
            say();
            ZP(0x7F) = 1;
            ss2_map6(0x17);
            x = ZP(0xB8);
            ZP(0x44) = O(0x281A, x);
            ZP(0x41) = (u8)(O(0x249A, x) + 0x10);
            ZP(0x40) = 0;
            ZP(0x43) = (u8)(O(0x24FA, x) + 0x10);
            ZP(0x42) = 0;
            ss2_spawn_handler(0x17, 0xC2A4);
            ss2_map6(npc_bank);
            ss2_obj_death(ZP(0xB8));
            return;
        }
        case 5:                              /* $9C75 */
            mark_7a();
            text_at(0x5E30);
            ZP(0x7E) = 0x0E;
            finish();
            return;
        case 6:                              /* $9CAE */
            mark_7a();
            shop();
            return;
    }
    ss2_unported(0x9A8F);
}

/* ---- type 2 ($9CB4, table $9CBB) ---------------------------------------- */

static void type2(void)
{
    u8 z;
    switch (Z_STAGE)
    {
        case 0: case 1:                      /* $9CC9 */
            mark_7b();
            text_at(0x5EDB);
            finish();
            return;
        case 2:                              /* $9CD7 */
            ZP(0x7E) = ZP(0x7B);
            z = !ZP(0x7B);
            mark_7b();
            text_at(z ? 0x5F19 : 0x5F70);
            finish();
            return;
        case 3:                              /* $9D55 */
            mark_7b();
            text_at(0x5FA9);
            finish();
            return;
        case 4:                              /* $9D63 */
            z = !ZP(0x7B);
            mark_7b();
            if (!z) { shop(); return; }
            ZP(0x7E) = 9;
            text_at(0x5FD6);
            finish();
            return;
        case 5:                              /* $9D7E: fortune teller */
            mark_7b();
            text_at(0x6005);
            finish();
            return;
        case 6:                              /* $9DBC: trades the $20 item */
            mark_7b();
            if (R(0x22C1) & 0x20)
            {
                text_at(0x608C);
                say();
                R(0x22C1) &= (u8)~0x20;
                ss2_hud_text(0x4EB3);
                ZP(0x7E) = 0x1C;
                text_at(0x60B8);
            }
            else text_at(0x60EF);
            finish();
            return;
    }
    ss2_unported(0x9CB8);
}

/* bank 3 $997F: item kind 29, touching a townsperson */
void ss2_npc(u8 x)
{
    if (R(0x22AA) | R(0x22B1) | R(0x22B0)) return;
    ZP(0x7E) = 0;
    O(0x245A, x) = 0x40;
    ZP(0x30) = O(0x297A, x);
    ZP(0xB8) = x;
    npc_bank = ZP(0x8E);
    ss2_map6(8);
    switch (ZP(0x30))
    {
        case 0: type0(); return;
        case 1: type1(); return;
        case 2: type2(); return;
    }
    ss2_unported(0x99A5);
}

/* ---- continuations named by message text after a choice ($96) ---------- */

u8 ss2_npc_continue(u16 addr, u8 y)
{
    (void)y;
    switch (addr)
    {
        case 0x9AD0:                         /* buy the staff upgrade ($4C44) */
            if (ZP(0x96)) text_at(0x5C29);
            else if (price_cmp(0x4C44, 0x9AE4) == 2) text_at(0x5C00);
            else
            {
                text_at(0x5BF5);             /* $9AF8 */
                ZP(0x7E) = 0x19;
                pay(0x4C44);
                hud_money();
            }
            ss2_msg_text();
            return 1;
        case 0x9B45:
            if (!ZP(0x96)) { ZP(0x7E) = 0x08; text_at(0x5C66); }
            else { ZP(0x7E) = 0x12; text_at(0x5C7C); }
            ss2_msg_text();
            return 1;
        case 0x9C28:                         /* pay $4C64 or lose it all */
            if (ZP(0x96)) text_at(0x5DFD);
            else
            {
                if (price_cmp(0x4C64, 0x9C3C) == 0) pay(0x4C64);   /* $9C51 */
                else money_clear();          /* $9C45 */
                hud_money();
                text_at(0x5DBB);
            }
            ss2_msg_text();
            return 1;
        case 0x9C87:
            if (!ZP(0x96)) text_at(0x5E62);
            else
            {
                money_clear();
                hud_money();
                text_at(0x5E97);
            }
            ss2_msg_text();
            return 1;
        case 0x9CF5:                         /* buy: $4C14, or $4C1C again */
            if (ZP(0x96)) { ZP(0x7E) = 0; text_at(0x5F5D); }
            else
            {
                u16 p = ZP(0x7E) ? 0x4C1C : 0x4C14;
                if (price_cmp(p, 0x9D15) == 2) { ZP(0x7E) = 0; text_at(0x5C00); }
                else
                {
                    pay(p);                  /* $9D2B */
                    hud_money();
                    ZP(0x7E) = 0x1B;
                    text_at(0x5F52);
                }
            }
            ss2_msg_text();
            return 1;
        case 0x9D8C:                         /* fortune: an item, if lucky */
        {
            u8 it = !ZP(0x96) ? 0x0F : ZP(0x96) == 1 ? 0x10 : 0x0C;
            if (ZP(0x8D) & 3) { ZP(0x7E) = it; text_at(0x6056); }
            else { ZP(0x7E) = 0; text_at(0x6076); }
            ss2_msg_text();
            return 1;
        }
    }
    return 0;
}

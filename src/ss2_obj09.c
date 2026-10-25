/* SON SON II bank $09 objects, rebuilt from HuCard bank 9 $DC6F-$DFBF
 * (the player code at $C000-$DB.. lives in ss2_player.c).
 *
 *   $DC6F  dropper: up to 8 objects placed at random columns on the
 *          ground row, each waiting $20 frames, then falling ($DD39/$DD69)
 *   $DDCA  popper: thrown up at vy -6, scores when hit ($DE33/$DE60)
 *   $DE81/$DE85  flyer: crosses the screen from the left/right edge at the
 *          player's height on a vy table ($DF57/$DF98)
 *
 * Extra per-object fields used here: $297A timer; flyer: $29BA:$299A
 * (lo:hi) vy table pointer.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"

/* field a of slot x through o = &ss2_ram[x] (a fixed displacement) */
#define OF(a) (o[(u16)((a) - 0x2000)])

static void set_vx(u8 x, u16 v) { O(0x25BA, x) = (u8)(v >> 8); O(0x25DA, x) = (u8)v; }
static void set_vy(u8 x, u16 v) { O(0x25FA, x) = (u8)(v >> 8); O(0x261A, x) = (u8)v; }
static void set_script(u8 x, u16 p) { O(0x295A, x) = (u8)p; O(0x293A, x) = (u8)(p >> 8); }
static void set_handler(u8 x, u16 h) { O(0x28DA, x) = (u8)h; O(0x28BA, x) = (u8)(h >> 8); }
static void set_hit(u8 x, u16 h) { O(0x291A, x) = (u8)h; O(0x28FA, x) = (u8)(h >> 8); }

/* the $2CDA per-spawner palette nibble shared by the spawns */
static void spawner_nibble(u8 s, u8 x)
{
    u8 *const o = &ss2_ram[x];
    if (!O(0x2CDA, s)) O(0x2CDA, s) = (u8)(OF(0x289A) & 0x0F);
    else OF(0x289A) = (u8)((OF(0x289A) & 0xF0) | O(0x2CDA, s));
}

/* ---- $DC6F dropper ---------------------------------------------------- */

static void dropper_spawn(void)
{
    u8 s = ZP(0x44), x;
    if (Z_INLOCK1 || ZP(0x71)) return;
    if (O(0x2C1A, s)) return;
    O(0x2C1A, s) = 4;
    /* $DC83: align the spawn row to the 16 px grid (camera relative) */
    ZP(0x43) = (u8)((u8)(((u8)((u8)(Z_CAMY_LO + ZP(0x43)) + 0x0A) | 0x0F) - 0x0A) - Z_CAMY_LO);
    ZP(0x36) = 0x18;
    do
    {
        /* $DC99 */
        if (O(0x2B5A, s) >= 5) return;
        ss2_random();
        if (!(ZP(0x8D) & 0x0F))
        {
            x = ss2_obj_alloc();
            if (x == 0xFF) return;
            O(0x247A, x) = 0;
            O(0x249A, x) = ZP(0x36);
            O(0x24DA, x) = ZP(0x42);
            O(0x24FA, x) = ZP(0x43);
            O(0x263A, x) = 0x08;
            O(0x265A, x) = 0x0A;
            /* $DCCB: climb in 16 px steps to the first solid block */
            for (;;)
            {
                u8 v;
                if (ss2_obj_probe(x)) break;
                v = O(0x24FA, x);
                O(0x24FA, x) = (u8)(v - 0x10);
                if (v < 0x10) goto next;
            }
            /* $DCDD: needs open space one pixel below */
            O(0x265A, x) = 0x0B;
            if (ss2_obj_probe(x)) goto next;
            O(0x265A, x) = 0x10;
            ss2_obj_init(x, 0xDD23);
            set_vx(x, 0);
            set_vy(x, 0);
            O(0x297A, x) = 0x20;
            O(0x287A, x) = 0;
            O(0x281A, x) = s;
            O(0x2B5A, s)++;
        }
    next:
        /* $DD14 */
        ZP(0x36) = (u8)(ZP(0x36) + 0x20);
    } while (ZP(0x36) != 0x18);
}

/* $DD39: wait, then drop 7 px and start falling */
static void dropper(u8 x)
{
    u8 *const o = &ss2_ram[x];
    if (--OF(0x297A)) { ss2_obj_common(x); return; }
    {
        u16 t = (u16)(OF(0x24FA) + 7);
        OF(0x24FA) = (u8)t;
        OF(0x24DA) = (u8)(OF(0x24DA) + (t >> 8));
    }
    set_handler(x, 0xDD69);
    set_script(x, 0xDDC2);
    OF(0x267A) = 1;
    ss2_obj_common(x);
}

/* $DD69: fall until something solid is met */
static void dropper_fall(u8 x)
{
    u8 *const o = &ss2_ram[x];
    u16 t = (u16)(OF(0x261A) + 0x30);
    OF(0x261A) = (u8)t;
    OF(0x25FA) = (u8)(OF(0x25FA) + (t >> 8));
    if (ss2_obj_probe(x)) { ss2_obj_die(x); return; }
    ss2_obj_common(x);
}

/* $DD85: hit reaction / death */
static void dropper_hit(u8 x)
{
    u8 *const o = &ss2_ram[x];
    if (OF(0x245A) != 0x40)
    {
        OF(0x245A) = 0x80;
        ss2_obj_common(x);
        return;
    }
    O(0x2B5A, OF(0x281A))--;
    set_vy(x, 0);
    OF(0x267A) = 1;
    set_script(x, 0xDDC7);
    set_hit(x, 0xEE67);
    ss2_obj_common(x);
}

/* ---- $DDCA popper ----------------------------------------------------- */

static void popper_spawn(void)
{
    u8 s = ZP(0x44), x;
    if (O(0x2C1A, s) | ZP(0x71)) return;
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xDE1D);
    set_vy(x, 0xFA00);
    set_vx(x, 0);
    O(0x247A, x) = ZP(0x40);
    O(0x249A, x) = ZP(0x41);
    O(0x24DA, x) = ZP(0x42);
    O(0x24FA, x) = ZP(0x43);
    O(0x287A, x) = 0;
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    O(0x2C1A, s) = 7;
    ss2_obj_common(x);
}

/* $DE33: gravity; switch to the falling script once descending */
static void popper(u8 x)
{
    u8 *const o = &ss2_ram[x];
    u16 t = (u16)(OF(0x261A) + 0x20);
    OF(0x261A) = (u8)t;
    OF(0x25FA) = (u8)(OF(0x25FA) + (t >> 8));
    if ((s8)OF(0x25FA) >= 0 && OF(0x295A) < 0x7A) set_script(x, 0xDE7A);
    ss2_obj_common(x);
}

/* $DE60: every hit scores; it never dies of damage */
static void popper_hit(u8 x)
{
    u8 *const o = &ss2_ram[x];
    OF(0x245A) = 0x80;
    ss2_score_award(OF(0x279A));
    OF(0x26DA) = 0x64;
    ss2_obj_common(x);
}

/* ---- $DE81/$DE85 flyer ------------------------------------------------ */

static void flyer_spawn(u8 from_right)
{
    u8 s = ZP(0x44), x;
    ZP(0x35) = from_right;
    if (O(0x2C1A, s) | ZP(0x71)) return;
    ZP(0x30) = (u8)(P_XL - 0x30);
    if ((u8)(ZP(0x41) - ZP(0x30)) >= 0x60) return;
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xDF41);
    O(0x247A, x) = 0xFF;
    O(0x249A, x) = 0xF0;
    if (from_right) { O(0x247A, x) = 0x01; O(0x249A, x) = 0x00; }
    O(0x24FA, x) = P_YL;
    O(0x24DA, x) = P_YH;
    O(0x261A, x) = 0;
    set_vx(x, from_right ? 0xFEE0 : 0x0120);
    if (from_right) set_script(x, 0xDFB8);
    O(0x297A, x) = 1;
    O(0x29BA, x) = 0xA4;
    O(0x299A, x) = 0xDF;
    O(0x287A, x) = 0;
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    O(0x2C1A, s) = 0x0A;
    spawner_nibble(s, x);
}

/* $DF57: step through the (frames, vy hi) table at $29BA:$299A;
 * a 0 entry is a jump to the following pointer */
static void flyer(u8 x)
{
    u8 *const o = &ss2_ram[x];
    u16 p;
    if (--OF(0x297A)) { ss2_obj_common(x); return; }
    for (;;)
    {
        p = (u16)((OF(0x299A) << 8) | OF(0x29BA));
        ZP(0x26) = (u8)p; ZP(0x27) = (u8)(p >> 8);
        if (ss2_rom_byte(p)) break;
        OF(0x29BA) = ss2_rom_byte((u16)(p + 1));
        OF(0x299A) = ss2_rom_byte((u16)(p + 2));
    }
    OF(0x297A) = ss2_rom_byte(p);
    OF(0x25FA) = ss2_rom_byte((u16)(p + 1));
    p = (u16)(p + 2);
    OF(0x29BA) = (u8)p;
    OF(0x299A) = (u8)(p >> 8);
    ss2_obj_common(x);
}

/* $DF98: release the spawner and run the common death ($6E47) */
static void flyer_hit(u8 x)
{
    O(0x2B5A, O(0x281A, x))--;
    ss2_obj_death(x);
}

u8 ss2_obj09_handler(u16 addr, u8 x)
{
    switch (addr)
    {
        case 0xDD39: dropper(x); return 1;
        case 0xDD69: dropper_fall(x); return 1;
        case 0xDD85: dropper_hit(x); return 1;
        case 0xDE33: popper(x); return 1;
        case 0xDE60: popper_hit(x); return 1;
        case 0xDF57: flyer(x); return 1;
        case 0xDF98: flyer_hit(x); return 1;
    }
    return 0;
}

u8 ss2_spawn09_handler(u16 addr)
{
    switch (addr)
    {
        case 0xDC6F: dropper_spawn(); return 1;
        case 0xDDCA: popper_spawn(); return 1;
        case 0xDE81: flyer_spawn(0); return 1;
        case 0xDE85: flyer_spawn(1); return 1;
    }
    return 0;
}

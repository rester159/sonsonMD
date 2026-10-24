/* SON SON II bank $17 objects (stage 1 ground enemies and their shots),
 * rebuilt from HuCard bank 23 $CE60-$D20C.
 *
 * Extra per-object fields used here: $297A attack pause, $299A facing
 * (1 = right), $29BA facing last frame, $29DA turn timer, $29FA attack
 * timer, $2A1A knockback timer, $2A3A knockback latch, $2A5A will throw,
 * $2A7A turn request (edge ahead).
 */
#include "ss2_ram.h"
#include "ss2_logic.h"

#define O(a, x)     R((u16)((a) + (x)))
#define OX16(x)     ((u16)((O(0x247A, x) << 8) | O(0x249A, x)))
#define OY16(x)     ((u16)((O(0x24DA, x) << 8) | O(0x24FA, x)))

static void set_vx(u8 x, u16 v) { O(0x25BA, x) = (u8)(v >> 8); O(0x25DA, x) = (u8)v; }
static void set_vy(u8 x, u16 v) { O(0x25FA, x) = (u8)(v >> 8); O(0x261A, x) = (u8)v; }
static void set_script(u8 x, u16 p) { O(0x295A, x) = (u8)p; O(0x293A, x) = (u8)(p >> 8); }
static u8 vy_zero(u8 x) { return !(O(0x25FA, x) | O(0x261A, x)); }

static void ground(u8 x);

/* $CE72: common spawn for the walker types */
static void spawn_walker(u16 rec)
{
    u8 s = ZP(0x44), x;
    if (O(0x2B5A, s) | O(0x2C1A, s)) return;
    if (ss2_spawn_visible()) return;
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_obj_init(x, rec);
    O(0x247A, x) = ZP(0x40);
    O(0x249A, x) = ZP(0x41);
    O(0x24DA, x) = ZP(0x42);
    O(0x24FA, x) = ZP(0x43);
    set_vx(x, 0);
    set_vy(x, 0);
    O(0x287A, x) = 1;
    O(0x29DA, x) = 1;
    O(0x29BA, x) = 0;
    O(0x297A, x) = 0;
    O(0x2A3A, x) = 0;
    O(0x2A7A, x) = 0;
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    if (!O(0x2CDA, s)) O(0x2CDA, s) = (u8)(O(0x289A, x) & 0x0F);
    else O(0x289A, x) = (u8)((O(0x289A, x) & 0xF0) | O(0x2CDA, s));
    ss2_obj_common(x);
}

/* $D025: start the walk script when the facing changed */
static void face_script(u8 x)
{
    if (O(0x297A, x) || O(0x299A, x) != O(0x29BA, x))
    {
        O(0x297A, x) = 0;
        O(0x267A, x) = 1;
        set_script(x, O(0x299A, x) ? 0xD25E : 0xD251);
    }
    ground(x);
}

/* $D0DA */
static void finish(u8 x)
{
    O(0x29BA, x) = O(0x299A, x);
    ss2_obj_common(x);
}

/* $D055: gravity, landing, hop over attribute-3 steps, stop at edges */
static void ground(u8 x)
{
    if ((s8)O(0x25FA, x) >= 0)
    {
        O(0x263A, x) = 0x10;
        O(0x265A, x) = 0x20;
        if (ss2_obj_probe(x))
        {
            /* $D07C */
            if (Z_HIT_HURT) { ss2_obj_die(x); return; }
            ss2_obj_snap_y(x);
            set_vy(x, 0);
            O(0x263A, x) = (s8)O(0x25BA, x) < 0 ? 0x08 : 0x18;
            O(0x265A, x) = 0x10;
            if (ss2_obj_probe(x))
            {
                if (vy_zero(x) && Z_HIT_3) { set_vy(x, 0xFD80); finish(x); return; }
            }
            else
            {
                if (!vy_zero(x)) { finish(x); return; }
                O(0x265A, x) = 0x30;
                if (ss2_obj_probe(x)) { finish(x); return; }
                O(0x2A7A, x) = 1;
            }
            set_vx(x, 0);
            finish(x);
            return;
        }
    }
    {
        u16 t = (u16)(O(0x261A, x) + 0x30);
        O(0x261A, x) = (u8)t;
        O(0x25FA, x) = (u8)(O(0x25FA, x) + (t >> 8));
    }
    finish(x);
}

static void walk_left(u8 x)  { O(0x299A, x) = 0; set_vx(x, 0xFF00); face_script(x); }   /* $CF28 */
static void walk_right(u8 x) { O(0x299A, x) = 1; set_vx(x, 0x0100); face_script(x); }   /* $CF38 */

/* $CEE3: patrolling walker; re-aims at the player every $3C frames */
static void walker(u8 x)
{
    if (O(0x2A7A, x))
    {
        O(0x2A7A, x) = 0;
        O(0x29DA, x) = 0x40;
        if (O(0x299A, x)) walk_left(x); else walk_right(x);
        return;
    }
    if (!vy_zero(x) || --O(0x29DA, x)) { ground(x); return; }
    O(0x29DA, x) = 0x3C;
    if ((u16)(((u8)(P_XH + 1) << 8) | P_XL) >= (u16)(((u8)(O(0x247A, x) + 1) << 8) | O(0x249A, x)))
        walk_right(x);
    else
        walk_left(x);
}

/* $D16E/$D174: throw a shot from parent p */
static void throw_shot(u8 p)
{
    u8 x;
    ZP(0x37) = p;
    x = ss2_obj_alloc_top();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xD239);
    if (!O(0x299A, p))
    {
        set_script(x, 0xD269);
        O(0x263A, x) = 0;
        set_vx(x, 0xFE00);
        O(0x249A, x) = O(0x249A, p);
        O(0x247A, x) = O(0x247A, p);
    }
    else
    {
        u16 v = (u16)(OX16(p) + 0x10);
        O(0x263A, x) = 0x0F;
        set_vx(x, 0x0200);
        O(0x249A, x) = (u8)v;
        O(0x247A, x) = (u8)(v >> 8);
    }
    O(0x265A, x) = 0x08;
    set_vy(x, 0);
    {
        u16 v = (u16)(OY16(p) + 8);
        O(0x24FA, x) = (u8)v;
        O(0x24DA, x) = (u8)(v >> 8);
    }
    O(0x287A, x) = 0;
    O(0x281A, x) = 0xBF;
    ss2_obj_common(x);
}

/* $CF4A: keeps $4E-$5F px from the player, pausing to throw */
static void thrower(u8 x)
{
    u16 d;
    u8 left;
    if (!vy_zero(x)) { ground(x); return; }
    if (O(0x297A, x) && --O(0x29FA, x))
    {
        /* $CFE9 */
        if (O(0x2A5A, x) && O(0x29FA, x) == 0x0F) throw_shot(x);
        finish(x);
        return;
    }
    {
        u16 ox = (u16)(((u8)(O(0x247A, x) + 1) << 8) | O(0x249A, x));
        u16 px = (u16)(((u8)(P_XH + 1) << 8) | P_XL);
        d = (u16)(ox - px);
        left = 0;
        if (ox < px) { left = 1; d = (u16)(0 - d); }
        ZP(0x28) = (u8)d; ZP(0x29) = (u8)(d >> 8);
        ZP(0x37) = left;
    }
    if (d >= 0x60)
    {
        /* $CFFD: approach */
        if (left) { O(0x299A, x) = 1; set_vx(x, 0x0180); }
        else { O(0x299A, x) = 0; set_vx(x, 0xFE80); }
        face_script(x);
        return;
    }
    if (d < 0x4E)
    {
        /* $D003: back off */
        if (left) { O(0x299A, x) = 0; set_vx(x, 0xFE80); }
        else { O(0x299A, x) = 1; set_vx(x, 0x0180); }
        face_script(x);
        return;
    }
    /* $CFA4: in range: pause and maybe throw */
    O(0x297A, x) = 1;
    O(0x29FA, x) = 0x1E;
    O(0x299A, x) = left;
    set_script(x, left ? 0xD25C : 0xD24F);
    O(0x267A, x) = 1;
    set_vx(x, 0);
    ss2_random();
    O(0x2A5A, x) = 0;
    if ((ZP(0x8D) & 0x0F) < 3)
    {
        O(0x2A5A, x)++;
        throw_shot(x);
    }
    finish(x);
}

/* $D0E3: walker/thrower hit reaction and death */
static void walker_hit(u8 x)
{
    if (O(0x245A, x) == 0x40)
    {
        u8 s = O(0x281A, x);
        O(0x2B5A, s)--;
        O(0x2C1A, s) = 0x64;
        ss2_obj_death(x);
        return;
    }
    if (!O(0x2A3A, x))
    {
        O(0x2A3A, x)++;
        O(0x2A1A, x) = 8;
        if ((u16)((P_XH << 8) | P_XL) >= OX16(x))
        {
            O(0x299A, x) = 1;
            set_script(x, 0xD25C);
            set_vx(x, 0xFC80);
        }
        else
        {
            O(0x299A, x) = 0;
            set_script(x, 0xD24F);
            set_vx(x, 0x0380);
        }
    }
    if (O(0x2A1A, x)) O(0x2A1A, x)--;
    else if (vy_zero(x))
    {
        O(0x245A, x) = 0x80;
        O(0x297A, x) = 0;
        O(0x29DA, x) = 1;
        O(0x2A3A, x) = 0;
    }
    ground(x);
}

/* $D202: shot: dies against walls */
static void shot(u8 x)
{
    if (ss2_obj_probe(x)) { ss2_obj_die(x); return; }
    ss2_obj_common(x);
}

/* $D1F0: shot hit reaction */
static void shot_hit(u8 x)
{
    if (O(0x245A, x) != 0x20) { O(0x245A, x) = 0; return; }
    O(0x245A, x) = 0x80;
    shot(x);
}

u8 ss2_obj17_handler(u16 addr, u8 x)
{
    switch (addr)
    {
        case 0xCEE3: walker(x); return 1;
        case 0xCF4A: thrower(x); return 1;
        case 0xD0E3: walker_hit(x); return 1;
        case 0xD1F0: shot_hit(x); return 1;
        case 0xD202: shot(x); return 1;
    }
    return 0;
}

u8 ss2_spawn17_handler(u16 addr)
{
    switch (addr)
    {
        case 0xCE60: spawn_walker(0xD20D); return 1;
        case 0xCE6A: spawn_walker(0xD223); return 1;
    }
    return 0;
}

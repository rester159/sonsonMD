/* SON SON II objects in HuCard bank $05 ($C000-$DFFF), rebuilt from bank 5
 * $C000-$DCA9 (records and scripts are read from the ROM bank).
 *
 *   $C000-$C3EF  ceiling droppers ($C164 -> $C1C0) that walk ($C294) or
 *                split ($C222 + $C0A0 child), and vine climbers ($C32D)
 *   $C408-$C598  bats: bob until the player is near ($C4C8), then fly in
 *                one of 16 directions ($C50B, bank 2 $7BEA)
 *   $C5C7-$C908  hoppers: hop in place, then chase ($C70C) and flee when
 *                at half health ($C810)
 *   $C909-$CBF0  rock throwers: keep $40-$60 px away, throw ($CB17)
 *   $CBF8-$CF3D  pouncers: fall, walk, crouch and leap ($CD5F/$CE20)
 *   $CF3E-$D46B  stage 5/7 bosses (two kinds) and their shots
 *   $D8A1-$DCA9  stage 6/7 boss and its shots / falling debris
 *
 * Common extra fields: $277A facing (1 = player is to the left) or 16-way
 * direction, $297A variant / step flag, $299A timer or mode, $29BA-$29FA
 * timers, $2A1A latch.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"


#define OX16(x)     ((u16)((O(0x247A, x) << 8) | O(0x249A, x)))
#define OY16(x)     ((u16)((O(0x24DA, x) << 8) | O(0x24FA, x)))
#define PX16        ((u16)((P_XH << 8) | P_XL))

static void set_ox16(u8 x, u16 v) { O(0x247A, x) = (u8)(v >> 8); O(0x249A, x) = (u8)v; }
static void set_oy16(u8 x, u16 v) { O(0x24DA, x) = (u8)(v >> 8); O(0x24FA, x) = (u8)v; }
static void set_vx(u8 x, u16 v) { O(0x25BA, x) = (u8)(v >> 8); O(0x25DA, x) = (u8)v; }
static void set_vy(u8 x, u16 v) { O(0x25FA, x) = (u8)(v >> 8); O(0x261A, x) = (u8)v; }
static u16 get_vx(u8 x) { return (u16)((O(0x25BA, x) << 8) | O(0x25DA, x)); }
static u16 get_vy(u8 x) { return (u16)((O(0x25FA, x) << 8) | O(0x261A, x)); }
static void set_script(u8 x, u16 p) { O(0x295A, x) = (u8)p; O(0x293A, x) = (u8)(p >> 8); }
static void set_handler(u8 x, u16 h) { O(0x28DA, x) = (u8)h; O(0x28BA, x) = (u8)(h >> 8); }
static void probe_at(u8 x, u8 px, u8 py) { O(0x263A, x) = px; O(0x265A, x) = py; }
static u8 probe(u8 x, u8 px, u8 py) { probe_at(x, px, py); return ss2_obj_probe(x); }

static void place(u8 x)
{
    O(0x247A, x) = ZP(0x40); O(0x249A, x) = ZP(0x41);
    O(0x24DA, x) = ZP(0x42); O(0x24FA, x) = ZP(0x43);
}

/* vy += a (8.8, low byte add with carry into the high byte) */
static void fall(u8 x, u8 a)
{
    u16 t = (u16)(O(0x261A, x) + a);
    O(0x261A, x) = (u8)t;
    O(0x25FA, x) = (u8)(O(0x25FA, x) + (t >> 8));
}

/* common spawn epilogue: spawner link, live count and drop table shared
 * by the spawner ($C079 and copies) */
static void link_spawner(u8 x)
{
    u8 s = ZP(0x44);
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    if (!O(0x2CDA, s)) O(0x2CDA, s) = (u8)(O(0x289A, x) & 0x0F);
    else O(0x289A, x) = (u8)((O(0x289A, x) & 0xF0) | O(0x2CDA, s));
}

/* spawn guard ($C036 and copies): no live object, no respawn delay, off
 * screen. Returns 1 when the spawn may proceed. */
static u8 spawn_ok(void)
{
    u8 s = ZP(0x44);
    if (O(0x2B5A, s) | O(0x2C1A, s)) return 0;
    if (ss2_spawn_visible()) return 0;
    return 1;
}

/* death epilogue ($C8E3 and copies): release the spawner with a respawn
 * delay, then explode */
static void die_respawn(u8 x)
{
    u8 s = O(0x281A, x);
    O(0x2B5A, s)--;
    O(0x2C1A, s) = 0x64;
    ss2_obj_death(x);
}

/* hit handler shared by several enemies ($C579, $C8CF, $CAAE head, $CEED):
 * recover, or release the spawner and explode */
static void hit_recover(u8 x)
{
    if (O(0x245A, x) != 0x40) { O(0x245A, x) = 0x80; ss2_obj_common(x); return; }
    die_respawn(x);
}

/* $C8B8/$CED6: $277A = 1 when the object is not left of the player
 * (compares ($247A+1):$249A with $01:$2299) */
static void face(u8 x)
{
    u8 b = O(0x249A, x) < P_XL;
    u8 a = (u8)(O(0x247A, x) + 1);
    O(0x277A, x) = a >= (u8)(1 + b);
}

/* "lo + k - px - !carry < lim" proximity test used with x hi = 0 */
static u8 near_x(u8 x, u8 k, u8 lim)
{
    u16 t = (u16)(O(0x249A, x) + k);
    u8 v = (u8)((u8)t - P_XL - (1 - (t >> 8)));
    return v < lim;
}

/* ---- bank 3 $85E7: 16-way direction from the object to the player ---- */
static u8 dir_to_player(u8 x)
{
    u8 y = 0;
    u16 dx, dy, ox, oy, a2, a3, b2, b3, b4;
    u16 px = (u16)(((u8)(P_XH + 1 + ((u16)(P_XL + 0x10) >> 8)) << 8) | (u8)(P_XL + 0x10));
    u16 py = (u16)(((u8)(P_YH + 1 + ((u16)(P_YL + 0x10) >> 8)) << 8) | (u8)(P_YL + 0x10));
    if (!(O(0x269A, x) & 0x20))
    {
        u16 t = (u16)((u8)(((O(0x253A, x) & 1) + 1) << 3) + O(0x249A, x));
        ox = (u16)(((u8)(1 + O(0x247A, x) + (t >> 8)) << 8) | (u8)t);
        t = (u16)((u8)(((O(0x253A, x) & 0x30) + 0x10) >> 1) + O(0x24FA, x));
        oy = (u16)(((u8)(1 + O(0x24DA, x) + (t >> 8)) << 8) | (u8)t);
    }
    else
    {
        ox = (u16)(((u8)(O(0x247A, x) + 1) << 8) | O(0x249A, x));
        oy = (u16)(((u8)(O(0x24DA, x) + 1) << 8) | O(0x24FA, x));
    }
    dx = (u16)(px - ox);
    if (px < ox) { dx = (u16)~dx; y = 8; }
    dy = (u16)(py - oy);
    if (py < oy) { dy = (u16)~dy; y |= 0x10; }
    a2 = (u16)(dx << 1);
    a3 = (u16)(dx + a2);
    b2 = (u16)(dy << 1);
    b3 = (u16)(dy + b2);
    b4 = (u16)(b3 + b2);
    {
        u16 a5 = (u16)(a3 + a2);
        if (dx < b4)
        {
            y++;
            if (a2 < b3)
            {
                y++;
                if (a3 < b2)
                {
                    y++;
                    if (a5 < dy) y++;
                }
            }
        }
    }
    return ss2_rom_byte((u16)(0x8712 + y));
}

/* bank 2 $7BEA: velocity for 16-way direction $277A from table $27FA */
static void dir_velocity(u8 x)
{
    u8 t = O(0x27FA, x), y = (u8)(O(0x277A, x) << 2);
    u16 p = (u16)(ss2_rom_byte((u16)(0x7C1B + t)) | (ss2_rom_byte((u16)(0x7C1B + (u8)(t + 1))) << 8));
    O(0x25DA, x) = ss2_rom_byte((u16)(p + y));
    O(0x25BA, x) = ss2_rom_byte((u16)(p + (u8)(y + 1)));
    O(0x261A, x) = ss2_rom_byte((u16)(p + (u8)(y + 2)));
    O(0x25FA, x) = ss2_rom_byte((u16)(p + (u8)(y + 3)));
}

/* ======================================================================
 * $C000-$C3EF: ceiling droppers / vine climbers
 * ==================================================================== */

static void dropper_walk(u8 x);
static void dropper_fall(u8 x);

/* $C036 */
static void spawn_dropper(u16 rec, u8 kind)
{
    u8 x;
    ZP(0x35) = kind;
    if (!spawn_ok()) return;
    x = ss2_obj_alloc_top();
    if (x == 0xFF) return;
    ss2_obj_init(x, rec);               /* $C048 */
    O(0x297A, x) = ZP(0x35);
    place(x);
    set_vx(x, 0);
    set_vy(x, 0);
    O(0x287A, x) = 0;
    O(0x299A, x) = 1;
    link_spawner(x);
    ss2_obj_common(x);
}

/* $C164: wait on the ceiling until the player is within $50 px */
static void dropper_wait(u8 x)
{
    u16 a = (u16)(OX16(x) + 8), b = (u16)(PX16 + 0x10);
    u16 d = (u16)(a - b + 0x50);
    ZP(0x26) = (u8)d; ZP(0x27) = (u8)(d >> 8);
    if (d >= 0xA0) { ss2_obj_common(x); return; }
    O(0x267A, x) = 1;
    set_script(x, 0xC3F5);
    set_handler(x, 0xC1C0);
    dropper_fall(x);
}

static void dropper_split(u8 x);

/* $C1C0: fall to the ground, then walk (or split for kind 1) */
static void dropper_fall(u8 x)
{
    if (O(0x25FA, x) != 1) fall(x, 0x10);
    if (!probe(x, 0x08, 0x10)) { ss2_obj_common(x); return; }
    ss2_obj_snap_y(x);
    set_vy(x, 0);
    O(0x269A, x) = 0x08;
    if (O(0x297A, x) != 1)
    {
        set_handler(x, 0xC294);
        dropper_walk(x);
        return;
    }
    O(0x29BA, x) = 0x3C;
    O(0x29DA, x) = 0;
    O(0x29FA, x) = 4;
    set_handler(x, 0xC222);
    dropper_split(x);
}

static void dropper_jitter(u8 x);

/* $C0A0: split off a copy into a free slot below the parent */
static void dropper_child(u8 p)
{
    u8 x;
    for (x = p; ; )
    {
        if (!O(0x245A, x)) break;
        if (!--x) return;
    }
    ss2_obj_init(x, 0xC14E);            /* $C0AD */
    O(0x297A, x) = 1;
    O(0x247A, x) = O(0x247A, p);
    O(0x249A, x) = O(0x249A, p);
    O(0x24DA, x) = O(0x24DA, p);
    O(0x24FA, x) = O(0x24FA, p);
    set_vx(x, 0x0020);
    set_vy(x, 0);
    O(0x287A, x) = 0;
    O(0x299A, x) = 1;
    O(0x29DA, x) = 0x50;
    O(0x29FA, x) = O(0x29FA, p);
    O(0x289A, x) = O(0x289A, p);
    O(0x281A, x) = O(0x281A, p);
    O(0x2B5A, O(0x281A, x))++;
    dropper_jitter(x);
}

/* $C222: splits up to 4 times at random intervals */
static void dropper_split(u8 x)
{
    if (O(0x29DA, x)) { dropper_jitter(x); return; }
    if (--O(0x29BA, x)) { dropper_walk(x); return; }
    if (!--O(0x29FA, x))
    {
        set_handler(x, 0xC294);
        dropper_walk(x);
        return;
    }
    ss2_random();
    O(0x29BA, x) = (u8)(ZP(0x8D) | 0x80);
    O(0x29DA, x) = 0x50;
    O(0x299A, x) = 1;
    set_vx(x, 0xFFE0);
    set_script(x, 0xC3FA);
    O(0x267A, x) = 1;
    dropper_child(x);
    dropper_jitter(x);
}

static void dropper_edge(u8 x);

/* $C26F: shiver 2 px left/right while $29DA runs */
static void dropper_jitter(u8 x)
{
    u16 d = (Z_FRAME & 1) ? 0x0002 : 0xFFFE;
    ZP(0x26) = (u8)d; ZP(0x27) = (u8)(d >> 8);
    set_ox16(x, (u16)(OX16(x) + d));
    if (--O(0x29DA, x)) { dropper_edge(x); return; }
    dropper_walk(x);
}

/* $C294: turn toward the player every $3C frames */
static void dropper_walk(u8 x)
{
    if (!--O(0x299A, x))
    {
        u16 t, a, b;
        O(0x299A, x) = 0x3C;
        t = (u16)(O(0x249A, x) + 8);
        a = (u16)(((u8)(O(0x247A, x) + 1 + (t >> 8)) << 8) | (u8)t);
        t = (u16)(P_XL + 0x10);
        b = (u16)(((u8)(P_XH + 1 + (t >> 8)) << 8) | (u8)t);
        if (a >= b) { set_script(x, 0xC3FA); set_vx(x, 0xFF80); }
        else { set_script(x, 0xC401); set_vx(x, 0x0080); }
        O(0x267A, x) = 1;
    }
    dropper_edge(x);
}

/* $C2F6: turn around at walls and ledges */
static void dropper_edge(u8 x)
{
    u8 px = (s8)O(0x25BA, x) < 0 ? 0x02 : 0x0D;
    if (probe(x, px, 0x0F) || !probe(x, px, 0x10))
        set_vx(x, (u16)(0 - get_vx(x)));     /* $C31B */
    ss2_obj_common(x);
}

/* $C32D: vine climber: re-aims up/down every $3C frames, reverses off the
 * vine */
static void climber(u8 x)
{
    if (!--O(0x299A, x))
    {
        u16 t, a, b;
        O(0x299A, x) = 0x3C;
        t = (u16)(O(0x24FA, x) + 8);
        a = (u16)(((u8)(O(0x24DA, x) + 1 + (t >> 8)) << 8) | (u8)t);
        t = (u16)(P_YL + 0x10);
        b = (u16)(((u8)(P_YH + 1 + (t >> 8)) << 8) | (u8)t);
        if (a >= b)
        {
            set_script(x, 0xC3FA);
            set_vy(x, 0xFFA0);
            O(0x265A, x) = 0x0F;
        }
        else
        {
            set_script(x, 0xC401);
            set_vy(x, 0x0060);
            O(0x265A, x) = 0x12;
            O(0x267A, x) = 1;
        }
    }
    O(0x263A, x) = 0x08;                /* $C399 */
    ss2_obj_probe(x);
    if (!Z_HIT_VINE)
    {
        set_vy(x, (u16)(0 - get_vy(x)));
        O(0x299A, x) = 0x3C;
    }
    ss2_obj_common(x);
}

/* $C3BC */
static void dropper_hit(u8 x)
{
    if (O(0x245A, x) == 0x40)
    {
        u8 s = O(0x281A, x);
        if (O(0x297A, x) == 4) O(0x2B5A, s) = 0xFF;
        else if (!--O(0x2B5A, s)) O(0x2C1A, s) = 0x64;
        ss2_obj_death(x);
        return;
    }
    O(0x245A, x) = 0x80;
    ss2_obj_common(x);
}

/* ======================================================================
 * $C408-$C598: bats
 * ==================================================================== */

static void spawn_bat(u8 strong)
{
    u8 x;
    ZP(0x35) = strong;
    if (!spawn_ok()) return;
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xC4B2);            /* $C422 */
    place(x);
    set_vx(x, 0);
    set_vy(x, 0x0080);
    if (!(O(0x247A, x) & 0x80)) set_script(x, 0xC5A0);
    if (ZP(0x35))
    {
        O(0x26DA, x) = 0x10;
        O(0x26FA, x) = 0x03;
        O(0x279A, x) = 0x0A;
        O(0x255A, x) = 0x8A;
        O(0x289A, x) = 0x23;
    }
    O(0x287A, x) = 0;
    O(0x297A, x) = ZP(0x35);
    O(0x299A, x) = 0x0A;
    link_spawner(x);
    ss2_obj_common(x);
}

/* $C4C8: hang and bob until the player is near */
static void bat_wait(u8 x)
{
    if (!O(0x247A, x) && near_x(x, 0x60, 0xC0))
    {
        set_handler(x, 0xC50B);
        O(0x299A, x) = 1;
        ss2_obj_common(x);
        return;
    }
    /* $C4EC */
    if (!--O(0x299A, x))
    {
        O(0x299A, x) = 0x14;
        set_vy(x, (u16)(0 - get_vy(x)));
    }
    ss2_obj_common(x);
}

/* $C50B: every $18 frames pick a direction near the one to the player */
static void bat_fly(u8 x)
{
    u16 tab;
    u8 v;
    if (--O(0x299A, x)) { ss2_obj_common(x); return; }
    O(0x277A, x) = dir_to_player(x);
    ss2_random();
    tab = O(0x297A, x) ? 0xC5B7 : 0xC5A7;
    v = ss2_rom_byte((u16)(tab + (ZP(0x8D) & 0x0F)));
    if (v != 0xFF)
    {
        O(0x277A, x) = (u8)((v + O(0x277A, x)) & 0x0F);
        dir_velocity(x);
    }
    else
    {
        set_vx(x, 0);
        set_vy(x, 0);
    }
    /* $C555 */
    set_script(x, P_XL >= O(0x249A, x) ? 0xC599 : 0xC5A0);
    O(0x299A, x) = 0x18;
    ss2_obj_common(x);
}

/* ======================================================================
 * $C5C7-$C908: hoppers
 * ==================================================================== */

static void spawn_hopper(u8 strong)
{
    u8 x;
    ZP(0x35) = strong;
    if (!spawn_ok()) return;
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xC678);            /* $C5E1 */
    place(x);
    set_vx(x, 0);
    set_vy(x, 0xFC00);
    face(x);
    if (O(0x277A, x)) set_script(x, 0xC8F9);
    if (ZP(0x35))
    {
        O(0x26DA, x) = 0x24;
        O(0x26FA, x) = 0x03;
        O(0x279A, x) = 0x0C;
        O(0x255A, x) = 0x87;
        O(0x289A, x) = 0x23;
    }
    O(0x29BA, x) = (u8)(O(0x26DA, x) >> 1);
    O(0x297A, x) = 0;
    O(0x299A, x) = 0;
    O(0x29DA, x) = 0;
    O(0x287A, x) = 0;
    link_spawner(x);                    /* no $EE67 this frame */
}

/* $C8A4 */
static void hopper_gravity(u8 x)
{
    fall(x, 0x40);
    ss2_obj_common(x);
}

/* $C8DE */
static void hopper_burn(u8 x)
{
    O(0x245A, x) = 0x40;
    die_respawn(x);
}

/* $C68E: small hops in place until the player is within $5A px */
static void hopper_idle(u8 x)
{
    if (!O(0x299A, x))
    {
        if ((s8)O(0x25FA, x) < 0 || !probe(x, 0x10, 0x20)) { hopper_gravity(x); return; }
        if (!O(0x297A, x))
        {
            O(0x297A, x)++;
            set_vy(x, 0xFE00);
            hopper_gravity(x);
            return;
        }
        O(0x299A, x) = 0x15;
        set_vy(x, 0);
        O(0x297A, x) = 0;
    }
    /* $C6D0 */
    if (!--O(0x299A, x)) { set_vy(x, 0xFB00); ss2_obj_common(x); return; }
    if (!O(0x247A, x) && near_x(x, 0x5A, 0xB4))
    {
        set_handler(x, 0xC70C);
        O(0x299A, x) = 1;
        O(0x297A, x) = 0;
    }
    ss2_obj_common(x);
}

static void hopper_chase(u8 x);

/* $C711: airborne step of the chase */
static void hopper_air(u8 x)
{
    probe_at(x, O(0x277A, x) ? 0x00 : 0x20, 0x10);
    if (ss2_obj_probe(x)) set_vx(x, 0);
    probe_at(x, 0x10, (s8)O(0x25FA, x) < 0 ? 0x08 : 0x20);
    if (!ss2_obj_probe(x)) { hopper_gravity(x); return; }
    if (Z_HIT_HURT) { hopper_burn(x); return; }
    if (!(O(0x297A, x) | O(0x29DA, x)))
    {
        if ((s8)O(0x25FA, x) < 0) { set_vy(x, 0); hopper_gravity(x); return; }
        O(0x297A, x)++;
        set_vx(x, 0);
        set_vy(x, 0xFD00);
        hopper_gravity(x);
        return;
    }
    /* $C77D */
    O(0x299A, x) = 0x0D;
    set_vy(x, 0);
    O(0x297A, x) = 0;
    hopper_chase(x);
}

/* $C70C: chase in big hops; at half health switch to fleeing ($C810) */
static void hopper_chase_entry(u8 x)
{
    if (!O(0x299A, x)) { hopper_air(x); return; }
    hopper_chase(x);
}

/* $C78B */
static void hopper_chase(u8 x)
{
    if (O(0x29BA, x) >= O(0x26DA, x))
    {
        /* $C7DD */
        set_handler(x, 0xC810);
        set_script(x, 0xC8FE);
        set_vx(x, 0x0140);
        face(x);
        if (O(0x277A, x)) set_vx(x, 0xFEC0);
        ss2_obj_common(x);
        return;
    }
    if (--O(0x299A, x))
    {
        set_script(x, 0xC8F4);
        face(x);
        if (O(0x277A, x)) set_script(x, 0xC8F9);
        ss2_obj_common(x);
        return;
    }
    /* $C7B7 */
    set_vx(x, 0x0140);
    if (O(0x277A, x)) set_vx(x, 0xFEC0);
    set_vy(x, 0xFB00);
    hopper_air(x);
}

/* $C810: flee: run, hopping over walls */
static void hopper_flee(u8 x)
{
    if (!O(0x297A, x))
    {
        if (!probe(x, 0x10, 0x20))
        {
            /* $C891 */
            set_handler(x, 0xC70C);
            O(0x299A, x) = 0;
            O(0x29DA, x)++;
            ss2_obj_common(x);
            return;
        }
        if (Z_HIT_HURT) { hopper_burn(x); return; }
        probe_at(x, O(0x277A, x) ? 0x00 : 0x20, 0x10);
        if (!ss2_obj_probe(x)) { ss2_obj_common(x); return; }
        if (!O(0x297A, x))
        {
            O(0x297A, x)++;
            set_vy(x, 0xFD00);
            O(0x277A, x) = (u8)((O(0x277A, x) + 1) & 1);
            set_vx(x, (u16)(0 - get_vx(x)));
            hopper_gravity(x);
            return;
        }
    }
    /* $C871 */
    if ((s8)O(0x25FA, x) < 0 || !probe(x, 0x10, 0x20)) { hopper_gravity(x); return; }
    set_vy(x, 0);
    O(0x297A, x) = 0;
    ss2_obj_common(x);
}

/* $C8CF */
static void hopper_hit(u8 x)
{
    hit_recover(x);
}

/* ======================================================================
 * $C909-$CBF0: rock throwers
 * ==================================================================== */

static void thrower(u8 x);

static void spawn_thrower(u16 rec)
{
    u8 x;
    if (!spawn_ok()) return;
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_obj_init(x, rec);               /* $C92D */
    set_vy(x, 0);
    O(0x287A, x) = 1;
    O(0x29BA, x) = 1;
    O(0x29DA, x) = 1;
    O(0x29FA, x) = 0;
    O(0x299A, x) = 0;
    place(x);
    link_spawner(x);
    thrower(x);
}

/* $CB17: throw a rock from parent p */
static void throw_rock(u8 p)
{
    u8 x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xCB9E);            /* $CB1C */
    if (!O(0x299A, p)) set_vx(x, 0xFE80);
    else { O(0x263A, x) = 0; set_vx(x, 0x0180); }
    ss2_random();
    set_vy(x, (ZP(0x8D) & 0x80) ? 0xFC00 : 0xFA00);
    set_ox16(x, (u16)(OX16(p) + 8));
    set_oy16(x, (u16)(OY16(p) + 8));
    O(0x287A, x) = 0;
    O(0x263A, x) = 0x08;
    O(0x265A, x) = 0x0E;
    O(0x281A, x) = 0xBF;
    O(0x297A, x) = 5;
    ss2_obj_common(x);
}

static void thrower_move(u8 x);

/* $C9B0 */
static void thrower(u8 x)
{
    if (!--O(0x29DA, x))
    {
        O(0x29DA, x) = 0x1E;
        ss2_random();
        if ((ZP(0x8D) & 0x0F) < 0x0C) throw_rock(x);
    }
    if (--O(0x29BA, x)) { thrower_move(x); return; }
    O(0x29BA, x) = 0x18;
    {
        u8 a = (u8)(O(0x247A, x) + 1), b = (u8)(P_XH + 1);
        u8 lo = O(0x249A, x), bo = lo < P_XL;
        u16 d = (u16)((((u8)(a - b - bo)) << 8) | (u8)(lo - P_XL));
        u8 right = (u16)((a << 8) | lo) < (u16)((b << 8) | P_XL);
        u8 approach;
        ZP(0x28) = (u8)d; ZP(0x29) = (u8)(d >> 8);
        O(0x297A, x) = right;
        if ((u16)(d + 0x60) >= 0xC0) approach = 1;          /* far: $CA2F */
        else if ((u16)(d + 0x40) >= 0x80)
            approach = !!(ss2_random(), (ZP(0x8D) & 0x80));
        else approach = 0;                                  /* near: $CA36 */
        if (approach ? O(0x297A, x) : !O(0x297A, x)) set_vx(x, 0x0120);
        else set_vx(x, 0xFEE0);
    }
    /* $CA53 */
    if (O(0x297A, x) != O(0x299A, x))
    {
        O(0x299A, x) = O(0x297A, x);
        set_script(x, O(0x297A, x) ? 0xCB10 : 0xCB09);
        O(0x267A, x) = 1;
    }
    thrower_move(x);
}

/* $CA83: stop at ledges and walls */
static void thrower_move(u8 x)
{
    u8 px = (s8)O(0x25BA, x) < 0 ? 0x06 : 0x19;
    if (!probe(x, px, 0x20) || probe(x, px, 0x1F)) set_vx(x, 0);
    ss2_obj_common(x);
}

/* $CAAE: knocked back for 6 frames */
static void thrower_hit(u8 x)
{
    if (O(0x245A, x) == 0x40) { die_respawn(x); return; }
    if (!O(0x29FA, x))
    {
        O(0x29FA, x) = 6;
        set_vx(x, OX16(x) >= PX16 ? 0x0380 : 0xFC80);
    }
    else if (!--O(0x29FA, x))
    {
        O(0x245A, x) = 0x80;
        O(0x29BA, x) = 1;
    }
    thrower_move(x);
}

/* $CBC6: rock */
static void rock(u8 x)
{
    if (O(0x297A, x) && !--O(0x297A, x)) O(0x269A, x) = 0x18;
    if (ss2_obj_probe(x)) { ss2_obj_die(x); return; }
    fall(x, 0x30);
    ss2_obj_common(x);
}

/* $CBB4 */
static void rock_hit(u8 x)
{
    if (O(0x245A, x) != 0x20) { O(0x245A, x) = 0; return; }
    O(0x245A, x) = 0x80;
    rock(x);
}

/* ======================================================================
 * $CBF8-$CF3D: pouncers
 * ==================================================================== */

static void spawn_pouncer(void)
{
    u8 x;
    if (!spawn_ok()) return;
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xCC7B);            /* $CC0A */
    place(x);
    set_vx(x, 0);
    set_vy(x, 0x0100);
    face(x);
    if (O(0x277A, x)) set_script(x, 0xCF17);
    O(0x297A, x) = 1;
    O(0x287A, x) = 0;
    link_spawner(x);
    ss2_obj_common(x);
}

/* $CEFC */
static void pouncer_burn(u8 x)
{
    O(0x245A, x) = 0x40;
    die_respawn(x);
}

/* $CC91: drop to the ground */
static void pouncer_drop(u8 x)
{
    if (!probe(x, 0x08, 0x10)) { ss2_obj_common(x); return; }
    if (Z_HIT_HURT) { pouncer_burn(x); return; }
    set_vy(x, 0);
    set_handler(x, 0xCCBD);
    ss2_obj_common(x);
}

/* $CD3C */
static void pouncer_stop(u8 x)
{
    set_vx(x, 0);
    O(0x299A, x) = 0x26;
    set_handler(x, 0xCD5F);
    O(0x267A, x) = 1;
    O(0x297A, x) = 1;
    O(0x29BA, x) = 1;
    ss2_obj_common(x);
}

/* $CCBD: walk toward the player, re-aiming every $1E frames */
static void pouncer_walk(u8 x)
{
    if (!--O(0x297A, x))
    {
        set_vx(x, 0x0140);
        set_script(x, 0xCF1C);
        face(x);
        if (O(0x277A, x)) { set_vx(x, 0xFEC0); set_script(x, 0xCF23); }
        O(0x297A, x) = 0x1E;
        O(0x267A, x) = 1;
        ss2_obj_common(x);
        return;
    }
    /* $CCFF */
    if (!O(0x247A, x) && near_x(x, 0x50, 0xA0)) { pouncer_stop(x); return; }
    if (!probe(x, 0x08, 0x10)) { pouncer_stop(x); return; }
    probe_at(x, O(0x277A, x) ? 0x00 : 0x10, 0x08);
    if (ss2_obj_probe(x)) { pouncer_stop(x); return; }
    ss2_obj_common(x);
}

/* $CEAF */
static void pouncer_crouch(u8 x)
{
    O(0x267A, x) = 1;
    O(0x297A, x) = 1;
    set_vx(x, 0);
    set_vy(x, 0);
    O(0x299A, x) = 0x26;
    O(0x255A, x) = (u8)((O(0x255A, x) & 0xF0) | 5);
    O(0x29BA, x) = 0;
}

/* $CE6B */
static void pouncer_rewalk(u8 x)
{
    pouncer_crouch(x);
    set_handler(x, 0xCCBD);
    ss2_obj_common(x);
}

/* $CD5F: crouch (flashing), then leap at the player */
static void pouncer_aim(u8 x)
{
    if (!O(0x29BA, x))
    {
        if (O(0x247A, x) || !near_x(x, 0x50, 0xA0)) { pouncer_rewalk(x); return; }
    }
    /* $CD7C */
    if (!--O(0x297A, x))
    {
        set_script(x, 0xCF2A);
        face(x);
        if (O(0x277A, x)) set_script(x, 0xCF2F);
        O(0x297A, x) = 0x14;
        O(0x267A, x) = 1;
    }
    /* $CDA7 */
    if (O(0x299A, x))
    {
        u8 t = O(0x299A, x)--;
        if (t == 0x1E) O(0x255A, x) = (u8)((O(0x255A, x) & 0xF0) | 1);
        else if (t == 0x14) O(0x255A, x) = (u8)((O(0x255A, x) & 0xF0) | 6);
        ss2_obj_common(x);
        return;
    }
    /* $CDD4 */
    face(x);
    set_vy(x, 0xFB80);
    set_script(x, 0xCF34);
    set_vx(x, 0x0240);
    if (O(0x277A, x)) { set_script(x, 0xCF39); set_vx(x, 0xFDC0); }
    O(0x267A, x) = 1;
    set_handler(x, 0xCE20);
    ss2_obj_common(x);
}

/* $CE20: leap */
static void pouncer_leap(u8 x)
{
    probe_at(x, (s8)O(0x25BA, x) < 0 ? 0x00 : 0x10, 0x08);
    if (ss2_obj_probe(x))
    {
        /* $CE7A: hit a wall: drop */
        pouncer_crouch(x);
        ss2_obj_snap_y(x);
        set_vy(x, 0x0100);
        set_script(x, O(0x277A, x) ? 0xCF17 : 0xCF12);
        set_handler(x, 0xCC91);
        ss2_obj_common(x);
        return;
    }
    if ((s8)O(0x25FA, x) >= 0 && probe(x, 0x08, 0x10))
    {
        if (Z_HIT_HURT) { pouncer_burn(x); return; }
        ss2_obj_snap_y(x);              /* $CE68 */
        pouncer_rewalk(x);
        return;
    }
    fall(x, 0x40);                      /* $CE54 */
    ss2_obj_common(x);
}

/* ======================================================================
 * $CF3E-$D46B: stage 5/7 bosses
 * ==================================================================== */

static void boss_wait_cloud(u8 x);

/* $CF52 */
static void spawn_boss(u8 kind)
{
    u8 x, y;
    ZP(0x35) = kind;
    if (!Z_INLOCK1) return;
    x = ss2_obj_alloc_top();
    if (x == 0xFF) return;
    if (ZP(0x35) == 2)
    {
        ZP(0x10) = 0x1A; ZP(0x11) = 0x6E;
        ss2_unported(0xE1E2);           /* start task 2 at bank 2 $6E1A */
    }
    ss2_obj_init(x, 0xD46C);
    place(x);
    set_vx(x, 0);
    set_vy(x, 0);
    y = (u8)(ZP(0x35) << 2);
    O(0x26DA, x) = ss2_rom_byte((u16)(0xD482 + y));
    O(0x26FA, x) = ss2_rom_byte((u16)(0xD483 + y));
    O(0x255A, x) = ss2_rom_byte((u16)(0xD484 + y));
    O(0x279A, x) = ss2_rom_byte((u16)(0xD485 + y));
    O(0x287A, x) = 1;
    O(0x29BA, x) = 1;
    O(0x297A, x) = ZP(0x35);
    O(0x29DA, x) = 0;
    O(0x2ADA, x) = 0;
    O(0x2AFA, x) = 0xF0;
    O(0x2B1A, x) = 0x26;
    O(0x2B3A, x) = 0x12;
    O(0x281A, x) = ZP(0x44);
    O(0x2B5A, ZP(0x44)) = 0xFF;
    ss2_obj_common(x);
}

/* $CFE9: wait for the boss to scroll in, then lock the arena */
static void boss_enter(u8 x)
{
    if (OX16(x) >= 0xD0) { ss2_obj_common(x); return; }
    if (!(O(0x297A, x) & 1))
    {
        ZP(0x8F) = 0x06;
        ZP(0x91) = 0x0A;
        ZP(0x93) = 0xB9; ZP(0x94) = 0x57;
        if (O(0x297A, x) >= 2) { ZP(0x93) = 0xEE; ZP(0x94) = 0x57; }
        ss2_message();                  /* bank 2 $6984: boss speech */
    }
    Z_HSCROLL = 1;
    Z_INLOCK1 = 0;
    if (O(0x297A, x) == 2 && (P_FLAGSC1 & 0x08))
    {
        P_CLOUDREQ = 0x08;
        ZP(0x26) = 0xA1; ZP(0x27) = 0x4E;
        P_FLAGSC1 &= (u8)~0x08;
        ss2_hud_text(0x4EA1);           /* $BADE */
        set_handler(x, 0xD052);
        if (!P_ONCLOUD) { ss2_obj_common(x); return; }
    }
    set_handler(x, 0xD064);
    ss2_obj_common(x);
}

/* $D052 */
static void boss_wait_cloud(u8 x)
{
    if (P_ONCLOUD) set_handler(x, 0xD064);
    ss2_obj_common(x);
}

static void boss_shot(u8 p);

/* $D11F: run the current action */
static void boss_action(u8 x)
{
    switch (O(0x299A, x))               /* JMP ($D128,X) */
    {
        case 0:                         /* $D132: walk toward the player */
        case 1:                         /* $D19A: walk away */
        {
            u8 away = O(0x299A, x), toward_right = O(0x277A, x);
            if (away) toward_right = !toward_right;
            if (!O(0x29BA, x))
            {
                set_vx(x, toward_right ? 0x0100 : 0xFF00);
                set_script(x, O(0x277A, x) ? 0xD5B4 : 0xD653);
                O(0x267A, x) = 1;
                O(0x29BA, x) = 0x24;
            }
            O(0x265A, x) = 0;
            O(0x263A, x) = toward_right ? 0x20 : 0xE0;
            if (ss2_obj_probe(x))
            {
                set_vx(x, 0);
                set_vy(x, 0);
                O(0x29BA, x) = 1;
            }
            ss2_obj_common(x);
            return;
        }
        case 2:                         /* $D202: stand */
            if (!O(0x29BA, x))
            {
                set_script(x, O(0x277A, x) ? 0xD50A : 0xD55F);
                O(0x267A, x) = 1;
                set_vx(x, 0);
                set_vy(x, 0);
                O(0x29BA, x) = 0x3C;
            }
            ss2_obj_common(x);
            return;
        case 3:                         /* $D23A: shoot high */
        case 4:                         /* $D240: shoot low */
            ZP(0x35) = O(0x299A, x) == 3 ? 0xE8 : 0x04;
            if (O(0x29BA, x)) { ss2_obj_common(x); return; }
            if (O(0x29DA, x) >= 2) { O(0x29BA, x) = 1; ss2_obj_common(x); return; }
            {
                u8 a = ZP(0x35);
                boss_shot(x);
                ZP(0x35) = a;
            }
            O(0x29DA, x)++;
            if (O(0x277A, x))
                set_script(x, (ZP(0x35) & 0x80) ? 0xD6F2 : 0xD7A4);
            else
                set_script(x, (ZP(0x35) & 0x80) ? 0xD74B : 0xD7FD);
            set_vx(x, 0);
            O(0x267A, x) = 1;
            O(0x29BA, x) = 0x14;
            ss2_obj_common(x);
            return;
    }
    ss2_unported(0xD125);               /* action index out of the table */
}

/* $D064: pick a weighted random action every time the timer runs out */
static void boss(u8 x)
{
    if (--O(0x29BA, x)) { boss_action(x); return; }
    {
        u16 tab = O(0x297A, x) >= 2 ? 0xD4F6 : 0xD4E2;
        u16 a, b, d;
        u8 y, n, v;
        ZP(0x2E) = (u8)tab; ZP(0x2F) = (u8)(tab >> 8);
        a = (u16)(((u8)(O(0x247A, x) + 1) << 8) | O(0x249A, x));
        {
            u16 t = (u16)(P_XL + 0x10);
            b = (u16)(((u8)(P_XH + 1 + (t >> 8)) << 8) | (u8)t);
        }
        d = (u16)(a - b);
        O(0x277A, x) = 0;
        if (a < b)
        {
            O(0x277A, x)++;
            d = (u16)(0 - d);
        }
        ZP(0x26) = (u8)d; ZP(0x27) = (u8)(d >> 8);
        y = 0x0F;
        if (!(d >> 8))
        {
            u8 r = 0, lo = (u8)d;
            if (O(0x297A, x) >= 2)
            {
                if (lo >= 0x50) { r++; if (lo >= 0x80) r++; }
            }
            else
            {
                if (lo >= 0x50) { r++; if (lo >= 0x90) r++; }
            }
            y = (u8)(r * 5);
        }
        /* $D0F7: subtract weights until the roll goes negative */
        n = 0;
        v = (u8)(ZP(0x8D) & 0x0F);
        {
            u8 i;
            for (i = 0; i < 4; i++)
            {
                u8 w = ss2_rom_byte((u16)(tab + y));
                if (v < w) break;
                v = (u8)(v - w);
                y++;
                n++;
            }
        }
        ZP(0x30) = n;
        O(0x299A, x) = n;
    }
    boss_action(x);
}

/* $D2B3: fire a shot from boss p at height offset ZP($35) */
static void boss_shot(u8 p)
{
    u8 x = ss2_obj_alloc(), y;
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xD492);            /* $D2B8 */
    O(0x247A, x) = O(0x247A, p);
    O(0x249A, x) = O(0x249A, p);
    {
        u16 t = (u16)(ZP(0x35) + O(0x24FA, p));
        O(0x24FA, x) = (u8)t;
        O(0x24DA, x) = O(0x24DA, p);
        (void)t;                        /* the carry is dropped ($D2DA) */
    }
    set_vy(x, 0);
    set_vx(x, 0x0200);
    O(0x297A, x) = 0;
    if (!O(0x277A, p))
    {
        set_vx(x, 0xFE00);
        O(0x297A, x)++;
    }
    y = (u8)(O(0x297A, p) << 1);
    O(0x26DA, x) = ss2_rom_byte((u16)(0xD4A8 + y));
    O(0x26FA, x) = ss2_rom_byte((u16)(0xD4A9 + y));
    O(0x287A, x) = 1;
    O(0x281A, x) = 0xBF;
    O(0x29DA, x) = p;
    O(0x299A, x) = 0;
    O(0x29BA, x) = 0x30;
    if (!(O(0x297A, p) & 1))
    {
        if ((ZP(0x8D) & 7) >= 2) O(0x29BA, x) = 0x50;
    }
    else
    {
        if ((ZP(0x8D) & 7) >= 4) O(0x29BA, x) = 0x50;
    }
    ss2_obj_common(x);
}

/* $D359: boss struck */
static void boss_hit(u8 x)
{
    u8 y;
    if (O(0x245A, x) != 0x40) { O(0x245A, x) = 0x80; ss2_obj_common(x); return; }
    y = (u8)(O(0x297A, x) << 1);
    ZP(0x26) = ss2_rom_byte((u16)(0xD4B0 + y));
    ZP(0x27) = ss2_rom_byte((u16)(0xD4B1 + y));
    ss2_unported(0x6F40);               /* bank 2: boss defeat sequence */
}

/* $D37A: boomerang shot: slows down, returns to the boss */
static void boss_boomerang(u8 x)
{
    if (!O(0x299A, x))
    {
        if (O(0x29BA, x)) { O(0x29BA, x)--; ss2_obj_common(x); return; }
        if (!(O(0x25DA, x) | O(0x25BA, x))) { O(0x299A, x) = 1; ss2_obj_common(x); return; }
        set_vx(x, (u16)(get_vx(x) + (O(0x297A, x) ? 0x0020 : 0xFFE0)));
        ss2_obj_common(x);
        return;
    }
    /* $D3C7: fly back */
    if (!O(0x297A, x))
    {
        set_vx(x, (u16)(get_vx(x) + 0xFFE0));
        if (O(0x25BA, x) == 0xFD) set_vx(x, 0xFE00);
    }
    else
    {
        set_vx(x, (u16)(get_vx(x) + 0x0020));
        if (O(0x25BA, x) == 0x02) set_vx(x, 0x0200);
    }
    /* $D40C: caught by the boss */
    {
        u8 p = O(0x29DA, x);
        u16 t = (u16)(O(0x249A, x) + 0x10);
        u16 a = (u16)(((u8)(O(0x247A, x) + 1 + (t >> 8)) << 8) | (u8)t);
        u16 b = (u16)(((u8)(O(0x247A, p) + 1) << 8) | O(0x249A, p));
        u16 d = (u16)(a - b);
        ZP(0x2A) = (u8)d; ZP(0x2B) = (u8)(d >> 8);
        if ((d >> 8) || (u8)d >= 0x20) { ss2_obj_common(x); return; }
        O(0x245A, x) = 0;
        O(0x29DA, p)--;
    }
}

/* $D452 */
static void boss_boomerang_hit(u8 x)
{
    if (O(0x245A, x) != 0x40) { O(0x245A, x) = 0x80; ss2_obj_common(x); return; }
    O(0x269A, x) &= 0xDF;
    ss2_obj_death(x);
}

/* ======================================================================
 * $D8A1-$DCA9: stage 6/7 boss
 * ==================================================================== */

static void spawn_boss2(u8 kind)
{
    u8 x;
    ZP(0x35) = kind;
    if (!Z_INLOCK1) return;
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    if (!ZP(0x35))
    {
        ZP(0x10) = 0x1A; ZP(0x11) = 0x6E;
        ss2_unported(0xE1E2);           /* start task 2 at bank 2 $6E1A */
    }
    ss2_obj_init(x, 0xDCAA);            /* $D8C3 */
    place(x);
    set_vx(x, 0);
    set_vy(x, 0);
    if (ZP(0x35))
    {
        O(0x26DA, x) = 0x8C;
        O(0x255A, x) = 0x8D;
        O(0x279A, x) = 0x1A;
    }
    O(0x287A, x) = 1;
    O(0x29DA, x) = 1;
    O(0x297A, x) = ZP(0x35);
    O(0x2ADA, x) = 0;
    O(0x2AFA, x) = 0;
    O(0x2B1A, x) = 0x10;
    O(0x2B3A, x) = 0x10;
    O(0x281A, x) = ZP(0x44);
    O(0x2B5A, ZP(0x44)) = 0xFF;
    ss2_obj_common(x);
}

/* $D92F */
static void boss2_enter(u8 x)
{
    if (OX16(x) >= 0xD0) { ss2_obj_common(x); return; }
    if (!O(0x297A, x))
    {
        ZP(0x8F) = 0x06;
        ZP(0x91) = 0x0A;
        ZP(0x93) = 0x2F; ZP(0x94) = 0x58;
        ss2_message();                  /* bank 2 $6984: boss speech */
    }
    Z_HSCROLL = 1;
    Z_INLOCK1 = 0;
    set_handler(x, 0xD967);
    ss2_obj_common(x);
}

static void boss2_spawn_shot(u8 p);
static void boss2_action(u8 x);

/* $D967 */
static void boss2(u8 x)
{
    if (!--O(0x29DA, x))
    {
        if (O(0x2A1A, x) || O(0x247A, x))
        {
            O(0x299A, x) = 6;
            O(0x29DA, x) = 0;
        }
        else
        {
            u16 t, a, b;
            u8 r;
            O(0x29FA, x) = 0;
            a = (u16)(((u8)(O(0x247A, x) + 1) << 8) | O(0x249A, x));
            t = (u16)(P_XL + 0x10);
            b = (u16)(((u8)(P_XH + 1 + (t >> 8)) << 8) | (u8)t);
            O(0x277A, x) = a < b;
            ss2_random();
            r = 0;
            if ((ZP(0x8D) & 0x0F) >= 4) { r++; if ((ZP(0x8D) & 0x0F) >= 0x0B) r++; }
            ZP(0x30) = r;
            O(0x299A, x) = (u8)(r << 1);
        }
    }
    boss2_action(x);
}

/* $D9CD: JMP ($D9D5,X) */
static void boss2_action(u8 x)
{
    switch (O(0x299A, x))
    {
        case 0:                         /* $D9DD: stand */
            if (!O(0x29DA, x))
            {
                O(0x29DA, x) = 0x28;
                O(0x267A, x) = 1;
                O(0x271A, x) = 0x0A;
                O(0x273A, x) = 0x28;
                set_script(x, O(0x277A, x) ? 0xDD20 : 0xDD45);
            }
            ss2_obj_common(x);
            return;
        case 2:                         /* $DA13: throw */
        case 4:
            if (!O(0x29DA, x))
            {
                O(0x29DA, x) = 0x36;
                O(0x267A, x) = 1;
                O(0x271A, x) = 0x0A;
                O(0x273A, x) = 0x28;
                O(0x29FA, x)++;
                set_script(x, O(0x277A, x) ? 0xDD6A : 0xDDE7);
            }
            else if (O(0x29DA, x) == 0x1E)
            {
                O(0x271A, x) = 0x20;
                O(0x273A, x) = 0x0A;
                boss2_spawn_shot(x);
            }
            ss2_obj_common(x);
            return;
        case 6:                         /* $DB0C: sink and reappear */
            if (!O(0x29DA, x))
            {
                O(0x29DA, x) = 0x5A;
                O(0x267A, x) = 1;
                O(0x269A, x) = 0x27;
                O(0x29BA, x) = 0;
                O(0x271A, x) = 0x0A;
                O(0x273A, x) = 0x28;
                O(0x2A1A, x) = 0;
                set_script(x, O(0x277A, x) ? 0xDE64 : 0xDE8F);
            }
            else if (!O(0x29BA, x))
            {
                if (O(0x29DA, x) == 0x1E)
                {
                    /* $DB58: pick a free spot on the floor */
                    for (;;)
                    {
                        O(0x29BA, x) = 1;
                        ss2_random();
                        O(0x249A, x) = ss2_rom_byte((u16)(0xDB9D + (ZP(0x8D) & 7)));
                        O(0x247A, x) = 0;
                        if (probe(x, 0xF0, 0x00)) continue;
                        if (probe(x, 0x10, 0x00)) continue;
                        break;
                    }
                }
            }
            else if ((u8)(O(0x29DA, x) - 1) == 0)
            {
                O(0x29BA, x) = 0;
                O(0x29DA, x) = 1;
                O(0x269A, x) = 0x20;
            }
            ss2_obj_common(x);
            return;
    }
    ss2_unported(0xD9D2);               /* odd action index */
}

/* $DA63: shot from boss p: arc (actions 0/4) or along the floor (2) */
static void boss2_spawn_shot(u8 p)
{
    u8 x = ss2_obj_alloc();
    u16 rec = O(0x299A, p) == 2 ? 0xDCC0 : 0xDCD6, t, o;
    if (x == 0xFF) return;
    ss2_obj_init(x, rec);
    o = O(0x277A, p) ? 0x0020 : 0xFFE0;
    ZP(0x30) = (u8)o; ZP(0x31) = (u8)(o >> 8);
    t = (u16)(OX16(p) + o);
    set_ox16(x, t);
    O(0x24DA, x) = O(0x24DA, p);
    O(0x24FA, x) = O(0x24FA, p);
    if (O(0x299A, p) != 2)
    {
        set_vy(x, 0xFA80);
        set_vx(x, O(0x277A, p) ? 0x0100 : 0xFF00);
    }
    else
    {
        ss2_sfx(0x27);
        set_vy(x, 0);
        set_vx(x, O(0x277A, p) ? 0x0300 : 0xFD00);
    }
    O(0x287A, x) = 0;
    ss2_obj_common(x);
}

/* $DBA5: boss struck */
static void boss2_hit(u8 x)
{
    u8 y;
    if (O(0x245A, x) != 0x40)
    {
        O(0x245A, x) = 0x80;
        if (O(0x29FA, x))
        {
            O(0x2A1A, x) = O(0x29FA, x);
            boss2(x);
            return;
        }
        O(0x299A, x) = 6;
        O(0x29DA, x) = 0;
        boss2_action(x);
        return;
    }
    y = (u8)(O(0x297A, x) << 1);
    ZP(0x26) = ss2_rom_byte((u16)(0xDD02 + y));
    ZP(0x27) = ss2_rom_byte((u16)(0xDD03 + y));
    ss2_unported(0x6F40);               /* bank 2: boss defeat sequence */
}

/* $DBD9: shot rolling along the floor: gone at a wall */
static void boss2_roller(u8 x)
{
    if (!probe(x, 0x00, 0x00)) { ss2_obj_common(x); return; }
    ZP(0x86) = 0;
    O(0x245A, x) = 0;
}

/* $DC12: burst into 8 fragments at the object's position */
static void burst(u8 p)
{
    ZP(0x40) = O(0x247A, p); ZP(0x41) = O(0x249A, p);
    ZP(0x42) = O(0x24DA, p); ZP(0x43) = O(0x24FA, p);
    for (ZP(0x35) = 0; ; )
    {
        u8 x = ss2_obj_alloc_top(), y;
        if (x == 0xFF) return;
        ss2_obj_init(x, 0xDCEC);        /* $DC2C */
        y = (u8)(ZP(0x35) << 1);
        O(0x25DA, x) = ss2_rom_byte((u16)(0xDC7B + y));
        O(0x25BA, x) = ss2_rom_byte((u16)(0xDC7C + y));
        set_vy(x, 0);
        place(x);
        O(0x287A, x) = 0;
        O(0x281A, x) = 0xBF;
        {
            u8 a = ZP(0x35);
            ss2_obj_common(x);
            ZP(0x35) = a;
        }
        if (++ZP(0x35) == 8) return;
    }
}

/* $DBEF: arcing shot: bursts at the top of its arc */
static void boss2_arc(u8 x)
{
    fall(x, 0x20);
    if (O(0x25FA, x)) { ss2_obj_common(x); return; }
    burst(x);
    O(0x245A, x) = 0;
}

/* $DC8B: fragment: falls until it leaves the screen top band */
static void fragment(u8 x)
{
    if (O(0x24DA, x)) { O(0x245A, x) = 0; return; }
    fall(x, 0x10);
    ss2_obj_common(x);
}

/* ---- dispatch ---------------------------------------------------------- */

u8 ss2_obj05_handler(u16 addr, u8 x)
{
    switch (addr)
    {
        case 0xC164: dropper_wait(x); return 1;
        case 0xC1C0: dropper_fall(x); return 1;
        case 0xC222: dropper_split(x); return 1;
        case 0xC294: dropper_walk(x); return 1;
        case 0xC32D: climber(x); return 1;
        case 0xC3BC: dropper_hit(x); return 1;
        case 0xC4C8: bat_wait(x); return 1;
        case 0xC50B: bat_fly(x); return 1;
        case 0xC579: hit_recover(x); return 1;
        case 0xC68E: hopper_idle(x); return 1;
        case 0xC70C: hopper_chase_entry(x); return 1;
        case 0xC810: hopper_flee(x); return 1;
        case 0xC8CF: hopper_hit(x); return 1;
        case 0xC9B0: thrower(x); return 1;
        case 0xCAAE: thrower_hit(x); return 1;
        case 0xCBB4: rock_hit(x); return 1;
        case 0xCBC6: rock(x); return 1;
        case 0xCC91: pouncer_drop(x); return 1;
        case 0xCCBD: pouncer_walk(x); return 1;
        case 0xCD5F: pouncer_aim(x); return 1;
        case 0xCE20: pouncer_leap(x); return 1;
        case 0xCEED: hit_recover(x); return 1;
        case 0xCFE9: boss_enter(x); return 1;
        case 0xD052: boss_wait_cloud(x); return 1;
        case 0xD064: boss(x); return 1;
        case 0xD359: boss_hit(x); return 1;
        case 0xD37A: boss_boomerang(x); return 1;
        case 0xD452: boss_boomerang_hit(x); return 1;
        case 0xD92F: boss2_enter(x); return 1;
        case 0xD967: boss2(x); return 1;
        case 0xDBA5: boss2_hit(x); return 1;
        case 0xDBD9: boss2_roller(x); return 1;
        case 0xDBEF: boss2_arc(x); return 1;
        case 0xDC8B: fragment(x); return 1;
    }
    return 0;
}

u8 ss2_spawn05_handler(u16 addr)
{
    switch (addr)
    {
        case 0xC000: spawn_dropper(0xC10C, 0); return 1;
        case 0xC00E: spawn_dropper(0xC122, 1); return 1;
        case 0xC01C: spawn_dropper(0xC138, 2); return 1;
        case 0xC02A: spawn_dropper(0xC10C, 4); return 1;
        case 0xC408: spawn_bat(0); return 1;
        case 0xC40C: spawn_bat(1); return 1;
        case 0xC5C7: spawn_hopper(0); return 1;
        case 0xC5CB: spawn_hopper(1); return 1;
        case 0xC909: spawn_thrower(0xC984); return 1;
        case 0xC913: spawn_thrower(0xC99A); return 1;
        case 0xCBF8: spawn_pouncer(); return 1;
        case 0xCF3E: spawn_boss(0); return 1;
        case 0xCF42: spawn_boss(1); return 1;
        case 0xCF48: spawn_boss(2); return 1;
        case 0xCF4E: spawn_boss(3); return 1;
        case 0xD8A1: spawn_boss2(0); return 1;
        case 0xD8A5: spawn_boss2(1); return 1;
    }
    return 0;
}

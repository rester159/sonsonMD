/* SON SON II objects in HuCard bank $07 at $C000: the flying boss of
 * stages 2 and 7, rebuilt from bank 7 $D800-$DD64.
 *
 * Two slots: x (head, record $DD65 / $DD7B) and x+1 (body, record $DD91,
 * dragged $18 px below the head by the head's handler). Shots: three at
 * a time (record $DDA7) fanned around the aim at the player.
 * Fields: $297A:$299A swoop speed, $29BA:$29DA / $29FA:$2A1A velocity
 * saved over a knockback, $2A3A move (0 choose, 1 rise and hover, 2/3
 * swoop), $2A5A hover count, $2A7A knockback timer, $2A9A started,
 * $2ABA facing (1 = right).
 */
#include "ss2_ram.h"
#include "ss2_logic.h"

#define OX16(x)     ((u16)((O(0x247A, x) << 8) | O(0x249A, x)))
#define OY16(x)     ((u16)((O(0x24DA, x) << 8) | O(0x24FA, x)))

/* bank $17 file: bank 3 $85E7, bank 2 $7BEA, bank 2 $6F40 */
u8   ss2_o17_aim(u8 x);
void ss2_o17_dir_velocity(u8 x);
void ss2_o17_boss_death(u8 x, u16 rle);

static void set_vx(u8 x, u16 v) { O(0x25BA, x) = (u8)(v >> 8); O(0x25DA, x) = (u8)v; }
static void set_vy(u8 x, u16 v) { O(0x25FA, x) = (u8)(v >> 8); O(0x261A, x) = (u8)v; }
static u16 vx16(u8 x) { return (u16)((O(0x25BA, x) << 8) | O(0x25DA, x)); }
static u16 vy16(u8 x) { return (u16)((O(0x25FA, x) << 8) | O(0x261A, x)); }
static void set_script(u8 x, u16 p) { O(0x295A, x) = (u8)p; O(0x293A, x) = (u8)(p >> 8); }
static void set_handler(u8 x, u16 h) { O(0x28DA, x) = (u8)h; O(0x28BA, x) = (u8)(h >> 8); }
static u16 swoop(u8 x) { return (u16)((O(0x297A, x) << 8) | O(0x299A, x)); }
static void set_swoop(u8 x, u16 v) { O(0x297A, x) = (u8)(v >> 8); O(0x299A, x) = (u8)v; }

/* $DD3D: carry = head x + $100 >= player x + $110 (player to the left) */
static u8 player_left(u8 x)
{
    return (u16)(OX16(x) + 0x100) >= (u16)(((P_XH << 8) | P_XL) + 0x110);
}

/* $D819 */
static void spawn_boss(u16 rec, u8 v)
{
    u8 x, s = ZP(0x44);
    u16 t;
    ZP(0x35) = v;
    if (!ZP(0x7F)) return;
    for (x = 0; O(0x245A, x) | O(0x245A, x + 1); )
        if (++x == 0x1F) return;
    ss2_obj_init(x, rec);
    ss2_obj_init((u8)(x + 1), 0xDD91);
    if (!v)
    {
        /* $E1E2: task 3 becomes bank 2 $6E1A (stage clear once the object
         * table is empty) */
        ZP(0x10) = 0x1A; ZP(0x11) = 0x6E;
        ZP(0x08) = 0x08; ZP(0x0A) = 0x1A; ZP(0x0B) = 0x6E;
    }
    O(0x249A, x) = O(0x249A, x + 1) = ZP(0x41);
    O(0x247A, x) = O(0x247A, x + 1) = ZP(0x40);
    O(0x24FA, x) = ZP(0x43);
    t = (u16)(ZP(0x43) + 0x18);
    O(0x24FB, x) = (u8)t;
    O(0x24DA, x) = ZP(0x42);
    O(0x24DB, x) = (u8)(ZP(0x42) + (t >> 8));
    O(0x25DA, x) = 0; O(0x25DB, x) = 0;
    O(0x25BA, x) = 0; O(0x25BB, x) = 0;
    O(0x261A, x) = 0; O(0x261B, x) = 0;
    O(0x25FA, x) = 0; O(0x25FB, x) = 0;
    O(0x287A, x) = 1; O(0x287B, x) = 1;
    O(0x281A, x) = s;
    O(0x281B, x) = 0xBF;
    O(0x26FB, x) = O(0x26FA, x);
    O(0x2A3A, x) = 0;
    O(0x2A9A, x) = 0;
    O(0x2A7A, x) = 0;
    O(0x2B5A, s) = 0xFF;
    ss2_obj_common(x);
    ss2_obj_common((u8)(x + 1));
}

/* $DB6E: stop at walls, run both slots, keep the body under the head */
static void move(u8 x)
{
    u16 t;
    O(0x263A, x) = (O(0x25BA, x) & 0x80) ? 0xF0 : 0x0F;
    O(0x265A, x) = 0;
    if (ss2_obj_probe(x)) set_vx(x, 0);
    ss2_obj_common(x);
    O(0x245B, x) = O(0x245A, x);
    O(0x283B, x) = O(0x283A, x);
    O(0x26DB, x) = 0xFA;
    O(0x285B, x) = 0;
    O(0x249B, x) = O(0x249A, x);
    O(0x247B, x) = O(0x247A, x);
    t = (u16)(O(0x24FA, x) + 0x18);
    O(0x24FB, x) = (u8)t;
    O(0x24DB, x) = (u8)(O(0x24DA, x) + (t >> 8));
    ss2_obj_common((u8)(x + 1));
}

/* $DB66 */
static void restart_scripts(u8 x)
{
    O(0x267A, x) = 1;
    O(0x267B, x) = 1;
    move(x);
}

/* $DCAA: three shots (record $DDA7) from ($40-$43) in the first free run
 * of three slots at or after x */
static void shots(u8 x)
{
    u8 i, d;
    for (; O(0x245A, x) | O(0x245A, x + 1) | O(0x245A, x + 2); )
        if (++x == 0x1E) return;
    for (i = 0; i < 3; i++) ss2_obj_init((u8)(x + i), 0xDDA7);
    for (i = 0; i < 3; i++)
    {
        O(0x247A, x + i) = ZP(0x40);
        O(0x249A, x + i) = ZP(0x41);
        O(0x24DA, x + i) = ZP(0x42);
        O(0x24FA, x + i) = ZP(0x43);
    }
    for (i = 0; i < 3; i++) O(0x281A, x + i) = 0xBF;
    for (i = 0; i < 3; i++) O(0x287A, x + i) = 0;
    d = ss2_o17_aim(x);
    O(0x277A, x) = d;
    O(0x277B, x) = (u8)((d - 1) & 0x0F);
    O(0x277C, x) = (u8)((d + 1) & 0x0F);
    for (i = 0; i < 3; i++)
    {
        ss2_o17_dir_velocity((u8)(x + i));
        ss2_obj_common((u8)(x + i));
    }
}

/* $DABF: face the player and fire */
static void fire(u8 x)
{
    u16 t;
    if (player_left(x))
    {
        O(0x2ABA, x) = 0;
        set_script(x, 0xDDF3);
        set_script((u8)(x + 1), 0xDE41);
        t = (u16)(OX16(x) + 0xFFF8);
    }
    else
    {
        O(0x2ABA, x) = 1;
        set_script(x, 0xDE1A);
        set_script((u8)(x + 1), 0xDE58);
        t = (u16)(OX16(x) + 0x0008);
    }
    ZP(0x41) = (u8)t;
    ZP(0x40) = (u8)(t >> 8);
    ZP(0x43) = O(0x24FA, x);
    ZP(0x42) = O(0x24DA, x);
    shots(x);
    restart_scripts(x);
}

/* $DB25: turn toward the player */
static void face(u8 x)
{
    if (player_left(x))
    {
        if (!O(0x2ABA, x)) { move(x); return; }
        O(0x2ABA, x) = 0;
        set_script(x, 0xDE05);
        set_script((u8)(x + 1), 0xDE41);
    }
    else
    {
        if (O(0x2ABA, x)) { move(x); return; }
        O(0x2ABA, x) = 1;
        set_script(x, 0xDE2C);
        set_script((u8)(x + 1), 0xDE58);
    }
    restart_scripts(x);
}

/* $D993: rise to y $58 and bob there; fires now and then */
static void hover(u8 x)
{
    set_vy(x, (u16)(vy16(x) + (O(0x24FA, x) >= 0x58 ? 0xFFE0 : 0x0010)));
    if (!O(0x2A9A, x) || O(0x24FA, x) < 0x58 || vy16(x)) { face(x); return; }
    if (!--O(0x2A5A, x)) O(0x2A3A, x) = 0;
    ss2_random();
    if ((ZP(0x8D) & 0x0F) < (P_ONCLOUD ? 0x0C : 0x04)) { fire(x); return; }
    face(x);
}

/* $DA7F: swoop: accelerate sideways, decelerate upward */
static void swoop_step(u8 x)
{
    set_vx(x, (u16)(vx16(x) + swoop(x)));
    set_vy(x, (u16)(vy16(x) + 0xFFE8));
    if (!P_ONCLOUD || vy16(x)) { face(x); return; }
    ss2_random();
    if (ZP(0x8D) & 1) { face(x); return; }
    fire(x);
}

/* $DA00 / $DA1D: start a swoop (narrow / wide) */
static void swoop_start(u8 x, u16 sp)
{
    set_swoop(x, player_left(x) ? (u16)(0 - sp) : sp);
    set_vx(x, 0);
    set_vy(x, 0x0378);
    swoop_step(x);
}

/* $DA4A: swooping */
static void swooping(u8 x)
{
    if ((O(0x25FA, x) & 0x80) && O(0x24FA, x) < 0x58)
    {
        O(0x2A3A, x) = 0;
        set_vx(x, 0);
        set_vy(x, 0);
        face(x);
        return;
    }
    if (!vy16(x)) set_swoop(x, (u16)(0 - swoop(x)));
    swoop_step(x);
}

/* $D92E */
static void boss(u8 x)
{
    u8 m = O(0x2A3A, x);
    if (m)
    {
        if (m == 1) hover(x); else swooping(x);
        return;
    }
    if (O(0x247A, x)) { O(0x2A3A, x) = 3; swoop_start(x, 0x0010); return; }
    /* $D94A */
    ss2_random();
    m = ss2_bank_byte(7, (u16)(0xDDBD + (u8)((ZP(0x8D) & 0x0F) | (P_ONCLOUD ? 0x10 : 0))));
    O(0x2A3A, x) = m;
    if (m == 1)
    {
        /* $D976 */
        O(0x24FA, x) = 0x58;
        O(0x251A, x) = 0;
        set_vx(x, 0);
        set_vy(x, 0xFF80);
        O(0x2A5A, x) = 4;
        hover(x);
        return;
    }
    swoop_start(x, m == 2 ? 0x000C : 0x0010);
}

/* $D91B: the fight starts */
static void boss_start(u8 x)
{
    Z_INLOCK1 = 0;
    Z_HSCROLL = 1;
    O(0x2A9A, x)++;
    set_handler(x, 0xD92E);
    boss(x);
}

/* $D903: wait until the player rides the cloud */
static void wait_cloud(u8 x)
{
    if (P_ONCLOUD) { boss_start(x); return; }
    move(x);
}

/* $D8BC: stage 2: fly in to x < $D0, talk, hand over the cloud */
static void enter2(u8 x)
{
    if (OX16(x) >= 0xD0) { move(x); return; }
    ZP(0x8F) = 0x06; ZP(0x91) = 0x0A; ZP(0x93) = 0x0F; ZP(0x94) = 0x57;
    /* bank 2 $6984: message window, several frames inside the object task */
    ss2_unported(0x6984);
    if (!(P_FLAGSC1 & 0x08)) { boss_start(x); return; }
    P_FLAGSC1 &= (u8)~0x08;
    P_CLOUDREQ = 0x08;
    /* bank 4 $BADE: print text $4EA1 into the BAT (presentation) */
    ZP(0x8B) = 0x02;
    set_handler(x, 0xD903);
    wait_cloud(x);
}

/* $D90B: stage 7 */
static void enter7(u8 x)
{
    if (OX16(x) >= 0xD0) { move(x); return; }
    boss_start(x);
}

/* $DBE6: knockback for $0C frames, then resume (mirrored when swooping
 * past the player) and fire */
static void knocked(u8 x)
{
    if (!O(0x2A7A, x))
    {
        O(0x2A7A, x) = 0x0C;
        O(0x29BA, x) = O(0x25BA, x);
        O(0x29DA, x) = O(0x25DA, x);
        O(0x29FA, x) = O(0x25FA, x);
        O(0x2A1A, x) = O(0x261A, x);
        set_vy(x, 0);
        set_vx(x, player_left(x) ? 0x0280 : 0xFD80);
        move(x);
        return;
    }
    if (--O(0x2A7A, x)) { move(x); return; }
    {
        u8 flip = 0;
        if (O(0x2A3A, x) >= 2)
            flip = player_left(x) ? (O(0x29BA, x) & 0x80) : !(O(0x29BA, x) & 0x80);
        if (!flip)
        {
            O(0x25DA, x) = O(0x29DA, x);
            O(0x25BA, x) = O(0x29BA, x);
        }
        else
        {
            /* $DC5B: CLC; CLA; SBC: one's complement */
            set_swoop(x, (u16)~swoop(x));
            set_vx(x, (u16)~(u16)((O(0x29BA, x) << 8) | O(0x29DA, x)));
        }
        if (O(0x2A3A, x) >= 2 && !(O(0x29FA, x) & 0x80))
            set_vy(x, (u16)~(u16)((O(0x29FA, x) << 8) | O(0x2A1A, x)));
        else
        {
            O(0x261A, x) = O(0x2A1A, x);
            O(0x25FA, x) = O(0x29FA, x);
        }
    }
    O(0x245A, x) = 0x80;
    fire(x);
}

/* $DBC2 / $DBD4 */
static void boss_hit(u8 x, u16 rle)
{
    if (O(0x245A, x) == 0x40) { ss2_o17_boss_death(x, rle); return; }
    knocked(x);
}

u8 ss2_obj07_handler(u16 addr, u8 x)
{
    switch (addr)
    {
        case 0xD8BC: enter2(x); return 1;
        case 0xD903: wait_cloud(x); return 1;
        case 0xD90B: enter7(x); return 1;
        case 0xD92E: boss(x); return 1;
        case 0xDBC2: boss_hit(x, 0xDDDD); return 1;
        case 0xDBD4: boss_hit(x, 0xDDE7); return 1;
        case 0xDD37: O(0x245A, x) = 0; return 1;    /* shot hit */
    }
    return 0;
}

u8 ss2_spawn07_handler(u16 addr)
{
    switch (addr)
    {
        case 0xD800: spawn_boss(0xDD65, 0); return 1;
        case 0xD80D: spawn_boss(0xDD7B, 1); return 1;
    }
    return 0;
}

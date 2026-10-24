/* SON SON II bank $0F objects, rebuilt from HuCard bank 15 $DDF6-$DFBA.
 *
 *   $DDF6/$DDFA/$DE00/$DE06  shuttle: moves up/down/right/left ($35 = 0-3)
 *          for $30 frames, pauses $5A frames, reverses, then shuttles $60
 *          frames each way; $96 hits destroy it ($DEB0/$DF36)
 *
 * Extra per-object fields used here: $297A move timer, $299A pause timer,
 * $29BA hit count, $29FA:$29DA (lo:hi) saved vx, $2A3A:$2A1A saved vy.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"

static void set_vx(u8 x, u16 v) { O(0x25BA, x) = (u8)(v >> 8); O(0x25DA, x) = (u8)v; }
static void set_vy(u8 x, u16 v) { O(0x25FA, x) = (u8)(v >> 8); O(0x261A, x) = (u8)v; }

/* the $2CDA per-spawner palette nibble shared by the spawns */
static void spawner_nibble(u8 s, u8 x)
{
    if (!O(0x2CDA, s)) O(0x2CDA, s) = (u8)(O(0x289A, x) & 0x0F);
    else O(0x289A, x) = (u8)((O(0x289A, x) & 0xF0) | O(0x2CDA, s));
}

/* ---- $DDF6 shuttle ---------------------------------------------------- */

static void shuttle_spawn(u8 dir)
{
    u8 s = ZP(0x44), x;
    u16 t;
    ZP(0x35) = dir;
    if (O(0x2B5A, s) | O(0x2C1A, s)) return;
    if (ss2_spawn_visible()) return;
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xDE9A);
    O(0x247A, x) = ZP(0x40);
    O(0x249A, x) = ZP(0x41);
    O(0x24DA, x) = ZP(0x42);
    O(0x24FA, x) = ZP(0x43);
    t = (u16)(0xDFAB + (u8)(dir << 2));      /* (vx lo, vx hi, vy lo, vy hi) */
    O(0x25DA, x) = ss2_rom_byte(t);
    O(0x25BA, x) = ss2_rom_byte((u16)(t + 1));
    O(0x261A, x) = ss2_rom_byte((u16)(t + 2));
    O(0x25FA, x) = ss2_rom_byte((u16)(t + 3));
    O(0x2ADA, x) = 0;
    O(0x2AFA, x) = 0;
    O(0x2B1A, x) = 0;
    O(0x2B3A, x) = 0;
    O(0x287A, x) = 0;
    O(0x297A, x) = 0x30;
    O(0x299A, x) = 0;
    O(0x29BA, x) = 0;
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    spawner_nibble(s, x);
    ss2_obj_common(x);
}

/* $DEE7: stop for $5A frames, keeping the velocity */
static void shuttle_pause(u8 x)
{
    O(0x299A, x) = 0x5A;
    O(0x29FA, x) = O(0x25DA, x);
    O(0x29DA, x) = O(0x25BA, x);
    O(0x2A3A, x) = O(0x261A, x);
    O(0x2A1A, x) = O(0x25FA, x);
    set_vx(x, 0);
    set_vy(x, 0);
}

static void shuttle(u8 x)
{
    if (O(0x299A, x))
    {
        /* $DF13: resume after the pause */
        if (!--O(0x299A, x))
        {
            O(0x25DA, x) = O(0x29FA, x);
            O(0x25BA, x) = O(0x29DA, x);
            O(0x261A, x) = O(0x2A3A, x);
            O(0x25FA, x) = O(0x2A1A, x);
        }
        ss2_obj_common(x);
        return;
    }
    if (!--O(0x297A, x))
    {
        /* $DEC4: reverse */
        set_vx(x, (u16)(0 - ((O(0x25BA, x) << 8) | O(0x25DA, x))));
        set_vy(x, (u16)(0 - ((O(0x25FA, x) << 8) | O(0x261A, x))));
        O(0x297A, x) = 0x60;
        shuttle_pause(x);
    }
    else if (O(0x297A, x) == 0x30)
        shuttle_pause(x);
    ss2_obj_common(x);
}

/* $DF36: count hits; the $96th destroys it */
static void shuttle_hit(u8 x)
{
    if (++O(0x29BA, x) == 0x96)
    {
        /* $DF84 */
        u8 s;
        O(0x245A, x) = 0x40;
        ss2_score_award(O(0x279A, x));
        s = O(0x281A, x);
        O(0x2B5A, s)--;
        O(0x2C1A, s) = 0x64;
        ss2_obj_death(x);
        return;
    }
    O(0x245A, x) = 0x80;
    O(0x26DA, x) = 0x64;
    O(0x299A, x) = 0x5A;
    if (O(0x25DA, x) | O(0x25BA, x) | O(0x261A, x) | O(0x25FA, x))
    {
        O(0x29FA, x) = O(0x25DA, x);
        O(0x29DA, x) = O(0x25BA, x);
        O(0x2A3A, x) = O(0x261A, x);
        O(0x2A1A, x) = O(0x25FA, x);
        set_vx(x, 0);
        set_vy(x, 0);
    }
    ss2_obj_common(x);
}

/* ---- $D000 boss (stage 7) ---------------------------------------------- */

u8   ss2_aim_dir(u8 x);                 /* bank 3 $85E7 (ss2_obj1f.c)  */
void ss2_aim_velocity(u8 x);            /* bank 2 $7BEA (ss2_obj1f.c)  */
void ss2_boss_watch_start(void);        /* $E1E2 -> bank 2 $6E1A       */

#define OX16(x)     ((u16)((O(0x247A, x) << 8) | O(0x249A, x)))
#define OY16(x)     ((u16)((O(0x24DA, x) << 8) | O(0x24FA, x)))
#define PX16        ((u16)((P_XH << 8) | P_XL))

static void set_ox16(u8 x, u16 v) { O(0x247A, x) = (u8)(v >> 8); O(0x249A, x) = (u8)v; }
static void set_oy16(u8 x, u16 v) { O(0x24DA, x) = (u8)(v >> 8); O(0x24FA, x) = (u8)v; }
static void set_script(u8 x, u16 p) { O(0x295A, x) = (u8)p; O(0x293A, x) = (u8)(p >> 8); }
static void set_handler(u8 x, u16 h) { O(0x28DA, x) = (u8)h; O(0x28BA, x) = (u8)(h >> 8); }
static void set_hit(u8 x, u16 h) { O(0x291A, x) = (u8)h; O(0x28FA, x) = (u8)(h >> 8); }
static u16 vy16(u8 x) { return (u16)((O(0x25FA, x) << 8) | O(0x261A, x)); }

/* $D000: needs the boss-room lock ($7F) */
static void boss_spawn(void)
{
    u8 s = ZP(0x44), x;
    if (!Z_INLOCK1) return;
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_boss_watch_start();
    ss2_obj_init(x, 0xD51A);
    O(0x247A, x) = ZP(0x40);
    O(0x249A, x) = ZP(0x41);
    O(0x24DA, x) = ZP(0x42);
    O(0x24FA, x) = ZP(0x43);
    set_vx(x, 0);
    set_vy(x, 0);
    O(0x287A, x) = 1;
    O(0x281A, x) = s;
    O(0x297A, x) = 0;
    O(0x29BA, x) = 0;
    O(0x2AFA, x) = 0xE0;
    O(0x2B3A, x) = 0x18;
    O(0x2B5A, s) = 0xFF;
    ss2_obj_common(x);
}

/* $D485: shot from boss y, in the first free slot after it */
static void boss_fire(u8 y)
{
    u8 x;
    for (x = y; O(0x245A, x); )
        if (++x == 0x20) return;
    ss2_obj_init(x, 0xD546);
    O(0x297A, x) = y;
    O(0x287A, x) = 0;
    O(0x281A, x) = 0xBF;
    set_oy16(x, (u16)(OY16(y) + 0xFFF4));
    if (!O(0x29BA, y))
    {
        set_ox16(x, (u16)(OX16(y) + 0xFFF8));
        set_script(x, 0xDCB0);
    }
    else
    {
        set_ox16(x, (u16)(OX16(y) + 8));
        set_script(x, 0xDD53);
    }
    O(0x277A, x) = ss2_aim_dir(x);
    ss2_aim_velocity(x);
    ss2_obj_common(x);
}

/* $D505: shot hit */
static void boss_shot_hit(u8 x)
{
    if (O(0x245A, x) == 0x40) { O(0x245A, x) = 0; return; }
    O(0x245A, x) = 0x80;
    ss2_obj_common(x);
}

/* distance to the player: $26:$27 = |(x + $200) - (px + $210)|,
 * $299A = 1 when the player is to the right */
static u16 boss_distance(u8 x)
{
    u16 a = (u16)(OX16(x) + 0x200), p = (u16)(PX16 + 0x210), d = (u16)(a - p);
    O(0x299A, x) = 0;
    if (a < p) { O(0x299A, x) = 1; d = (u16)(0 - d); }
    ZP(0x26) = (u8)d; ZP(0x27) = (u8)(d >> 8);
    return d;
}

/* $D173: walking */
static void boss_walking(u8 x)
{
    O(0x263A, x) = (s8)O(0x25BA, x) < 0 ? 0xE8 : 0x17;
    O(0x265A, x) = 0x2F;
    if (ss2_obj_probe(x)) set_vx(x, 0);
    if (!--O(0x29DA, x)) O(0x297A, x) = 0;
    ss2_obj_common(x);
}

/* $D1CF/$D23C: count the action timer down */
static void boss_timer(u8 x)
{
    if (!--O(0x29DA, x)) O(0x297A, x) = 0;
    ss2_obj_common(x);
}

/* $D212: firing: the shot leaves after $18 frames */
static void boss_firing(u8 x)
{
    if (!--O(0x29FA, x))
    {
        set_script(x, O(0x29BA, x) ? 0xDB6F : 0xDAE2);
        O(0x267A, x) = 1;
        boss_fire(x);
    }
    boss_timer(x);
}

/* $D09A: AI */
static void boss_ai(u8 x)
{
    u16 d = boss_distance(x);
    u8 y;
    switch (O(0x297A, x))
    {
        case 0: break;
        case 1: boss_walking(x); return;
        case 3: boss_firing(x); return;
        default: boss_timer(x); return;
    }
    y = 3;
    if (!(d >> 8))
    {
        y = 0;
        if ((u8)d >= 0x50) y++;
        if ((u8)d >= 0x78) y++;
        if ((u8)d >= 0xB0) y++;
    }
    y = (u8)(y << 2);
    ss2_random();
    {
        /* $D10D: odds row at $D55C: walk toward, walk away, crouch, fire */
        u8 a = (u8)(ZP(0x8D) & 0x0F), k;
        for (k = 0; k < 3; k++)
        {
            u8 c = ss2_rom_byte((u16)(0xD55C + (u8)(y + k)));
            if (a < c) break;
            a = (u8)(a - c);
        }
        if (k < 2)
        {
            u8 right = O(0x299A, x) ? 1 : 0;
            if (k == 1) right ^= 1;
            set_vx(x, right ? 0x0100 : 0xFF00);
            O(0x29BA, x) = O(0x299A, x);
            set_script(x, O(0x299A, x) ? 0xD67F : 0xD56C);
            O(0x267A, x) = 1;
            O(0x297A, x) = 1;
            O(0x29DA, x) = 0x20;
            boss_walking(x);
            return;
        }
        O(0x29BA, x) = O(0x299A, x);
        if (k == 2)
        {
            /* $D19C: crouch */
            set_script(x, O(0x299A, x) ? 0xD823 : 0xD792);
            O(0x267A, x) = 1;
            set_vx(x, 0);
            O(0x297A, x) = 2;
            O(0x29DA, x) = 0x1E;
            boss_timer(x);
            return;
        }
        /* $D1DA: fire */
        set_script(x, O(0x299A, x) ? 0xD9CB : 0xD8B4);
        O(0x267A, x) = 1;
        set_vx(x, 0);
        O(0x297A, x) = 3;
        O(0x29DA, x) = 0x1E;
        O(0x29FA, x) = 0x18;
        boss_firing(x);
    }
}

/* $D067: walk in until x < $D0, then rebuild the room and fight */
static void boss_enter(u8 x)
{
    if (OX16(x) >= 0xD0) { ss2_obj_common(x); return; }
    ZP(0x8F) = 6;
    ZP(0x91) = 0x0A;
    ZP(0x93) = 0x67;
    ZP(0x94) = 0x58;
    ss2_unported(0x6984);                   /* bank 2: room tiles */
    Z_INLOCK1 = 0;
    ZP(0x64) = 1;
    set_handler(x, 0xD09A);
    boss_ai(x);
}

/* bank 2 $7001: boss explosion step. Not rebuilt (bank 2 owner); when it
 * finishes it continues at the instruction after the JMP $EE67 that
 * follows its call ($D265 / $D475). */
static u8 boss_explode(u8 x)
{
    (void)x;
    ss2_unported(0x7001);
    return 0;
}

static void boss2_ai(u8 x);

/* $D265/$D272: the first form is replaced by the flying second form */
static void boss_reform(u8 x)
{
    ZP(0x87) = 0;
    ss2_obj_init(x, 0xD530);
    O(0x24FA, x) = 0x78;
    O(0x24DA, x) = 0;
    O(0x251A, x) = 0;
    O(0x24BA, x) = 0;
    set_vx(x, 0);
    set_vy(x, 0);
    O(0x297A, x) = 0;
    set_script(x, O(0x29BA, x) ? 0xDC29 : 0xDBFC);
    ss2_obj_common(x);
}

/* $D25F: exploding first form */
static void boss_exploding(u8 x)
{
    if (boss_explode(x)) { boss_reform(x); return; }
    ss2_obj_common(x);
}

/* $D247 */
static void boss_hit(u8 x)
{
    if (O(0x245A, x) == 0x40)
    {
        ZP(0x87)++;
        O(0x297A, x) = 0x32;
        set_hit(x, 0xD25F);
        boss_exploding(x);
        return;
    }
    O(0x245A, x) = 0x80;
    ss2_obj_common(x);
}

/* $D2BA: second form: bounces around the top of the room */
static void boss2_ai(u8 x)
{
    u16 d;
    if (O(0x247A, x))
    {
        if (O(0x247A, x) & 0x80) { if (O(0x249A, x) <= 0xE0) O(0x249A, x) = 0xE0; }
        else if (O(0x249A, x) > 0x20) O(0x249A, x) = 0x20;
    }
    if (O(0x24FA, x) == 0x78 && !O(0x251A, x))
    {
        d = boss_distance(x);
        if (d >= 0x40)
        {
            /* $D332 */
            if (!O(0x299A, x)) { set_script(x, 0xDBFC); O(0x2A7A, x) = 0xF0; O(0x2A5A, x) = 0xFF; }
            else { set_script(x, 0xDC29); O(0x2A7A, x) = 0x10; O(0x2A5A, x) = 0x00; }
            O(0x267A, x) = 1;
            ss2_random();
            if (ZP(0x8D) & 0x80)
            {
                if ((s8)O(0x25FA, x) < 0) { set_vy(x, 0xFF00); O(0x2ABA, x) = 0x10; O(0x2A9A, x) = 0x00; }
                else { set_vy(x, 0x0100); O(0x2ABA, x) = 0xF0; O(0x2A9A, x) = 0xFF; }
            }
            else
            {
                if ((s8)O(0x25FA, x) < 0) { set_vy(x, 0xFE00); O(0x2ABA, x) = 0x20; O(0x2A9A, x) = 0x00; }
                else { set_vy(x, 0x0200); O(0x2ABA, x) = 0xE0; O(0x2A9A, x) = 0xFF; }
            }
        }
        else
        {
            /* $D3D1 */
            if (!O(0x299A, x)) { set_script(x, 0xDC56); O(0x2A7A, x) = 0xE0; O(0x2A5A, x) = 0xFF; }
            else { set_script(x, 0xDC83); O(0x2A7A, x) = 0x20; O(0x2A5A, x) = 0x00; }
            O(0x267A, x) = 1;
            set_vy(x, 0x0300);
            O(0x2ABA, x) = 0xE8;
            O(0x2A9A, x) = 0xFF;
        }
    }
    /* $D419: accelerate; the x acceleration flips when vy is 0 */
    if (!vy16(x))
    {
        u16 a = (u16)(0 - ((O(0x2A5A, x) << 8) | O(0x2A7A, x)));
        O(0x2A7A, x) = (u8)a;
        O(0x2A5A, x) = (u8)(a >> 8);
    }
    set_vx(x, (u16)(((O(0x25BA, x) << 8) | O(0x25DA, x)) + ((O(0x2A5A, x) << 8) | O(0x2A7A, x))));
    set_vy(x, (u16)(vy16(x) + ((O(0x2A9A, x) << 8) | O(0x2ABA, x))));
    ss2_obj_common(x);
}

/* $D46F: exploding second form; then $D475 frees the slot */
static void boss2_exploding(u8 x)
{
    if (boss_explode(x)) { ZP(0x87) = 0; O(0x245A, x) = 0; return; }
    ss2_obj_common(x);
}

/* $D459 */
static void boss2_hit(u8 x)
{
    if (O(0x245A, x) == 0x40)
    {
        O(0x297A, x) = 0x32;
        set_hit(x, 0xD46F);
        boss2_exploding(x);
        return;
    }
    O(0x245A, x) = 0x80;
    boss2_ai(x);
}

u8 ss2_obj0f_handler(u16 addr, u8 x)
{
    switch (addr)
    {
        case 0xD067: boss_enter(x); return 1;
        case 0xD09A: boss_ai(x); return 1;
        case 0xD247: boss_hit(x); return 1;
        case 0xD25F: boss_exploding(x); return 1;
        case 0xD2BA: boss2_ai(x); return 1;
        case 0xD459: boss2_hit(x); return 1;
        case 0xD46F: boss2_exploding(x); return 1;
        case 0xD505: boss_shot_hit(x); return 1;
        case 0xDEB0: shuttle(x); return 1;
        case 0xDF36: shuttle_hit(x); return 1;
    }
    return 0;
}

u8 ss2_spawn0f_handler(u16 addr)
{
    switch (addr)
    {
        case 0xD000: boss_spawn(); return 1;
        case 0xDDF6: shuttle_spawn(0); return 1;
        case 0xDDFA: shuttle_spawn(1); return 1;
        case 0xDE00: shuttle_spawn(2); return 1;
        case 0xDE06: shuttle_spawn(3); return 1;
    }
    return 0;
}

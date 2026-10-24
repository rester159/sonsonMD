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

/* ---- shared helpers ---------------------------------------------------- */

static void set_x16(u8 x, u16 v) { O(0x247A, x) = (u8)(v >> 8); O(0x249A, x) = (u8)v; }
static void set_y16(u8 x, u16 v) { O(0x24DA, x) = (u8)(v >> 8); O(0x24FA, x) = (u8)v; }
static void set_handler(u8 x, u16 h) { O(0x28DA, x) = (u8)h; O(0x28BA, x) = (u8)(h >> 8); }
static void set_hit(u8 x, u16 h) { O(0x291A, x) = (u8)h; O(0x28FA, x) = (u8)(h >> 8); }
static void stop(u8 x) { set_vx(x, 0); set_vy(x, 0); }

/* place a freshly initialised slot at the spawn point ($40-$43) */
static void at_spawn(u8 x)
{
    O(0x247A, x) = ZP(0x40);
    O(0x249A, x) = ZP(0x41);
    O(0x24DA, x) = ZP(0x42);
    O(0x24FA, x) = ZP(0x43);
}

/* the usual spawn epilogue: count the object on its spawner and share the
 * spawner's item-table group ($2CDA) */
static void claim(u8 x, u8 s)
{
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    if (!O(0x2CDA, s)) O(0x2CDA, s) = (u8)(O(0x289A, x) & 0x0F);
    else O(0x289A, x) = (u8)((O(0x289A, x) & 0xF0) | O(0x2CDA, s));
}

/* the usual spawn prologue: spawner idle, off screen, a free slot */
static u8 spawn_slot(void)
{
    u8 s = ZP(0x44);
    if (O(0x2B5A, s) | O(0x2C1A, s)) return 0xFF;
    if (ss2_spawn_visible()) return 0xFF;
    return ss2_obj_alloc();
}

/* death through the spawner: release the count, start the respawn delay */
static void die_respawn(u8 x)
{
    u8 s = O(0x281A, x);
    O(0x2B5A, s)--;
    O(0x2C1A, s) = 0x64;
    ss2_obj_death(x);
}

/* the common hit handler: dying -> die_respawn, otherwise back to normal */
static void hit_common(u8 x)
{
    if (O(0x245A, x) == 0x40) { die_respawn(x); return; }
    O(0x245A, x) = 0x80;
    ss2_obj_common(x);
}

/* x+1 : lo against the player, as the source's "INC A" 16-bit compares */
static u16 ox1(u8 x) { return (u16)(((u8)(O(0x247A, x) + 1) << 8) | O(0x249A, x)); }
static u16 px1(void) { return (u16)(((u8)(P_XH + 1) << 8) | P_XL); }

/* bank 3 $85E7: 16-way direction from the object to the player (0 right,
 * 4 down, 8 left, 12 up) */
u8 ss2_o17_aim(u8 x)
{
    u16 dx, dy, dx2, dx3, dx5, dy2, dy3, dy5, a;
    u8 y = 0;
    dx = (u16)(((P_XH << 8) | P_XL) + 0x110);
    dy = (u16)(((P_YH << 8) | P_YL) + 0x110);
    if (!(O(0x269A, x) & 0x20))
        a = (u16)(OX16(x) + 0x100 + (u8)(((O(0x253A, x) & 1) + 1) << 3));
    else
        a = (u16)(OX16(x) + 0x100);
    if (dx < a) { dx = (u16)~(u16)(dx - a); y = 8; }
    else dx = (u16)(dx - a);
    if (!(O(0x269A, x) & 0x20))
        a = (u16)(OY16(x) + 0x100 + (u8)(((O(0x253A, x) & 0x30) + 0x10) >> 1));
    else
        a = (u16)(OY16(x) + 0x100);
    if (dy < a) { dy = (u16)~(u16)(dy - a); y |= 0x10; }
    else dy = (u16)(dy - a);
    dx2 = (u16)(dx << 1); dx3 = (u16)(dx + dx2); dx5 = (u16)(dx3 + dx2);
    dy2 = (u16)(dy << 1); dy3 = (u16)(dy + dy2); dy5 = (u16)(dy3 + dy2);
    if (dx < dy5)
    {
        y++;
        if (dx2 < dy3)
        {
            y++;
            if (dx3 < dy2)
            {
                y++;
                if (dx5 < dy) y++;
            }
        }
    }
    return ss2_rom_byte((u16)(0x8712 + y));
}

/* bank 2 $7BEA: velocity from the direction ($277A) in the object's speed
 * table ($27FA indexes the pointer list at $7C1B) */
void ss2_o17_dir_velocity(u8 x)
{
    u8 i = O(0x27FA, x);
    u16 t = (u16)(ss2_rom_byte((u16)(0x7C1B + i)) | (ss2_rom_byte((u16)(0x7C1B + (u8)(i + 1))) << 8));
    u8 y = (u8)(O(0x277A, x) << 2);
    O(0x25DA, x) = ss2_rom_byte((u16)(t + y));
    O(0x25BA, x) = ss2_rom_byte((u16)(t + (u8)(y + 1)));
    O(0x261A, x) = ss2_rom_byte((u16)(t + (u8)(y + 2)));
    O(0x25FA, x) = ss2_rom_byte((u16)(t + (u8)(y + 3)));
}

/* bank 2 $7001: big-enemy death: every 4th frame a spark ($40 dying
 * object) somewhere in the 64 px square around the object; $297A counts
 * the sparks. Returns 1 when the last one was made (the source then
 * returns past its caller's JMP $EE67). */
u8 ss2_o17_burst(u8 x)
{
    u8 y;
    if (Z_FRAME & 3) return 0;
    ss2_sfx(0x17);
    for (y = 0x1F; ; y--)
    {
        if (!O(0x245A, y))
        {
            u16 bx = (u16)(OX16(x) + 0xFFCE), by = (u16)(OY16(x) + 0xFFCE);
            ss2_random();
            set_x16(y, (u16)(bx + (ZP(0x8D) & 0x3F)));
            ss2_random();
            set_y16(y, (u16)(by + (ZP(0x8D) & 0x3F)));
            stop(y);
            O(0x255A, y) = 0x84;
            O(0x253A, y) = 0x11;
            O(0x267A, y) = 1;
            set_script(y, 0x6F39);
            O(0x27DA, y) = 0xB2; O(0x27BA, y) = 0x2D;
            set_hit(y, 0xEE67);
            O(0x269A, y) = 0x07;
            O(0x26BA, y) = 0;
            O(0x287A, y) = 0;
            O(0x245A, y) = 0x40;
            break;
        }
        if (!y) break;
    }
    if (--O(0x297A, x)) return 0;
    ss2_sfx(0x29);
    return 1;
}

/* ---- $C000/$C00E: standing lobber ---------------------------------------
 * $297A variant (1: throws more often), $299A facing, $29BA phase counter,
 * $29DA high throw. */

static void lob_shot(u8 p);

/* $C0B4 */
static void lobber(u8 x)
{
    if (ox1(x) >= px1())
    {
        if (O(0x299A, x)) { O(0x299A, x) = 0; set_script(x, 0xC1B3); O(0x267A, x) = 1; }
    }
    else if (O(0x299A, x) != 1) { O(0x299A, x) = 1; set_script(x, 0xC1BA); O(0x267A, x) = 1; }
    switch (O(0x29BA, x))
    {
        case 0xB4:
            O(0x29BA, x) = 0;
            O(0x255A, x) = 0x83;
            break;
        case 0x0A:
        {
            u8 lim = 6;
            O(0x255A, x) = 0x84;
            if (O(0x297A, x)) { O(0x255A, x) = 0x82; lim = 3; }
            ZP(0x35) = lim;
            ss2_random();
            O(0x29DA, x) = (u8)((ZP(0x8D) & 7) >= lim);
        }
            /* fall through: $C144 */
        case 0x22:
            if (!O(0x29DA, x)) set_script(x, O(0x299A, x) ? 0xC1B8 : 0xC1B1);
            else set_script(x, O(0x299A, x) ? 0xC1C6 : 0xC1BF);
            O(0x267A, x) = 1;
            lob_shot(x);
            break;
    }
    O(0x29BA, x)++;
    ss2_obj_common(x);
}

/* $C1CD: shot from parent p (record $C25F) */
static void lob_shot(u8 p)
{
    u8 x = ss2_obj_alloc();
    u16 v;
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xC25F);
    if (!O(0x299A, p)) { O(0x263A, x) = 0; set_script(x, 0xC292); set_vx(x, 0xFE00); }
    else { O(0x263A, x) = 0x0F; set_script(x, 0xC299); set_vx(x, 0x0200); }
    O(0x265A, x) = 0x08;
    set_vy(x, 0);
    O(0x249A, x) = O(0x249A, p);
    O(0x247A, x) = O(0x247A, p);
    v = (u16)(O(0x24FA, p) + (O(0x29DA, p) ? 0x12 : 0x04));
    O(0x24FA, x) = (u8)v;
    O(0x24DA, x) = (u8)(O(0x24DA, p) + (v >> 8));
    O(0x287A, x) = 0;
    O(0x281A, x) = 0xBF;
    ss2_obj_common(x);
}

/* $C000/$C00E */
static void spawn_lobber(u16 rec, u8 v)
{
    u8 x = spawn_slot();
    ZP(0x35) = v;
    if (x == 0xFF) return;
    ss2_obj_init(x, rec);
    O(0x297A, x) = v;
    at_spawn(x);
    stop(x);
    O(0x287A, x) = 1;
    O(0x299A, x) = 0;
    O(0x29BA, x) = 0;
    claim(x, ZP(0x44));
    lobber(x);
}

/* $C191 */
static void lobber_hit(u8 x)
{
    if (O(0x245A, x) == 0x40) { die_respawn(x); return; }
    O(0x245A, x) = 0x80;
    lobber(x);
}

/* ---- $D277/$D27B: diving flyer --------------------------------------------
 * Sleeps until the player is within $50 px, then makes up to six $10-frame
 * dashes toward the player, remembering each direction as a nibble in
 * $299A/$29BA/$29DA, and retraces them home. $297A dash timer, $29FA dash
 * count, $2A1A one level dash used. */

/* $D27F */
static void spawn_diver(u8 v)
{
    u8 x = spawn_slot();
    ZP(0x35) = v;
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xD327);
    at_spawn(x);
    stop(x);
    if (!(O(0x247A, x) & 0x80)) set_script(x, 0xD4FF);
    if (v)
    {
        O(0x26DA, x) = 0x06;
        O(0x26FA, x) = 0x03;
        O(0x279A, x) = 0x09;
        O(0x289A, x) = 0x34;
        O(0x255A, x) = 0x8D;
    }
    O(0x297A, x) = 1;
    O(0x29FA, x) = 0;
    O(0x299A, x) = 0;
    O(0x29BA, x) = 0;
    O(0x29DA, x) = 0;
    O(0x287A, x) = 0;
    O(0x2A1A, x) = 0;
    claim(x, ZP(0x44));
    ss2_obj_common(x);
}

/* the "x + $50 - player x < $A0" proximity test with the ADC carry fed
 * into the SBC ($D342, $D5F0) */
static u8 near_x(u8 x, u8 add, u8 lim)
{
    u16 t = (u16)(O(0x249A, x) + add);
    u8 r = (u8)((u8)t - P_XL - (1 - (t >> 8)));
    return r < lim;
}

/* $D33D */
static void diver_wait(u8 x)
{
    if (!O(0x247A, x) && near_x(x, 0x50, 0xA0)) set_handler(x, 0xD35C);
    ss2_obj_common(x);
}

/* face the script toward the player ($D3E8/$D48F) */
static void diver_face(u8 x)
{
    u8 b = O(0x249A, x) < P_XL;
    set_script(x, 0xD4F8);
    if ((u8)(O(0x247A, x) + 1) >= 1 + b) set_script(x, 0xD4FF);
}

static void diver_return(u8 x);

/* $D383: dash in direction d (0-8, through the $D506 table) */
static void diver_dash(u8 x, u8 d)
{
    u8 t, v;
    O(0x277A, x) = d;
    t = (u8)(O(0x29FA, x) >> 1);
    O(0x277A, x) = ss2_rom_byte((u16)(0xD506 + (u8)(t * 9 + O(0x277A, x))));
    v = O(0x277A, x);
    if (O(0x29FA, x) & 1) v = (u8)(v << 4);
    R(0x299A + ((O(0x29FA, x) & 6) << 4) + x) |= v;
    O(0x297A, x) = 0x10;
    O(0x29FA, x)++;
    ss2_o17_dir_velocity(x);
    diver_face(x);
    ss2_obj_common(x);
}

/* $D404 */
static void diver_go_home(u8 x)
{
    set_handler(x, 0xD42F);
    O(0x297A, x) = 1;
    diver_return(x);
}

/* $D35C */
static void diver_attack(u8 x)
{
    if (--O(0x297A, x)) { ss2_obj_common(x); return; }
    if (O(0x29FA, x) == 6) { diver_go_home(x); return; }
    if (P_YL >= O(0x24FA, x))
    {
        u8 d = ss2_o17_aim(x);
        if (d >= 9) { diver_go_home(x); return; }
        diver_dash(x, d);
        return;
    }
    /* $D416: player above: one level dash */
    if (O(0x2A1A, x)) { diver_go_home(x); return; }
    O(0x2A1A, x)++;
    diver_dash(x, P_XL < O(0x249A, x) ? 8 : 0);
}

/* $D42F: retrace the dashes backwards */
static void diver_return(u8 x)
{
    u8 v;
    if (--O(0x297A, x)) { ss2_obj_common(x); return; }
    if (!O(0x29FA, x))
    {
        /* $D4AB */
        set_handler(x, 0xD33D);
        stop(x);
        O(0x297A, x) = 1;
        O(0x29FA, x) = 0;
        O(0x299A, x) = 0;
        O(0x29BA, x) = 0;
        O(0x29DA, x) = 0;
        O(0x2A1A, x) = 0;
        ss2_obj_common(x);
        return;
    }
    O(0x29FA, x)--;
    v = R(0x299A + ((O(0x29FA, x) & 6) << 4) + x);
    if (O(0x29FA, x) & 1) v = (u8)(v >> 4);
    v &= 0x0F;
    O(0x277A, x) = (u8)((v + 8) & 0x0F);
    O(0x297A, x) = 0x10;
    ss2_o17_dir_velocity(x);
    diver_face(x);
    ss2_obj_common(x);
}

/* ---- $D521-$D531: pod -----------------------------------------------------
 * Waits for the player within $40 px, then opens; variants 2/3 burst into
 * six shots. $297A variant, $299A cycle timer. */

/* $D535 */
static void spawn_pod(u8 v)
{
    u8 x = spawn_slot();
    ZP(0x35) = v;
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xD5BF);
    stop(x);
    at_spawn(x);
    O(0x297A, x) = v;
    if (v >= 2)
    {
        O(0x26DA, x) = 0x18;
        O(0x26FA, x) = 0x03;
        O(0x289A, x) = 0x13;
        O(0x279A, x) = 0x09;
        O(0x255A, x) = 0x82;
    }
    O(0x287A, x) = 0;
    claim(x, ZP(0x44));
    ss2_obj_common(x);
}

/* $D622 */
static void pod_open(u8 x)
{
    O(0x267A, x) = 1;
    O(0x269A, x) = 0x08;
    O(0x299A, x) = 0xC8;
    set_script(x, (O(0x297A, x) & 1) ? 0xD788 : 0xD768);
    set_handler(x, 0xD600);
    ss2_obj_common(x);
}

/* $D5EB */
static void pod_wait(u8 x)
{
    if (!O(0x247A, x) && near_x(x, 0x40, 0x80)) { pod_open(x); return; }
    ss2_obj_common(x);
}

/* $D659: six shots (record $D5D5) for variants 2 and 3 */
static void pod_burst(u8 p)
{
    u8 x;
    if (O(0x297A, p) < 2) return;
    ZP(0x40) = O(0x247A, p);
    ZP(0x41) = O(0x249A, p);
    ZP(0x42) = O(0x24DA, p);
    ZP(0x43) = O(0x24FA, p);
    ZP(0x36) = O(0x297A, p);
    for (ZP(0x35) = 0; ZP(0x35) != 6; ZP(0x35)++)
    {
        u16 v;
        x = ss2_obj_alloc();
        if (x == 0xFF) return;
        ss2_obj_init(x, 0xD5D5);
        O(0x25DA, x) = ss2_rom_byte((u16)(0xD6F3 + (u8)(ZP(0x35) << 1)));
        O(0x25BA, x) = ss2_rom_byte((u16)(0xD6F4 + (u8)(ZP(0x35) << 1)));
        set_vy(x, 0xFD00);
        O(0x297A, x) = ZP(0x36);
        if (ZP(0x36) & 1) set_vy(x, 0x0300);
        v = (u16)(ZP(0x41) + 8);
        O(0x249A, x) = (u8)v;
        O(0x247A, x) = (u8)(ZP(0x40) + (v >> 8));
        O(0x24FA, x) = ZP(0x43);
        O(0x24DA, x) = ZP(0x42);
        if (ZP(0x36))
        {
            v = (u16)(ZP(0x43) + 0x10);
            O(0x24FA, x) = (u8)v;
            O(0x24DA, x) = (u8)(ZP(0x42) + (v >> 8));
        }
        O(0x287A, x) = 0;
        O(0x281A, x) = 0xBF;
    }
}

/* $D600 */
static void pod_cycle(u8 x)
{
    if (!--O(0x299A, x)) { pod_open(x); return; }
    if (O(0x299A, x) == 0x96) pod_burst(x);
    if (O(0x299A, x) < 0x50) O(0x269A, x) = 0x0F;
    ss2_obj_common(x);
}

/* $D6FF: pod shot: arcs under gravity (upward for odd variants) */
static void pod_shot(u8 x)
{
    u16 v;
    O(0x263A, x) = 8;
    O(0x265A, x) = 8;
    if (ss2_obj_probe(x)) { O(0x245A, x) = 0; ss2_obj_common(x); return; }
    v = (u16)((O(0x25FA, x) << 8) | O(0x261A, x));
    v = (u16)(v + ((O(0x297A, x) & 1) ? 0xFFE0 : 0x0020));
    set_vy(x, v);
    ss2_obj_common(x);
}

/* $D743 */
static void pod_hit(u8 x)
{
    u8 s;
    if (O(0x245A, x) != 0x40) { O(0x245A, x) = 0x80; ss2_obj_common(x); return; }
    s = O(0x281A, x);
    O(0x2B5A, s) = 0;
    O(0x2C1A, s) = 0x64;
    ss2_obj_death(x);
}

/* ---- $D7AD-$D7B7: crawler -------------------------------------------------
 * Creeps toward the player, re-aiming every $28 frames; pauses $78 frames
 * at walls and ledges. $297A facing (1 = left), $299A aim timer, $29BA
 * pause timer. Big death: $7001 sparks. */

/* $D98F: $297A = player left of the object */
static void crawler_face(u8 x)
{
    u8 lo = (u8)(P_XL + 0x10);
    u8 b = lo < O(0x249A, x);
    O(0x297A, x) = 0;
    if (1 < (u8)(O(0x247A, x) + 1) + b) O(0x297A, x)++;
}

/* $D7BB */
static void spawn_crawler(u8 v)
{
    u8 x = spawn_slot(), y;
    ZP(0x35) = v;
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xD86F);
    at_spawn(x);
    set_vy(x, 0);
    set_vx(x, 0x0080);
    crawler_face(x);
    if (O(0x297A, x)) set_vx(x, 0xFF80);
    if (O(0x297A, x)) set_script(x, 0xDA40);
    y = (u8)(v << 2);
    O(0x26DA, x) = ss2_rom_byte((u16)(0xD9AD + y));
    O(0x26FA, x) = ss2_rom_byte((u16)(0xD9AE + y));
    O(0x279A, x) = ss2_rom_byte((u16)(0xD9AF + y));
    O(0x289A, x) = ss2_rom_byte((u16)(0xD9B0 + y));
    O(0x287A, x) = 1;
    O(0x299A, x) = 0x18;
    O(0x29BA, x) = 0;
    claim(x, ZP(0x44));
    ss2_obj_common(x);
}

/* $D885 */
static void crawler(u8 x)
{
    if (!--O(0x299A, x))
    {
        crawler_face(x);
        set_vx(x, O(0x297A, x) ? 0xFF80 : 0x0080);
        O(0x267A, x) = 1;
        set_script(x, O(0x297A, x) ? 0xDA40 : 0xD9B9);
        O(0x299A, x) = 0x28;
    }
    /* $D8C9 */
    O(0x265A, x) = 0x24;
    O(0x263A, x) = O(0x297A, x) ? 0xE8 : 0x18;
    if (ss2_obj_probe(x))
    {
        O(0x263A, x) = O(0x297A, x) ? 0xE8 : 0x18;
        O(0x265A, x) = 0x18;
        if (!ss2_obj_probe(x))
        {
            O(0x265A, x) = 0xE8;
            if (!ss2_obj_probe(x)) { ss2_obj_common(x); return; }
        }
    }
    /* $D908: blocked: back off and pause */
    O(0x29BA, x) = 0x78;
    set_vx(x, O(0x297A, x) ? 0x0080 : 0xFF80);
    set_handler(x, 0xD933);
    ss2_obj_common(x);
}

/* $D933 */
static void crawler_pause(u8 x)
{
    if (--O(0x29BA, x)) { ss2_obj_common(x); return; }
    O(0x299A, x) = 1;
    set_handler(x, 0xD885);
    crawler(x);
}

/* $D970 (then $D976 once the sparks are done) */
static void crawler_burst(u8 x)
{
    u8 s;
    if (!ss2_o17_burst(x)) { ss2_obj_common(x); return; }
    s = O(0x281A, x);
    O(0x2B5A, s) = 0;
    O(0x2C1A, s) = 0x64;
    O(0x269A, x) &= 0xDF;
    ss2_obj_death(x);
}

/* $D94D */
static void crawler_hit(u8 x)
{
    if (O(0x245A, x) != 0x40) { O(0x245A, x) = 0x80; ss2_obj_common(x); return; }
    O(0x267A, x) = 0xFF;
    O(0x297A, x) = 0x14;
    set_hit(x, 0xD970);
    crawler_burst(x);
}

/* ---- $DEC2-$DEEE: scenery animations (record $DF76, script only) ------ */

/* $DF64: stage bit in the flag byte */
static u8 stage_flag(u8 flags)
{
    u8 b = 1, i;
    ZP(0x30) = flags;
    for (i = Z_STAGE; i; i--) b = (u8)(b << 1);
    ZP(0x37) = b;
    return (u8)(b & flags);
}

/* $DEFB */
static void spawn_scenery(u8 v)
{
    u8 s = ZP(0x44), x;
    ZP(0x35) = v;
    if (O(0x2B5A, s)) return;
    if (ss2_spawn_visible()) return;
    x = ss2_obj_alloc_top();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xDF76);
    stop(x);
    at_spawn(x);
    O(0x297A, x) = (u8)(v >> 1);
    set_script(x, (u16)(ss2_rom_byte((u16)(0xDF8C + (u8)(v << 1))) | (ss2_rom_byte((u16)(0xDF8D + (u8)(v << 1))) << 8)));
    O(0x255A, x) = ss2_rom_byte((u16)(0xDF98 + (u8)(v >> 1)));
    O(0x287A, x) = 1;
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
}

/* $DEC2/$DEC6: only while the spawner is idle */
static void spawn_scenery_idle(u8 v)
{
    u8 s = ZP(0x44);
    ZP(0x35) = v;
    if (O(0x2B5A, s) | O(0x2C1A, s)) return;
    spawn_scenery(v);
}

/* $DED5-$DEEE: only while the stage's bit in $7A / $7B is clear */
static void spawn_scenery_flag(u8 v, u8 flags)
{
    ZP(0x35) = v;
    if (stage_flag(flags)) return;
    spawn_scenery(v);
}

/* ---- dispatch ------------------------------------------------------------ */

void ss2_o17_boss_spawn(u8 v);
u8 ss2_o17_boss_handler(u16 addr, u8 x);

u8 ss2_obj17_handler(u16 addr, u8 x)
{
    switch (addr)
    {
        case 0xC0B4: lobber(x); return 1;
        case 0xC191: lobber_hit(x); return 1;
        case 0xC275: shot_hit(x); return 1;     /* same code as $D1F0 */
        case 0xC287: shot(x); return 1;         /* same code as $D202 */
        case 0xCEE3: walker(x); return 1;
        case 0xCF4A: thrower(x); return 1;
        case 0xD0E3: walker_hit(x); return 1;
        case 0xD1F0: shot_hit(x); return 1;
        case 0xD202: shot(x); return 1;
        case 0xD33D: diver_wait(x); return 1;
        case 0xD35C: diver_attack(x); return 1;
        case 0xD42F: diver_return(x); return 1;
        case 0xD4D8: hit_common(x); return 1;
        case 0xD5EB: pod_wait(x); return 1;
        case 0xD600: pod_cycle(x); return 1;
        case 0xD6FF: pod_shot(x); return 1;
        case 0xD743: pod_hit(x); return 1;
        case 0xD885: crawler(x); return 1;
        case 0xD933: crawler_pause(x); return 1;
        case 0xD94D: crawler_hit(x); return 1;
        case 0xD970: crawler_burst(x); return 1;
    }
    return ss2_o17_boss_handler(addr, x);
}

u8 ss2_spawn17_handler(u16 addr)
{
    switch (addr)
    {
        case 0xC000: spawn_lobber(0xC088, 0); return 1;
        case 0xC00E: spawn_lobber(0xC09E, 1); return 1;
        case 0xC2A0: ss2_o17_boss_spawn(0); return 1;
        case 0xC2A4: ss2_o17_boss_spawn(1); return 1;
        case 0xCE60: spawn_walker(0xD20D); return 1;
        case 0xCE6A: spawn_walker(0xD223); return 1;
        case 0xD277: spawn_diver(0); return 1;
        case 0xD27B: spawn_diver(1); return 1;
        case 0xD521: spawn_pod(0); return 1;
        case 0xD525: spawn_pod(1); return 1;
        case 0xD52B: spawn_pod(2); return 1;
        case 0xD531: spawn_pod(3); return 1;
        case 0xD7AD: spawn_crawler(0); return 1;
        case 0xD7B1: spawn_crawler(1); return 1;
        case 0xD7B7: spawn_crawler(2); return 1;
        case 0xDEC2: spawn_scenery_idle(0); return 1;
        case 0xDEC6: spawn_scenery_idle(1); return 1;
        case 0xDED5: spawn_scenery_flag(2, ZP(0x7A)); return 1;
        case 0xDEDB: spawn_scenery_flag(3, ZP(0x7A)); return 1;
        case 0xDEE8: spawn_scenery_flag(4, ZP(0x7B)); return 1;
        case 0xDEEE: spawn_scenery_flag(5, ZP(0x7B)); return 1;
    }
    return 0;
}

/* ---- $C2A0/$C2A4: the mirror boss (stages 3 and 7) ----------------------
 * Lives in slot 0; picks moves from the player's state ($CA37/$CA71
 * tables): dash, jumps, a bomb (slot 6, bursting into slots 6-9) and a
 * staff of up to $22B5 segments (slots 1-5) like the player's.
 * $297A variant (stage 7), $299A move (2-12), $277A player to the right,
 * $29BA staff started, $29DA airborne, $29FA staff length, $2A1A staff
 * hold, $2A3A dash started, $2A5A dash timer, $2A7A idle timer. */

void ss2_o17_boss_sparks(u8 x);

/* bank 2 $6F40: boss death: item list from the run-length list at rle into
 * $2243-$2262, freeze the others ($87), then $7001 sparks with hit handler
 * $6F6D */
void ss2_o17_boss_death(u8 x, u16 rle)
{
    u8 i = 0, y = 0, n;
    ZP(0x87) = 1;
    for (;;)
    {
        n = ss2_rom_byte((u16)(rle + y));
        y++;
        do
        {
            R(0x2243 + i) = ss2_rom_byte((u16)(rle + y));
            if (++i == 0x20) goto done;
        } while (--n);
        y++;
    }
done:
    O(0x297A, x) = 0x32;
    set_hit(x, 0x6F6D);
    ss2_o17_boss_sparks(x);
}

/* bank 2 $6F6D */
void ss2_o17_boss_sparks(u8 x)
{
    if (!ss2_o17_burst(x)) { ss2_obj_common(x); return; }
    /* $6F73: every composite piece drops its share of the items through
     * bank 3 $90CB/$9481 (item code not rebuilt), then bank 9 $D86A */
    ss2_unported(0x6F73);
}

static void boss_think(u8 x);
static void boss_state(u8 x);

/* $C2A8 */
void ss2_o17_boss_spawn(u8 v)
{
    u8 i, s = ZP(0x44);
    ZP(0x35) = v;
    if (!ZP(0x7F)) return;
    for (i = 0; i < 10; i++) O(0x245A, i) = 0;
    if (!v)
    {
        /* $E1E2: task 3 becomes bank 2 $6E1A (wait for an empty object
         * table, then the stage clear) */
        ZP(0x10) = 0x1A; ZP(0x11) = 0x6E;
        /* task slot 2 as the scheduler leaves it at the frame end: started,
         * first $B9B8 wait done (state 4, saved stack pointer $89) */
        ZP(0x08) = 0x04; ZP(0x0A) = 0x89; ZP(0x0B) = 0x6E;
    }
    ss2_obj_init(0, 0xC345);
    at_spawn(0);
    stop(0);
    if (v)
    {
        O(0x26DA, 0) = 0x41;
        O(0x26FA, 0) = 0x03;
        O(0x279A, 0) = 0x18;
    }
    O(0x287A, 0) = 1;
    O(0x297A, 0) = v;
    O(0x29BA, 0) = 0;
    O(0x29DA, 0) = 0;
    O(0x2A3A, 0) = 0;
    O(0x299A, 0) = 0;
    O(0x2A7A, 0) = 0;
    O(0x281A, 0) = s;
    O(0x2B5A, s) = 0xFF;
    ss2_obj_common(0);
}

/* $C35B: walk in until x < $D0, then (stage 3) the talk window, lock */
static void boss_enter(u8 x)
{
    if (OX16(x) >= 0xD0) { ss2_obj_common(x); return; }
    if (!O(0x297A, x))
    {
        ZP(0x8F) = 0x06; ZP(0x91) = 0x0A; ZP(0x93) = 0x40; ZP(0x94) = 0x57;
        /* bank 2 $6984: message window; runs for several frames inside the
         * object task (task 2 waits on $B9B8) */
        ss2_unported(0x6984);
    }
    Z_HSCROLL = 1;
    Z_INLOCK1 = 0;
    set_handler(x, 0xC393);
    ss2_obj_common(x);
}

/* $C616: landed */
static void boss_land(u8 x)
{
    ss2_obj_snap_y(x);
    stop(x);
    O(0x299A, x) = 0;
    O(0x29DA, x) = 0;
    set_script(x, O(0x277A, x) ? 0xCAD5 : 0xCAC0);
    O(0x2A7A, x) = 0x0A;
    boss_think(x);
}

/* $C5D3: airborne */
static void boss_air(u8 x)
{
    u16 v;
    O(0x265A, x) = 0;
    O(0x263A, x) = (O(0x25BA, x) & 0x80) ? 0xF0 : 0x10;
    if (ss2_obj_probe(x)) set_vx(x, 0);
    if (!(O(0x25FA, x) & 0x80))
    {
        O(0x263A, x) = 0;
        O(0x265A, x) = 0x10;
        if (ss2_obj_probe(x)) { boss_land(x); return; }
    }
    v = (u16)(((O(0x25FA, x) << 8) | O(0x261A, x)) + 0x40);
    set_vy(x, v);
    ss2_obj_common(x);
}

/* $C500/$C550/$C59C: jump toward, away, or straight up (kind 0/1/2) */
static void boss_jump(u8 x, u8 kind)
{
    if (!O(0x29DA, x))
    {
        set_vy(x, 0xFA80);
        if (kind == 0) set_vx(x, O(0x277A, x) ? 0x0180 : 0xFE80);
        else if (kind == 1) set_vx(x, O(0x277A, x) ? 0xFE80 : 0x0180);
        else set_vy(x, 0);
        set_script(x, O(0x277A, x) ? 0xCB80 : 0xCB95);
        O(0x267A, x) = 1;
        O(0x29DA, x) = 1;
    }
    boss_air(x);
}

/* $C484: dash at the player for $20 frames or until a wall */
static void boss_dash(u8 x)
{
    if (!O(0x2A3A, x))
    {
        set_vx(x, O(0x277A, x) ? 0x0180 : 0xFE80);
        set_script(x, O(0x277A, x) ? 0xCAEA : 0xCB35);
        O(0x267A, x) = 1;
        O(0x2A3A, x) = 1;
        O(0x2A5A, x) = 0x20;
    }
    if (--O(0x2A5A, x))
    {
        O(0x265A, x) = 0;
        O(0x263A, x) = O(0x277A, x) ? 0x10 : 0xF0;
        if (!ss2_obj_probe(x)) { ss2_obj_common(x); return; }
    }
    /* $C4E8 */
    stop(x);
    O(0x299A, x) = 0;
    O(0x2A3A, x) = 0;
    O(0x2A7A, x) = 0;
    boss_think(x);
}

/* $C809: lob a bomb (slot 6, record $C9F5) unless slots 6-9 are busy */
static void boss_bomb(void)
{
    u8 d;
    u16 v;
    if (O(0x245A, 6) | O(0x245A, 7) | O(0x245A, 8) | O(0x245A, 9)) return;
    ss2_obj_init(6, 0xC9F5);
    O(0x249A, 6) = O(0x249A, 0);
    O(0x247A, 6) = O(0x247A, 0);
    O(0x24FA, 6) = O(0x24FA, 0);
    O(0x24DA, 6) = O(0x24DA, 0);
    O(0x263A, 6) = 0;
    O(0x265A, 6) = 8;
    set_vy(6, 0xFA00);
    d = (u8)(P_XL - O(0x249A, 6));
    if (P_XL < O(0x249A, 6)) d = (u8)(0 - d);
    if (d <= 0x40) d = 0x40;
    v = (u16)((u16)((u16)d << 8) >> 6);
    v = (u16)(v + 0x60);
    if (!O(0x277A, 0)) v = (u16)(0 - v);
    set_vx(6, v);
}

/* $C7B9: hold the staff $0A frames, then drop it */
static void boss_staff_end(u8 x)
{
    u8 i;
    if (O(0x2A1A, x) != 0x0A) { O(0x2A1A, x)++; ss2_obj_common(x); return; }
    for (i = 1; i < 6; i++) O(0x245A, i) = 0;
    set_script(x, O(0x277A, x) ? 0xCAD5 : 0xCAC0);
    O(0x2A7A, x) = 0x0A;
    O(0x29BA, x) = 0;
    O(0x299A, x) = 0;
    boss_think(x);
}

/* $C64C: extend the staff by one segment per frame */
static void boss_staff(u8 x)
{
    u8 n, s, k;
    if (!O(0x29BA, x))
    {
        if (!O(0x29DA, x)) stop(x);
        set_script(x, O(0x277A, x) ? 0xCBAA : 0xCBBF);
        O(0x29BA, x) = 1;
        O(0x267A, x) = 1;
        O(0x29FA, x) = 0;
    }
    if (O(0x29FA, x) == P_STAFFMAX) { boss_staff_end(x); return; }
    /* $C692: new segment (record $C9DF) */
    n = O(0x29FA, x);
    if (!n)
    {
        s = 1;
        ss2_obj_init(s, 0xC9DF);
        set_script(s, 0xCC07);
        if (!O(0x277A, 0))
        {
            set_script(s, 0xCC10);
            O(0x255A, s) = ss2_rom_byte((u16)(0xCA31 + Z_STAFFPAL));
        }
    }
    else
    {
        s = (u8)(n + 1);
        ss2_obj_init(s, 0xC9DF);
        set_script(s, 0xCBFE);
    }
    O(0x26FA, s) = (u8)(R(0x22B4) >> 1);
    stop(s);
    O(0x2ADA, s) = 0;
    O(0x2AFA, s) = 0;
    O(0x2B1A, s) = 0;
    O(0x2B3A, s) = 0;
    O(0x287A, s) = 1;
    set_y16(s, (u16)(OY16(0) - 2));
    n = O(0x29FA, 0);
    if (O(0x277A, 0))
    {
        u8 t = (u8)(n << 4);
        for (k = 1; ; k++)
        {
            set_x16(k, (u16)((u16)(t + 0x18) + OX16(0)));
            if ((u8)(k - 1) == n) break;
            t = (u8)(t - 0x10);
        }
    }
    else
    {
        u16 t = (u16)(0 - (u8)(n << 4));
        for (k = 1; ; k++)
        {
            set_x16(k, (u16)((u16)(t - 0x18) + OX16(0)));
            if ((u8)(k - 1) == n) break;
            t = (u16)(t + 0x10);
        }
    }
    /* $C7A6 */
    O(0x29FA, x)++;
    O(0x2A1A, x) = 0;
    if (!O(0x29DA, x)) { ss2_obj_common(x); return; }
    boss_jump(x, 0);
}

/* $C46E: run the current move ($C478 table) */
static void boss_state(u8 x)
{
    switch (O(0x299A, x))
    {
        case 0x02: boss_staff(x); return;
        case 0x04: boss_dash(x); return;
        case 0x06: boss_bomb(); O(0x299A, x) = 2; boss_state(x); return;   /* $C7FC */
        case 0x08: boss_jump(x, 0); return;
        case 0x0A: boss_jump(x, 1); return;
        case 0x0C: boss_jump(x, 2); return;
    }
    ss2_unported(0xC475);
}

/* $C393: idle, or pick a move */
static void boss_think(u8 x)
{
    if (!O(0x299A, x))
    {
        u8 y, ti, yy;
        u16 tab, a, b, d;
        if (O(0x2A7A, x)) { O(0x2A7A, x)--; ss2_obj_common(x); return; }
        /* $C3A6: a player shot within $60 px ahead of x - $30? */
        for (y = 0; y < 4; y++)
            if (R(0x22E5 + y) == 0x80 && (u8)(R(0x22F3 + y) - (u8)(O(0x249A, x) - 0x30)) < 0x60) break;
        ti = (y < 4) ? 8 : (u8)((u8)(R(0x22AB) + R(0x22A9)) << 1);
        if (!O(0x297A, x)) tab = (u16)(ss2_rom_byte((u16)(0xCA37 + ti)) | (ss2_rom_byte((u16)(0xCA38 + ti)) << 8));
        else tab = (u16)(ss2_rom_byte((u16)(0xCA71 + ti)) | (ss2_rom_byte((u16)(0xCA72 + ti)) << 8));
        /* $C3ED: distance class */
        a = (u16)(OX16(x) + 0x100);
        b = (u16)(((P_XH << 8) | P_XL) + 0x110);
        d = (u16)(a - b);
        O(0x277A, x) = 0;
        if (a < b) { O(0x277A, x) = 1; d = (u16)(0 - d); }
        yy = 8;
        if (!(d >> 8))
        {
            u8 c = 0;
            if ((u8)d >= 0x40) c++;
            if ((u8)d >= 0x80) c++;
            yy = (u8)(c << 2);
        }
        ss2_random();
        tab = (u16)(tab + (ZP(0x8D) & 3));
        O(0x299A, x) = ss2_rom_byte((u16)(tab + yy));
    }
    boss_state(x);
}

/* $C8C2: the bomb bursts into four pieces (slots 6-9, record $CA0B) */
static void bomb_burst(void)
{
    static const u16 scr[4] = { 0xCC22, 0xCCBF, 0xCD56, 0xCDE1 };
    u8 i;
    for (i = 0; i < 4; i++)
    {
        ss2_obj_init((u8)(6 + i), 0xCA0B);
        set_script((u8)(6 + i), scr[i]);
    }
    for (i = 6; i < 10; i++) stop(i);
    for (i = 7; i < 10; i++)
    {
        O(0x247A, i) = O(0x247A, 6);
        O(0x249A, i) = O(0x249A, 6);
        set_y16(i, (u16)(OY16(6) - (u8)((i - 6) << 4)));
    }
    ss2_obj_common(6);
}

/* $C8A9: bomb in flight */
static void bomb(u8 x)
{
    if (ss2_obj_probe(x)) { bomb_burst(); return; }
    set_vy(x, (u16)(((O(0x25FA, x) << 8) | O(0x261A, x)) + 0x40));
    ss2_obj_common(x);
}

/* $C99B */
static void boss_hit(u8 x)
{
    u8 i;
    if (O(0x245A, x) != 0x40) { O(0x245A, x) = 0x80; ss2_obj_common(x); return; }
    for (i = 1; i < 10; i++) O(0x269A, i) = 0x27;
    ss2_o17_boss_death(x, O(0x297A, x) ? 0xCA29 : 0xCA21);
}

u8 ss2_o17_boss_handler(u16 addr, u8 x)
{
    switch (addr)
    {
        case 0xC35B: boss_enter(x); return 1;
        case 0xC393: boss_think(x); return 1;
        case 0xC99B: boss_hit(x); return 1;
        case 0xC8A9: bomb(x); return 1;
        case 0xC8C2: bomb_burst(); return 1;
    }
    return 0;
}

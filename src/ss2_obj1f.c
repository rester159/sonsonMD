/* SON SON II bank $1F objects: the two-slot ground boss (four variants),
 * rebuilt from HuCard bank 31 $D3E0-$DAFD.
 *
 *   $D3E0/$D3EE/$D3FC/$D40A  spawn variant 0-3 (records $DAFE/$DB14/$DB2A/
 *          $DB40) once the boss room locked the input ($7F). Slot x is the
 *          boss, slot x+1 a body box ($DB56, handler $EE67) carried at the
 *          offset ($299B:$29BB, $29DB:$29FB) of slot x+1.
 *   $D4C2 (variants 0/2) / $D529 (1/3): walk in until x < $D0, then the
 *          room is set up ($6984) and the AI ($D549) runs: walk to/away,
 *          jump, crouch, fire, picked from the per-variant odds table at
 *          ($2A7A:$2A5A) by distance band.
 *   $D7F6.. hit handlers (knockback $D83E; death via $6F40),
 *   $DAED/$DADB its shots (aimed with bank 3 $85E7, speed bank 2 $7BEA).
 *
 * Extra fields of the boss slot: $297A action (0 decide, 1 walk, 2 jump,
 * 3 crouch, 4 stand, 5 fire), $299A player is to the right, $29BA facing
 * (1 = right), $29DA action timer, $29FA knockback latch, $2A9A variant,
 * $2A5A/$2A7A odds table pointer (lo/hi).
 */
#include "ss2_ram.h"
#include "ss2_logic.h"
#include "ss2_task.h"

#define OX16(x)     ((u16)((O(0x247A, x) << 8) | O(0x249A, x)))
#define OY16(x)     ((u16)((O(0x24DA, x) << 8) | O(0x24FA, x)))
#define PX16        ((u16)((P_XH << 8) | P_XL))

static void set_ox16(u8 x, u16 v) { O(0x247A, x) = (u8)(v >> 8); O(0x249A, x) = (u8)v; }
static void set_oy16(u8 x, u16 v) { O(0x24DA, x) = (u8)(v >> 8); O(0x24FA, x) = (u8)v; }
static void set_vx(u8 x, u16 v) { O(0x25BA, x) = (u8)(v >> 8); O(0x25DA, x) = (u8)v; }
static void set_vy(u8 x, u16 v) { O(0x25FA, x) = (u8)(v >> 8); O(0x261A, x) = (u8)v; }
static u16 vx16(u8 x) { return (u16)((O(0x25BA, x) << 8) | O(0x25DA, x)); }
static u16 vy16(u8 x) { return (u16)((O(0x25FA, x) << 8) | O(0x261A, x)); }
static void set_script(u8 x, u16 p) { O(0x295A, x) = (u8)p; O(0x293A, x) = (u8)(p >> 8); }
static void set_handler(u8 x, u16 h) { O(0x28DA, x) = (u8)h; O(0x28BA, x) = (u8)(h >> 8); }

/* ---- fixed-bank helpers the bosses need (also used by ss2_obj0f.c);
 * candidates for the fixed-bank files once their owners port them ------- */

/* bank 3 $85E7: 16-way direction from object x to the player */
u8 ss2_aim_dir(u8 x)
{
    u16 dx, dy, ox, oy, a2, a3, a5, b2, b3, b5;
    u8 y = 0;
    if (!(O(0x269A, x) & 0x20))
    {
        ox = (u16)(OX16(x) + 0x100 + (u8)(((O(0x253A, x) & 1) + 1) << 3));
        oy = (u16)(OY16(x) + 0x100 + (u8)(((O(0x253A, x) & 0x30) + 0x10) >> 1));
    }
    else
    {
        ox = (u16)(OX16(x) + 0x100);
        oy = (u16)(OY16(x) + 0x100);
    }
    dx = (u16)((u16)(PX16 + 0x110) - ox);
    if ((u16)(PX16 + 0x110) < ox) { dx = (u16)~dx; y = 8; }
    a2 = (u16)(dx << 1);
    a3 = (u16)(dx + a2);
    a5 = (u16)(a3 + a2);
    dy = (u16)((u16)(((P_YH << 8) | P_YL) + 0x110) - oy);
    if ((u16)(((P_YH << 8) | P_YL) + 0x110) < oy) { dy = (u16)~dy; y |= 0x10; }
    b2 = (u16)(dy << 1);
    b3 = (u16)(dy + b2);
    b5 = (u16)(b3 + b2);
    /* $86DE: compare dx:5dy, 2dx:3dy, 3dx:2dy, 5dx:dy (16-bit unsigned) */
    if (dx < b5)
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
    return ss2_bank_byte(3, (u16)(0x8712 + y));
}

/* bank 2 $7BEA: velocity from the direction $277A in table set $27FA */
void ss2_aim_velocity(u8 x)
{
    u16 t = (u16)(0x7C1B + O(0x27FA, x));
    u16 p = (u16)(ss2_bank_byte(2, t) | (ss2_bank_byte(2, (u16)(t + 1)) << 8));
    p = (u16)(p + (u8)(O(0x277A, x) << 2));
    O(0x25DA, x) = ss2_bank_byte(2, p);
    O(0x25BA, x) = ss2_bank_byte(2, (u16)(p + 1));
    O(0x261A, x) = ss2_bank_byte(2, (u16)(p + 2));
    O(0x25FA, x) = ss2_bank_byte(2, (u16)(p + 3));
}

/* bank 2 $6E1A, started in task slot 2 by the spawn ($E1E2): wait for an
 * empty object table, a second, and unpause; then the stage clear runs */
static void boss_watch_task(void)
{
    u8 x;
    do
    {
        ss2_wait(1);                    /* $B9B8 */
        for (x = 0; x < 0x20 && !O(0x245A, x); x++) { }
    } while (x < 0x20);
    ss2_wait(0x3C);                     /* $B9D8 */
    do ss2_wait(1); while (ZP(0x6B));
    ss2_stage_clear_start();            /* task 0 = $F617, then $E212 */
}

void ss2_boss_watch_start(void)
{
    ZP(0x10) = 0x1A;
    ZP(0x11) = 0x6E;
    /* the slot's resume bytes ($0A/$0B) are native in ss2_task.c */
    ss2_task_create(2, boss_watch_task);
}

/* ---- poses: script, body-box offset and size --------------------------- */

static void pose(u8 x, u16 left, u16 right, u16 bx_left, u16 bx_right, u16 by, u8 w, u8 h)
{
    u16 bx;
    if (!O(0x29BA, x)) { set_script(x, left); bx = bx_left; }
    else { set_script(x, right); bx = bx_right; }
    O(0x29BA, (u8)(x + 1)) = (u8)bx;
    O(0x299A, (u8)(x + 1)) = (u8)(bx >> 8);
    O(0x267A, x) = 1;
    O(0x29FA, (u8)(x + 1)) = (u8)by;
    O(0x29DA, (u8)(x + 1)) = (u8)(by >> 8);
    O(0x271A, (u8)(x + 1)) = w;
    O(0x273A, (u8)(x + 1)) = h;
}

static void pose_walk(u8 x)   { pose(x, 0xDBFE, 0xDC7D, 0x0008, 0xFFF8, 0xFFFC, 0x08, 0x10); }  /* $D922 */
static void pose_stand(u8 x)  { pose(x, 0xDD76, 0xDDB7, 0x0008, 0xFFF8, 0xFFFC, 0x08, 0x10); }  /* $D96B */
static void pose_fire(u8 x)   { pose(x, 0xDDF8, 0xDE39, 0x0008, 0xFFF8, 0xFFFC, 0x08, 0x10); }  /* $D9B4 */
static void pose_crouch(u8 x) { pose(x, 0xDCFC, 0xDD39, 0xFFD8, 0x0028, 0x0008, 0x10, 0x08); }  /* $D9FD */

/* ---- $D7BB: run both slots, the body box following the boss ------------ */

/* The source ends with INX / JMP $EE67, so the object loop ($ED18-$ED1B)
 * continues with X = x + 1 and never updates the body slot itself. The
 * engine's loop resumes after ss2_obj_next (ss2_objects_update). */

static void tail(u8 x)
{
    u8 b = (u8)(x + 1);
    ss2_obj_next = b;
    ss2_obj_common(x);
    O(0x245A, b) = O(0x245A, x);
    O(0x26DA, b) = 0xFF;
    O(0x285A, b) = 0;
    set_ox16(b, (u16)(OX16(x) + (u16)((O(0x299A, b) << 8) | O(0x29BA, b))));
    set_oy16(b, (u16)(OY16(x) + (u16)((O(0x29DA, b) << 8) | O(0x29FA, b))));
    ss2_obj_common(b);
}

/* ---- spawn ------------------------------------------------------------- */

static void boss_spawn(u8 v, u16 rec)
{
    u8 x = 0, b;
    ZP(0x26) = (u8)rec;
    ZP(0x27) = (u8)(rec >> 8);
    ZP(0x35) = v;
    if (!Z_INLOCK1) return;
    /* $D41B: two consecutive free slots */
    while (O(0x245A, x) | O(0x245A, (u8)(x + 1)))
        if (++x == 0x1F) return;
    b = (u8)(x + 1);
    O(0x2A9A, x) = v;
    if (!(v & 1)) ss2_boss_watch_start();
    if (!(v & 2)) { O(0x2A5A, x) = 0x82; O(0x2A7A, x) = 0xDB; }
    else { O(0x2A5A, x) = 0xA2; O(0x2A7A, x) = 0xDB; }
    ss2_obj_init(x, rec);
    ss2_obj_init(b, 0xDB56);
    O(0x247A, x) = ZP(0x40);
    O(0x249A, x) = ZP(0x41);
    O(0x24DA, x) = ZP(0x42);
    O(0x24FA, x) = ZP(0x43);
    set_vx(x, 0); set_vx(b, 0);
    set_vy(x, 0); set_vy(b, 0);
    O(0x287A, x) = 1;
    O(0x287A, b) = 0;
    O(0x281A, x) = ZP(0x44);
    O(0x281A, b) = 0xBF;
    O(0x2B1A, b) = 0;
    O(0x2B3A, b) = 0;
    O(0x29FA, x) = 0;
    O(0x297A, x) = 0;
    O(0x29BA, x) = 0;
    pose_stand(x);
    O(0x2B5A, ZP(0x44)) = 0xFF;
    tail(x);
}

/* ---- shots ------------------------------------------------------------- */

/* $DA44: fire an aimed shot from boss y (allocated from the top) */
static void fire(u8 y)
{
    u8 x = ss2_obj_alloc_top(), a;
    if (x == 0xFF) return;
    ss2_obj_init(x, 0xDB6C);
    O(0x287A, x) = 0;
    O(0x281A, x) = 0xBF;
    if (O(0x2A9A, y) & 1) O(0x26FA, x) = 5;
    set_oy16(x, (u16)(OY16(y) + 0xFFF4));
    if (!O(0x29BA, y))
    {
        set_ox16(x, (u16)(OX16(y) + 0xFFF8));
        a = ss2_aim_dir(x);
        if (a < 5) a = 5;
        else if (a >= 0x0A) a = 0x0A;
    }
    else
    {
        set_ox16(x, (u16)(OX16(y) + 8));
        a = ss2_aim_dir(x);
        ZP(0x30) = a;
        if (a >= 2 && a < 0x0D) a = a < 8 ? 2 : 0x0D;
    }
    O(0x277A, x) = a;
    ss2_aim_velocity(x);
    ss2_obj_common(x);
}

/* $DAE8/$DAED: shot flies until it meets a wall */
static void shot(u8 x)
{
    O(0x245A, x) = 0x80;
    O(0x263A, x) = 0;
    O(0x265A, x) = 0;
    if (ss2_obj_probe(x)) { ss2_obj_die(x); return; }
    ss2_obj_common(x);
}

/* $DADB */
static void shot_hit(u8 x)
{
    if (O(0x245A, x) == 0x40) { O(0x245A, x) = 0; return; }
    shot(x);
}

/* ---- AI ($D549) -------------------------------------------------------- */

/* side probe at the leading edge; a wall stops the walk */
static void side_wall(u8 x)
{
    O(0x263A, x) = (s8)O(0x25BA, x) < 0 ? 0xE8 : 0x17;
    O(0x265A, x) = 0x1F;
    if (ss2_obj_probe(x)) set_vx(x, 0);
}

static void stand(u8 x);

/* $D645 */
static void walking(u8 x)
{
    side_wall(x);
    if (!--O(0x29DA, x)) O(0x297A, x) = 0;
    tail(x);
}

/* $D616/$D622: start walking left/right */
static void walk(u8 x, u8 right)
{
    set_vx(x, right ? 0x0100 : 0xFF00);
    set_vy(x, 0);
    O(0x29BA, x) = O(0x299A, x);
    pose_walk(x);
    O(0x297A, x) = 1;
    O(0x29DA, x) = 0x20;
    walking(x);
}

/* $D6A4 */
static void jumping(u8 x)
{
    side_wall(x);
    if ((s8)O(0x25FA, x) >= 0)
    {
        O(0x263A, x) = 0;
        O(0x265A, x) = 0x20;
        if (ss2_obj_probe(x)) { ss2_obj_snap_y(x); stand(x); return; }
    }
    set_vy(x, (u16)(vy16(x) + 0x40));
    if (O(0x29BA, x) == O(0x299A, x) && !vy16(x))
    {
        /* $D6FB: at the top of the jump, maybe fire */
        ss2_random();
        if (!(O(0x2A9A, x) & 2))
        {
            if (ZP(0x8D) & 1) pose_crouch(x);
        }
        else
        {
            u8 a = (u8)(ZP(0x8D) & 3);
            if (a == 1) pose_crouch(x);
            else if (a) { pose_fire(x); fire(x); }
        }
    }
    tail(x);
}

/* $D66E */
static void jump(u8 x)
{
    O(0x29BA, x) = O(0x299A, x);
    set_vx(x, O(0x299A, x) ? 0x0180 : 0xFE80);
    set_vy(x, 0xFB00);
    O(0x29BA, x) = O(0x299A, x);
    pose_stand(x);
    O(0x297A, x) = 2;
    jumping(x);
}

/* $D745 */
static void crouching(u8 x)
{
    u8 t = --O(0x29DA, x);
    if (!t) O(0x297A, x) = 0;
    else if (t == 0x14) pose_stand(x);
    else if (t == 0x0C) pose_crouch(x);
    tail(x);
}

/* $D726 */
static void crouch(u8 x)
{
    set_vx(x, 0);
    set_vy(x, 0);
    O(0x29BA, x) = O(0x299A, x);
    pose_crouch(x);
    O(0x297A, x) = 3;
    O(0x29DA, x) = 0x20;
    crouching(x);
}

/* $D784/$D7B3: count the action timer down */
static void waiting(u8 x)
{
    if (!--O(0x29DA, x)) O(0x297A, x) = 0;
    tail(x);
}

/* $D765 */
static void stand(u8 x)
{
    set_vx(x, 0);
    set_vy(x, 0);
    O(0x29BA, x) = O(0x299A, x);
    pose_stand(x);
    O(0x297A, x) = 4;
    O(0x29DA, x) = 0x18;
    waiting(x);
}

/* $D78F */
static void shoot(u8 x)
{
    set_vx(x, 0);
    set_vy(x, 0);
    O(0x29BA, x) = O(0x299A, x);
    pose_fire(x);
    O(0x297A, x) = 5;
    O(0x29DA, x) = 0x18;
    fire(x);
    waiting(x);
}

static void boss_ai(u8 x)
{
    u16 a = (u16)(OX16(x) + 0x100), p = (u16)(PX16 + 0x110), d = (u16)(a - p);
    u8 y, r, k;
    O(0x299A, x) = 0;
    if (a < p) { O(0x299A, x) = 1; d = (u16)(0 - d); }
    ZP(0x26) = (u8)d; ZP(0x27) = (u8)(d >> 8);
    switch (O(0x297A, x))
    {
        case 0: break;
        case 1: walking(x); return;
        case 2: jumping(x); return;
        case 3: crouching(x); return;
        default: waiting(x); return;        /* 4 ($D784), 5 ($D7B3) */
    }
    /* $D58F: distance band, then the odds row */
    y = 3;
    if (!(d >> 8))
    {
        y = 0;
        if ((u8)d >= 0x20) y++;
        if ((u8)d >= 0x58) y++;
        if ((u8)d >= 0x89) y++;
    }
    y = (u8)(y << 3);
    ss2_random();
    {
        u16 t = (u16)((O(0x2A7A, x) << 8) | O(0x2A5A, x));
        ZP(0x26) = (u8)t; ZP(0x27) = (u8)(t >> 8);
        r = (u8)(ZP(0x8D) & 0x1F);
        for (k = 0; k < 5; k++)
        {
            u8 c = ss2_rom_byte((u16)(t + (u8)(y + k)));
            if (r < c) break;
            r = (u8)(r - c);
        }
    }
    switch (k)
    {
        case 0: stand(x); return;                           /* $D765 */
        case 1: walk(x, O(0x299A, x) != 0); return;         /* $D60A toward */
        case 2: walk(x, O(0x299A, x) == 0); return;         /* $D611 away */
        case 3: jump(x); return;                            /* $D66E */
        case 4: crouch(x); return;                          /* $D726 */
        default: shoot(x); return;                          /* $D78F */
    }
}

/* $D539: the fight starts */
static void boss_start(u8 x)
{
    Z_INLOCK1 = 0;
    ZP(0x64) = 1;
    set_handler(x, 0xD549);
    boss_ai(x);
}

/* $D521: variant 2 waits for the player to be on the cloud */
static void boss_wait_cloud(u8 x)
{
    if (P_ONCLOUD) { boss_start(x); return; }
    tail(x);
}

/* $D4C2: variants 0/2 walk in, then the room is rebuilt */
static void boss_enter(u8 x)
{
    if (OX16(x) >= 0xD0) { tail(x); return; }
    ZP(0x8F) = 6;
    ZP(0x91) = 0x0A;
    if (!(O(0x2A9A, x) & 2)) { ZP(0x93) = 0xD6; ZP(0x94) = 0x56; }
    else { ZP(0x93) = 0x8C; ZP(0x94) = 0x57; }
    ss2_message();                          /* bank 2 $6984 */
    if ((O(0x2A9A, x) & 2) && (P_FLAGSC1 & 0x08))
    {
        P_FLAGSC1 &= (u8)~0x08;
        P_CLOUDREQ = 0x08;
        ss2_hud_text(0x4EA1);               /* bank 4 $BADE */
        set_handler(x, 0xD521);
        boss_wait_cloud(x);
        return;
    }
    boss_start(x);
}

/* $D529: variants 1/3 */
static void boss_enter_plain(u8 x)
{
    if (OX16(x) >= 0xD0) { tail(x); return; }
    boss_start(x);
}

/* ---- hits -------------------------------------------------------------- */

/* $D83E: knocked away from the player, sliding to a stop */
static void knockback(u8 x)
{
    if (!O(0x29FA, x))
    {
        O(0x29FA, x)++;
        if (OX16(x) >= PX16) { O(0x29BA, x) = 0; set_vx(x, 0x0380); }
        else { O(0x29BA, x) = 1; set_vx(x, 0xFC80); }
        set_vy(x, 0);
        pose_crouch(x);
    }
    if (!(vx16(x) | vy16(x)))
    {
        O(0x245A, x) = 0x80;
        O(0x29FA, x) = 0;
        O(0x297A, x) = 0;
        tail(x);
        return;
    }
    if (vx16(x))
    {
        O(0x263A, x) = (s8)O(0x25BA, x) < 0 ? 0xE8 : 0x17;
        O(0x265A, x) = 0x1F;
        if (ss2_obj_probe(x)) set_vx(x, 0);
        else if (!vy16(x)) set_vx(x, (u16)(vx16(x) + ((s8)O(0x25BA, x) < 0 ? 0x0040 : 0xFFC0)));
    }
    /* $D8F1 */
    if ((s8)O(0x25FA, x) >= 0)
    {
        O(0x263A, x) = 0;
        O(0x265A, x) = 0x20;
        if (ss2_obj_probe(x))
        {
            set_vy(x, 0);
            ss2_obj_snap_y(x);
            tail(x);
            return;
        }
    }
    set_vy(x, (u16)(vy16(x) + 0x40));
    tail(x);
}

/* $D7F6/$D808/$D81A/$D82C: death runs the bank 2 boss explosion */
static void boss_hit(u8 x, u16 pal)
{
    if (O(0x245A, x) == 0x40)
    {
        ZP(0x26) = (u8)pal;
        ZP(0x27) = (u8)(pal >> 8);
        ss2_unported(0x6F40);
        return;
    }
    knockback(x);
}

u8 ss2_obj1f_handler(u16 addr, u8 x)
{
    switch (addr)
    {
        case 0xD4C2: boss_enter(x); return 1;
        case 0xD521: boss_wait_cloud(x); return 1;
        case 0xD529: boss_enter_plain(x); return 1;
        case 0xD549: boss_ai(x); return 1;
        case 0xD7F6: boss_hit(x, 0xDBC2); return 1;
        case 0xD808: boss_hit(x, 0xDBD0); return 1;
        case 0xD81A: boss_hit(x, 0xDBDE); return 1;
        case 0xD82C: boss_hit(x, 0xDBEC); return 1;
        case 0xDADB: shot_hit(x); return 1;
        case 0xDAED: shot(x); return 1;
    }
    return 0;
}

u8 ss2_spawn1f_handler(u16 addr)
{
    switch (addr)
    {
        case 0xD3E0: boss_spawn(0, 0xDAFE); return 1;
        case 0xD3EE: boss_spawn(1, 0xDB14); return 1;
        case 0xD3FC: boss_spawn(2, 0xDB2A); return 1;
        case 0xD40A: boss_spawn(3, 0xDB40); return 1;
    }
    return 0;
}

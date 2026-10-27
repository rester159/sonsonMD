/* SON SON II object engine, rebuilt from HuCard bank 0 $EE01-$F5A9, bank 3
 * $8327-$85E6 and bank 4 $BB89/$BC10.
 *
 * 32 object slots kept as the source's struct-of-arrays (stride $20):
 *   $245A state ($80 live, $40 dying, $20 hit reaction)
 *   $247A:$249A:$24BA x (screen, 16.8)   $24DA:$24FA:$251A y
 *   $253A attributes   $255A ?   $257A:$259A pattern pointer
 *   $25BA:$25DA vx   $25FA:$261A vy   $263A/$265A probe offsets
 *   $267A script timer   $269A flags   $26BA item kind   $26DA hit points
 *   $26FA contact damage   $271A/$273A body box   $275A code bank
 *   $279A score id   $27BA:$27DA animation base   $281A spawner
 *   $283A flash timer   $285A invulnerability   $289A ?
 *   $28BA:$28DA handler   $28FA:$291A hit handler   $293A:$295A script
 *   $2ADA/$2AFA weak-box offset   $2B1A/$2B3A weak box
 * Per-spawner tables: $2B5A live count, $2C1A respawn timer, $2CDA ?.
 * Objects live in screen coordinates and are shifted by the camera delta.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"

/* field a of slot x through o = &ss2_ram[x] (a fixed displacement) */
#define OF(a) (o[(u16)((a) - 0x2000)])

#define OX16(x)     ((u16)((O(0x247A, x) << 8) | O(0x249A, x)))
#define OY16(x)     ((u16)((O(0x24DA, x) << 8) | O(0x24FA, x)))
#define PX16        ((u16)((P_XH << 8) | P_XL))
#define PY16        ((u16)((P_YH << 8) | P_YL))

u8 ss2_mpr6 = 9;

/* bank 4 $BB83: map a bank at $C000, remembering it in $8E */
void ss2_map6(u8 bank)
{
    ZP(0x8E) = bank;
    ss2_mpr6 = bank;
    ss2_page[6] = ss2_banks[bank & 0x1F];
    if (ss2_page[6] == ss2_banks[0x0A]) ss2_unported((u16)(0xB000 | bank));
}

static void kill_spawned(u8 x);

/* ---- small helpers ---------------------------------------------------- */

static void set_ox16(u8 x, u16 v) { O(0x247A, x) = (u8)(v >> 8); O(0x249A, x) = (u8)v; }
static void set_oy16(u8 x, u16 v) { O(0x24DA, x) = (u8)(v >> 8); O(0x24FA, x) = (u8)v; }

/* $FDB4: seed = seed*3 with the high byte folded with the old low byte.
 * Returns the carry of that last addition, which the source leaves in C. */
u8 ss2_random(void)
{
    u16 s = (u16)((ZP(0x8D) << 8) | ZP(0x8C));
    u16 t = (u16)(s * 3), h;
    ZP(0x26) = (u8)t;
    ZP(0x27) = (u8)(t >> 8);
    h = (u16)(ZP(0x8C) + (t >> 8));
    ZP(0x8D) = (u8)h;
    ZP(0x8C) = (u8)t;
    return (u8)(h >> 8);
}

/* bank 4 $BB89: queue a score award id */
void ss2_score_award(u8 id)
{
    u8 i = R(0x2284);
    R(0x2263 + i) = id;
    R(0x2284) = (u8)((i + 1) & 0x1F);
}

/* ss2_probe_point() (ss2_logic.h): map cell offsets for wy>>5, wx>>5 */
const u16 ss2_probe_row[64] __attribute__((used, externally_visible)) = {
    0x000, 0x008, 0x010, 0x018, 0x020, 0x028, 0x030, 0x038,
    0x200, 0x208, 0x210, 0x218, 0x220, 0x228, 0x230, 0x238,
    0x400, 0x408, 0x410, 0x418, 0x420, 0x428, 0x430, 0x438,
    0x600, 0x608, 0x610, 0x618, 0x620, 0x628, 0x630, 0x638,
    0x800, 0x808, 0x810, 0x818, 0x820, 0x828, 0x830, 0x838,
    0xA00, 0xA08, 0xA10, 0xA18, 0xA20, 0xA28, 0xA30, 0xA38,
    0xC00, 0xC08, 0xC10, 0xC18, 0xC20, 0xC28, 0xC30, 0xC38,
    0xE00, 0xE08, 0xE10, 0xE18, 0xE20, 0xE28, 0xE30, 0xE38,
};
const u16 ss2_probe_col[64] __attribute__((used, externally_visible)) = {
    0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007,
    0x040, 0x041, 0x042, 0x043, 0x044, 0x045, 0x046, 0x047,
    0x080, 0x081, 0x082, 0x083, 0x084, 0x085, 0x086, 0x087,
    0x0C0, 0x0C1, 0x0C2, 0x0C3, 0x0C4, 0x0C5, 0x0C6, 0x0C7,
    0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107,
    0x140, 0x141, 0x142, 0x143, 0x144, 0x145, 0x146, 0x147,
    0x180, 0x181, 0x182, 0x183, 0x184, 0x185, 0x186, 0x187,
    0x1C0, 0x1C1, 0x1C2, 0x1C3, 0x1C4, 0x1C5, 0x1C6, 0x1C7,
};

/* bank 3 $858B: drop the object onto the 16 px grid below its probe */
void ss2_obj_snap_y(u8 x)
{
    u8 t = (u8)(O(0x24FA, x) + O(0x265A, x));
    t = (u8)((Z_CAMY_LO + t) & 0x0F);
    ZP(0x30) = t;
    set_oy16(x, (u16)(OY16(x) - t));
}

/* bank 3 $8505: shift by the camera movement since the previous frame
 * (the delta is the same for every slot, so it is computed once a frame) */
static s16 cam_dx, cam_dy;

/* the camera's move since the previous frame ($4C/$4F - $46/$49), which
 * $EE01 subtracts from every object: hoisted per frame (also for the
 * title/intro scripts that call $EE01 directly) */
void ss2_obj_camera(void)
{
    cam_dx = (s8)(u8)(ZP(0x4C) - Z_CAMX_LO);
    cam_dy = (s8)(u8)(ZP(0x4F) - Z_CAMY_LO);
}
static u8 shots_live;
static void camera_shift(u8 x)
{
    u8 *const o = &ss2_ram[x];
    u16 v = (u16)(((OF(0x247A) << 8) | OF(0x249A)) + cam_dx);
    OF(0x247A) = (u8)(v >> 8); OF(0x249A) = (u8)v;
    v = (u16)(((OF(0x24DA) << 8) | OF(0x24FA)) + cam_dy);
    OF(0x24DA) = (u8)(v >> 8); OF(0x24FA) = (u8)v;
}

/* bank 3 $8428/$8436: free slot searched upward from 0 / downward from 31
 * (slot 0 excluded). Returns 0xFF when full; the source then returns from
 * its caller. */
u8 ss2_obj_alloc(void)
{
    u8 x;
    for (x = 0; x < 0x20; x++) if (!O(0x245A, x)) return x;
    return 0xFF;
}

u8 ss2_obj_alloc_top(void)
{
    u8 x;
    for (x = 0x1F; x; x--) if (!O(0x245A, x)) return x;
    return 0xFF;
}

/* bank 3 $8451: initialise slot x from a 22-byte record in the object's bank */
void ss2_obj_init(u8 x, u16 rec)
{
    u8 *const o = &ss2_ram[x];
    static const u16 dst[22] = {
        0x245A, 0x267A, 0x269A, 0x26BA, 0x26DA, 0x26FA, 0x271A, 0x273A, 0x275A, 0x279A, 0x27FA,
        0x289A, 0x255A, 0x253A, 0x27DA, 0x27BA, 0x28DA, 0x28BA, 0x291A, 0x28FA, 0x295A, 0x293A };
    u8 i;
    for (i = 0; i < 22; i++)
    {
        u8 v = ss2_rom_byte((u16)(rec + i));
        O(dst[i], x) = v;
        if (i == 6) OF(0x2B1A) = v;
        if (i == 7) OF(0x2B3A) = v;
    }
    OF(0x283A) = 0;
    if (!(OF(0x269A) & 0x20))
    {
        OF(0x2ADA) = (u8)(((OF(0x253A) & 1) + 1) << 3);
        OF(0x2AFA) = (u8)(((OF(0x253A) & 0x30) + 0x10) >> 1);
    }
    else
    {
        OF(0x2ADA) = 0;
        OF(0x2AFA) = 0;
    }
}

/* bank 3 $85B1: refuse spawns that would appear on screen (unless $88) */
u8 ss2_spawn_visible(void)
{
    u16 sx = (u16)(((ZP(0x40) << 8) | ZP(0x41)) + 0x20);
    u16 sy = (u16)(((ZP(0x42) << 8) | ZP(0x43)) - 0x18);
    if (ZP(0x88)) return 0;
    ZP(0x2E) = (u8)sx; ZP(0x2F) = (u8)(sx >> 8);
    if (sx >= 0x140) return 0;
    return sy < 0x108;
}

/* ---- collision ($EFBD-$F5A9) ------------------------------------------ */

/* |a - b| < size with the source's 16-bit compare; sets the side word */
static u8 near(u16 a, u16 b, u8 size, u8 side)
{
    u16 d = (u16)(a - b);
    if (a < b)
    {
        if (side) { ZP(0x26) = 0xF8; ZP(0x27) = 0xFF; }
        d = (u16)(0 - d);
    }
    return d < size;
}

static void hurt_object(u8 x)        /* $F584 */
{
    O(0x283A, x) = 0x0A;
    O(0x285A, x) = 0x10;
    if (!ZP(0x71)) O(0x245A, x) = 0x20;
}

static void react(u8 x)              /* $F58E */
{
    if (!ZP(0x71)) O(0x245A, x) = 0x20;
}

void ss2_obj_die(u8 x)               /* $F59F */
{
    O(0x283A, x) = 0x0A;
    O(0x245A, x) = 0x40;
}

static void damage(u8 x, u8 amount)  /* $F188/$F33D */
{
    u8 hp = O(0x26DA, x);
    O(0x26DA, x) = (u8)(hp - amount);
    if (hp > amount) { hurt_object(x); return; }
    ss2_score_award(O(0x279A, x));   /* $F599 */
    ss2_obj_die(x);
}

static void spark(u8 y)              /* $F12E/$F209: staff impact point */
{
    u16 v;
    R(0x2432) = 4;
    R(0x243C) = 1;
    R(0x2455) = 8;
    v = (u16)(PX16 + (u16)((R(0x23E2 + y) << 8) | R(0x23E7 + y)));
    v = (u16)(v + (u16)((ZP(0x27) << 8) | ZP(0x26)));
    R(0x2457) = (u8)v; R(0x2456) = (u8)(v >> 8);
    v = (u16)(PY16 + (P_CLIMBSTEP ? 0x0F : 0x06));
    R(0x2459) = (u8)v; R(0x2458) = (u8)(v >> 8);
}

static void spend_magic(u8 x)        /* $F324: shots 4-6 cost magic */
{
    u8 m = P_MAGIC, d = O(0x26FA, x);
    P_MAGIC = (u8)(m - d);
    R(0x22BD) = P_MAGIC;
    if (m < d) { P_MAGIC = 0; R(0x22BD) = 0; }
}

static u8 collide(u8 x)
{
    u8 *const o = &ss2_ram[x];
    u8 y;
    u16 wx, wy, bx, by;
    if (P_MODE == 0x40) return 0;
    {
        /* the slot fields read once: each zero-page byte store below would
         * otherwise make the compiler reload them (char aliasing) */
        const u16 ox = (u16)((OF(0x247A) << 8) | OF(0x249A));
        const u16 oy = (u16)((OF(0x24DA) << 8) | OF(0x24FA));
        const s8 ax = (s8)OF(0x2ADA), ay = (s8)OF(0x2AFA);
        if (!(OF(0x269A) & 0x20))
        {
            const u8 sz = OF(0x253A);
            bx = (u16)(ox + (u8)(((sz & 1) + 1) << 3));
            by = (u16)(oy + (u8)(((sz & 0x30) + 0x10) >> 1));
        }
        else { bx = ox; by = oy; }
        wx = (u16)(ox + ax);
        wy = (u16)(oy + ay);
    }
    ZP(0xBB) = (u8)(bx >> 8); ZP(0xBC) = (u8)bx;
    ZP(0xBD) = (u8)(by >> 8); ZP(0xBE) = (u8)by;
    ZP(0xBF) = (u8)(wx >> 8); ZP(0xC0) = (u8)wx;
    ZP(0xC1) = (u8)(wy >> 8); ZP(0xC2) = (u8)wy;

    if (OF(0x245A) == 0x20 || OF(0x285A)) goto body;
    if (OF(0x269A) & 0x02) goto shots;
    if (R(0x2432) == 4) goto shots;
    /* $F063: staff segments; nothing here writes RAM before a hit but
     * $26/$27, so the player and slot fields hold for the whole loop */
    if (R(0x23DD) == 0x80)
    {
        const u16 px = PX16, sy = (u16)(PY16 + (P_CLIMBSTEP ? 0x17 : 0x0E));
        const u8 wb = (u8)(OF(0x2B1A) | OF(0x2B3A));
        const u8 wsx = (u8)(OF(0x2B1A) + 8), wsy = (u8)(OF(0x2B3A) + 2);
        const u8 bsx = (u8)(OF(0x271A) + 8), bsy = (u8)(OF(0x273A) + 2);
        const u8 n = P_STAFFMAX;
        for (y = 0; ; )
        {
            u16 sx;
            if (R(0x23DD + y) != 0x80) break;
            sx = (u16)(px + (u16)((R(0x23E2 + y) << 8) | R(0x23E7 + y)) + 8);
            if (wb)
            {
                ZP(0x26) = 8; ZP(0x27) = 0;
                if (near(wx, sx, wsx, 1) && near(wy, sy, wsy, 0))
                {
                    spark(y);
                    damage(x, R(0x22B4));
                    return 0;
                }
            }
            ZP(0x26) = 8; ZP(0x27) = 0;
            if (near(bx, sx, bsx, 1) && near(by, sy, bsy, 0))
            {
                spark(y);
                react(x);
                return 0;
            }
            if (++y == n) break;
        }
    }
shots:
    /* $F26F: player shots */
    if (shots_live && !(OF(0x269A) & 0x01))
    {
        for (y = 0; y < 7; y++)
        {
            u16 sx, sy;
            u8 mx, my;
            if (R(0x22E5 + y) != 0x80) continue;
            sx = (u16)(((R(0x22EC + y) << 8) | R(0x22F3 + y)) + 8);
            sy = (u16)(((R(0x2301 + y) << 8) | R(0x2308 + y)) + 8);
            mx = R(0x2371 + y); my = R(0x2378 + y);
            if ((OF(0x2B1A) | OF(0x2B3A)) &&
                near(wx, sx, (u8)(OF(0x2B1A) + mx), 0) && near(wy, sy, (u8)(OF(0x2B3A) + my), 0))
            {
                R(0x22E5 + y) = 0x40;
                R(0x2386 + y) = R(0x2394 + y);
                R(0x237F + y) = R(0x238D + y);
                if (y >= 4) spend_magic(x);
                damage(x, R(0x236A + y));
                return 0;
            }
            if (near(bx, sx, (u8)(OF(0x271A) + mx), 0) && near(by, sy, (u8)(OF(0x273A) + my), 0))
            {
                R(0x22E5 + y) = 0x40;
                R(0x2386 + y) = R(0x2394 + y);
                R(0x237F + y) = R(0x238D + y);
                if (y >= 4) spend_magic(x);
                react(x);
                return 0;
            }
        }
    }
body:
    /* $F3E4: touching the player */
    if (OF(0x269A) & 0x04) return 0;
    if (!OF(0x26BA) && (P_MODE != 0x80 || P_C4)) return 0;
    {
        u16 sx = (u16)(PX16 + 0x10), sy;
        u8 my;
        if (!P_CLIMBSTEP) { sy = (u16)(PY16 + 0x10); my = 0x0E; }
        else { sy = (u16)(PY16 + 0x14); my = 0x0C; }
        if (!((OF(0x2B1A) | OF(0x2B3A)) &&
              near(wx, sx, (u8)(OF(0x2B1A) + 8), 0) && near(wy, sy, (u8)(OF(0x2B3A) + my), 0)))
        {
            if (!near(bx, sx, (u8)(OF(0x271A) + 8), 0)) return 0;
            if (!near(by, sy, (u8)(OF(0x273A) + my), 0)) return 0;
        }
    }
    /* $F506 */
    if (OF(0x26BA)) return ss2_item_pickup(x);
    {
        u8 d = OF(0x26FA);
        if (P_FLAGSC1 & 0x80) d = (u8)(((u8)(d - 1) >> 1) + 1);
        ZP(0x30) = d;
        if ((u8)(R(0x22E9) | R(0x22EA) | R(0x22EB)) == 0x80)
        {
            u8 m = P_MAGIC;
            P_MAGIC = (u8)(m - d);
            R(0x22BD) = P_MAGIC;
            if (m <= d) { P_MAGIC = 0; R(0x22BD) = 0; }
            P_MODE = 0x20;
        }
        else if (!ZP(0x65))
        {
            u8 l = P_LIFE;
            P_LIFE = (u8)(l - d);
            R(0x22BA) = P_LIFE;
            if (l <= d) { P_LIFE = 0; R(0x22BA) = 0; P_MODE = 0x40; }
            else P_MODE = 0x20;
        }
        else P_MODE = 0x20;
    }
    P_HITOBJ = (u8)(x + 1);
    P_HURT = 0x5A;
    if (OF(0x269A) & 0x10) ss2_obj_die(x);
    return 0;
}

/* ---- script and motion ($EE67-$EFBA) ---------------------------------- */

static u8 rd6(u16 a) { return ss2_rom_byte(a); }

void ss2_obj_common(u8 x)
{
    u8 *const o = &ss2_ram[x];
    if (ZP(0x87) && !OF(0x26BA) && (OF(0x269A) & 0x20)) return;
    if (!--OF(0x267A))
    {
        /* $EE82: next script command */
        u16 p = (u16)((OF(0x293A) << 8) | OF(0x295A));
        u8 c = rd6(p), b;
        ZP(0x3A) = (u8)p; ZP(0x3B) = (u8)(p >> 8);
        if (c == 0xFF) { OF(0x245A) = 0; return; }
        if (!c)
        {
            OF(0x295A) = rd6((u16)(p + 1));
            OF(0x293A) = rd6((u16)(p + 2));
            p = (u16)((OF(0x293A) << 8) | OF(0x295A));
            ZP(0x3A) = (u8)p; ZP(0x3B) = (u8)(p >> 8);
        }
        OF(0x267A) = rd6(p);
        b = rd6((u16)(p + 1));
        if (!(OF(0x269A) & 0x20))
        {
            u16 an = (u16)((OF(0x27BA) << 8) | OF(0x27DA));
            ZP(0x28) = (u8)an; ZP(0x29) = (u8)(an >> 8);
            if (b == 0xFF)
            {
                OF(0x259A) = 0x90;
                OF(0x257A) = 0;
            }
            else
            {
                u16 t = (u16)((b & 0x3F) + rd6((u16)(an + 1)));
                OF(0x259A) = (u8)t;
                OF(0x257A) = (u8)(rd6(an) + (t >> 8));
            }
            ZP(0x30) = (u8)(OF(0x253A) & 0x77);
            ZP(0x30) |= (u8)(b & 0x80);
            OF(0x253A) = (u8)(ZP(0x30) | ((b & 0x40) >> 3));
            p = (u16)(p + 2);
        }
        else
        {
            OF(0x259A) = OF(0x295A);
            OF(0x257A) = OF(0x293A);
            p = (u16)(p + (u8)((u8)(b << 2) + 2));
        }
        OF(0x295A) = (u8)p;
        OF(0x293A) = (u8)(p >> 8);
    }
    /* $EF38: motion */
    if (OF(0x245A) == 0x40) return;
#ifdef SS2_HOST
    {
        u16 t;
        u8 h = OF(0x25BA);
        t = (u16)(OF(0x24BA) + OF(0x25DA));
        OF(0x24BA) = (u8)t;
        t = (u16)(OF(0x249A) + h + (t >> 8));
        OF(0x249A) = (u8)t;
        OF(0x247A) = (u8)(OF(0x247A) + ((h & 0x80) ? 0xFF : 0) + (t >> 8));
        h = OF(0x25FA);
        t = (u16)(OF(0x251A) + OF(0x261A));
        OF(0x251A) = (u8)t;
        t = (u16)(OF(0x24FA) + h + (t >> 8));
        OF(0x24FA) = (u8)t;
        OF(0x24DA) = (u8)(OF(0x24DA) + ((h & 0x80) ? 0xFF : 0) + (t >> 8));
        ZP(0x30) = (h & 0x80) ? 0xFF : 0;
    }
#else
    ss2_obj_motion_native(o);
#endif
    if (OF(0x269A) & 0x08)
    {
        u16 t = (u16)(((u16)((OF(0x247A) << 8) | OF(0x249A))) + 0x60);
        if (t >= 0x1A0 || ((u16)((OF(0x24DA) << 8) | OF(0x24FA))) >= 0x130) { kill_spawned(x); return; }
    }
    if (collide(x)) ss2_obj_common(x);     /* item handler: PLA PLA PLX JMP $EE67 */
}

/* $EE95: leave the world, releasing the spawner's count */
static void kill_spawned(u8 x)
{
    O(0x2B5A, O(0x281A, x))--;
    O(0x245A, x) = 0;
}

/* ---- dispatch ($EE01) ------------------------------------------------- */

static void run_handler(u8 x, u16 h)
{
    if (h == 0xEE67) { ss2_obj_common(x); return; }
    if (ss2_obj_handler(ss2_mpr6, h, x)) return;
    ss2_unported(h);
}

void ss2_obj_update(u8 x)
{
    u8 *const o = &ss2_ram[x];
    camera_shift(x);
    if (OF(0x283A)) OF(0x283A)--;
    if (OF(0x285A)) OF(0x285A)--;
    ss2_map6(OF(0x275A));
    if (OF(0x245A) == 0x40)
    {
        if (OF(0x283A)) return;
        goto hit;
    }
    if (!OF(0x26BA))
    {
        if (ZP(0x87)) return;
        if (ZP(0x71)) { if (collide(x)) ss2_obj_common(x); return; }
        if (!(OF(0x269A) & 0x20) && Z_INLOCK1) return;
    }
    if (OF(0x245A) != 0x20)
    {
        run_handler(x, (u16)((OF(0x28BA) << 8) | OF(0x28DA)));
        return;
    }
hit:
    run_handler(x, (u16)((OF(0x28FA) << 8) | OF(0x291A)));
}

/* bank 0 $ED0D: update every live slot */
u8 ss2_obj_next;

void ss2_objects_update(void)
{
    u8 x;
    P_PUSHOBJ = 0;
    ss2_obj_camera();
    shots_live = 0;
    for (x = 0; x < 7; x++) if (R(0x22E5 + x) == 0x80) shots_live = 1;
    for (x = 0; x < 0x20; x++)
        if (O(0x245A, x))
        {
            /* a handler that ends INX / JMP $EE67 moves the loop on (bank $1F
             * boss body slot) */
            ss2_obj_next = x;
#ifdef SS2_TPROF
            { extern u16 ss2_tc_now[4]; ss2_tc_now[3]++; }
#endif
            ss2_obj_update(x);
            x = ss2_obj_next;
        }
}

/* ---- spawning (bank 4 $BC10) ------------------------------------------ */

/* Scan the spawn lists of three screens around the camera, one screen row
 * per frame (frames where ($12 & 3) == 3 skip). Records are 6 bytes:
 * x, y, spawner, code bank, handler lo/hi; $FF ends a list. */
void ss2_spawn_scan(void)
{
    u8 row = (u8)(Z_FRAME & 3), saved = ZP(0x8E);
    u16 cx;
    const u8 *const b8 = ss2_banks[8];      /* the spawn lists' bank (fixed) */
    /* The record pointer ($3C/$3D), spawner id ($44) and $26/$27 are
     * rewritten for every record; only their values at a spawn handler call
     * and at the end can be seen, so they are kept here and stored there
     * (d44/d26: changed since the last store) */
    u16 zrec = 0, z26 = 0;
    u8 z44 = 0, d44 = 0, d26 = 0;
#define B8(a) (b8[(u16)(a) & 0x1FFF])
    if (row == 3) return;
    ZP(0xE3) = row;
    ZP(0xE2) = 0;
    cx = (u16)(((Z_CAMX_HI << 8) | Z_CAMX_LO) - 0x60);
    ZP(0xE5) = (u8)cx;
    ZP(0xE4) = (u8)(cx >> 8);
    for (;;)
    {
        u16 list, rec;
        u8 sx = (u8)(ZP(0xE4) + ZP(0xE2)), sy = (u8)(Z_CAMY_HI + ZP(0xE3));
        ZP(0x3E) = sx;
        ZP(0x3F) = sy;
        ZP(0x37) = (u8)((sx & 7) | ((sy & 7) << 3));
        ss2_map6(8);
        list = (u16)((ss2_rom_byte((u16)(0xBD09 + (u8)(Z_STAGE << 1))) << 8) | ss2_rom_byte((u16)(0xBD08 + (u8)(Z_STAGE << 1))));
        rec = (u16)((B8((u16)(list + (u8)(ZP(0x37) << 1) + 1)) << 8) |
                    B8((u16)(list + (u8)(ZP(0x37) << 1))));
        z26 = list; d26 = 1;
        for (;;)
        {
            u8 id;
            u16 d;
            if (B8(rec) == 0xFF) break;
            id = B8((u16)(rec + 2));
            z44 = id; d44 = 1;
            if ((id < 0x58 || !ZP(0x70)) && O(0x2B5A, id) != 0xFF)
            {
                d = (u16)(((ZP(0x3E) << 8) | B8(rec)) - ((ZP(0xE4) << 8) | ZP(0xE5)));
                z26 = d;
                if (d < 0x1A0)
                {
                    d = (u16)(((ZP(0x3F) << 8) | B8((u16)(rec + 1))) - ((Z_CAMY_HI << 8) | Z_CAMY_LO));
                    z26 = d;
                    if (d < 0x130)
                    {
                        /* $BCD7 */
                        u16 p;
                        p = (u16)(((ZP(0x3E) << 8) | B8(rec)) - ((ZP(0x4B) << 8) | ZP(0x4C)));
                        ZP(0x40) = (u8)(p >> 8); ZP(0x41) = (u8)p;
                        p = (u16)(((ZP(0x3F) << 8) | B8((u16)(rec + 1))) - ((ZP(0x4E) << 8) | ZP(0x4F)));
                        ZP(0x42) = (u8)(p >> 8); ZP(0x43) = (u8)p;
                        p = (u16)((B8((u16)(rec + 5)) << 8) | B8((u16)(rec + 4)));
                        ZP(0x3C) = (u8)rec; ZP(0x3D) = (u8)(rec >> 8);
                        ZP(0x44) = id;
                        ZP(0x26) = (u8)p; ZP(0x27) = (u8)(p >> 8);
                        d44 = d26 = 0;
                        ss2_map6(B8((u16)(rec + 3)));
                        if (!ss2_spawn_handler(ss2_mpr6, p)) ss2_unported(p);
                        ss2_map6(8);        /* (bank 8 is still mapped otherwise) */
                    }
                }
            }
            rec = (u16)(rec + 6);
        }
        zrec = rec;                         /* the list's $FF end */
        if (++ZP(0xE2) == 3) break;
    }
    ZP(0x3C) = (u8)zrec; ZP(0x3D) = (u8)(zrec >> 8);
    if (d44) ZP(0x44) = z44;
    if (d26) { ZP(0x26) = (u8)z26; ZP(0x27) = (u8)(z26 >> 8); }
    ss2_map6(saved);
#undef B8
}

/* ---- handler tables ---------------------------------------------------- */

/* per-bank object code: src/ss2_objNN.c (stubs return 0 until ported) */
u8 ss2_obj05_handler(u16 addr, u8 x);   u8 ss2_spawn05_handler(u16 addr);
u8 ss2_obj07_handler(u16 addr, u8 x);   u8 ss2_spawn07_handler(u16 addr);
u8 ss2_obj09_handler(u16 addr, u8 x);   u8 ss2_spawn09_handler(u16 addr);
u8 ss2_obj0f_handler(u16 addr, u8 x);   u8 ss2_spawn0f_handler(u16 addr);
u8 ss2_obj17_handler(u16 addr, u8 x);   u8 ss2_spawn17_handler(u16 addr);
u8 ss2_obj1f_handler(u16 addr, u8 x);   u8 ss2_spawn1f_handler(u16 addr);
/* fixed-bank object code outside the item system: bank 3 ($8000-$9FFF)
 * in ss2_objfix.c, bank 4 ($A000-$BFFF) in ss2_objfix4.c */
u8 ss2_objfix_handler(u16 addr, u8 x);  u8 ss2_spawnfix_handler(u16 addr);
u8 ss2_objfix4_handler(u16 addr, u8 x); u8 ss2_spawnfix4_handler(u16 addr);

/* Handlers below $C000 live in the fixed banks; $C000-$DFFF is the
 * object's own bank (MPR6). */
u8 ss2_item_handler(u16 addr, u8 x);

void ss2_o17_boss_sparks(u8 x);             /* bank 2 $6F6D (ss2_obj17.c) */

u8 ss2_obj_handler(u8 bank, u16 addr, u8 x)
{
    if (addr == 0x6F6D) { ss2_o17_boss_sparks(x); return 1; }   /* boss death hit */
    if (addr >= 0xA000 && addr < 0xC000) return ss2_objfix4_handler(addr, x);
    if (addr < 0xA000) return ss2_item_handler(addr, x) || ss2_objfix_handler(addr, x);
    if (addr < 0xE000)
        switch (bank)
        {
            case 0x05: return ss2_obj05_handler(addr, x);
            case 0x07: return ss2_obj07_handler(addr, x);
            case 0x09: return ss2_obj09_handler(addr, x);
            case 0x0F: return ss2_obj0f_handler(addr, x);
            case 0x17: return ss2_obj17_handler(addr, x);
            case 0x1F: return ss2_obj1f_handler(addr, x);
        }
    return 0;
}

u8 ss2_item_spawn_handler(u16 addr);

u8 ss2_spawn_handler(u8 bank, u16 addr)
{
    if (addr >= 0xA000 && addr < 0xC000) return ss2_spawnfix4_handler(addr);
    if (addr < 0xA000) return ss2_item_spawn_handler(addr) || ss2_spawnfix_handler(addr);
    if (addr < 0xE000)
        switch (bank)
        {
            case 0x05: return ss2_spawn05_handler(addr);
            case 0x07: return ss2_spawn07_handler(addr);
            case 0x09: return ss2_spawn09_handler(addr);
            case 0x0F: return ss2_spawn0f_handler(addr);
            case 0x17: return ss2_spawn17_handler(addr);
            case 0x1F: return ss2_spawn1f_handler(addr);
        }
    return 0;
}


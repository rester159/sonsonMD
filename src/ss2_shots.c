/* SON SON II player shots and magic, rebuilt from HuCard bank 9 $D03D-$D9FA
 (the pause selection $FE0C/$FEAE and the box $FE5A are bank 0, in
 * src/ss2_game.c).
 *
 * Seven shot slots, struct-of-arrays indexed by slot x:
 *   $22E5 state ($80 live, $40 hit: run the hit handler, 0 free)
 *   $22EC:$22F3:$22FA x (hi, lo, frac)  $2301:$2308:$230F y
 *   $2332:$2339 vx (8.8)  $2340:$2347 vy  $234E/$2355 probe offset
 *   $2316/$231D SATB attr  $2324/$232B pattern  $235C ?  $2363 timer
 *   $236A damage  $2371/$2378 hit box  $237F:$2386 handler
 *   $238D:$2394 hit handler (bank 0 $F313 swaps it in)  $239B:$23A2 script
 *   $23A9 / $23B0 per-handler state
 * Slots 0-2 fireballs (magic 1), slot 0 then 0-3 the fire pillar (magic 3),
 * slots 4-6 the orbiting shield (magic 4, from the pause selection).
 * Magic 2 ($71 = $F0) freezes enemies; magic 5 ($74) is the screen sweep
 * run from the main body ($ED61 -> $D7BC).
 *
 * The source also writes each live slot's SATB entry (VDC $0002/$0003 after
 * $BA4C register selects): that is presentation and is left to the renderer
 * (it can draw the slots from the RAM above). $BA4C keeps its argument in
 * zp $8B, which is kept.
 *
 * Control flow mirrors the source: JMP = tail call, JSR = call. Handlers may
 * change the slot loop's X register (the pillar handler $D4A9 ends with
 * CLX), so the loop index lives in `sx` like the source register.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"

#define B9(a)   (ss2_bank9[(u16)(a) - 0xC000])

#define S_STATE 0x22E5
#define S_XH    0x22EC
#define S_XL    0x22F3
#define S_XF    0x22FA
#define S_YH    0x2301
#define S_YL    0x2308
#define S_YF    0x230F
#define S_VXH   0x2332
#define S_VXL   0x2339
#define S_VYH   0x2340
#define S_VYL   0x2347
#define S_PRX   0x234E
#define S_PRY   0x2355
#define S_TIMER 0x2363
#define S_HANH  0x237F
#define S_HANL  0x2386
#define S_ALTH  0x238D
#define S_ALTL  0x2394
#define S_SCRH  0x239B
#define S_SCRL  0x23A2
#define S_V0    0x23A9
#define S_V1    0x23B0

static u8 sx;                               /* the source's X in $D03D */

static void run(u8 x);

/* script and descriptor bytes: bank 9 code addresses; anything else
 * through the current CPU view */
static u8 rb(u16 a)
{
    if (a >= 0xC000 && a < 0xE000) return B9(a);
    return ss2_rom_byte(a);
}

/* $D1CB: free the slot (and blank its SATB entry: presentation) */
static void kill(u8 x)
{
    O(S_STATE, x) = 0;
}

/* $D99A: 16-byte slot descriptor */
static void desc(u8 x, u16 d)
{
    O(S_STATE, x) = rb(d);
    O(0x235C, x) = rb((u16)(d + 1));
    O(S_TIMER, x) = rb((u16)(d + 2));
    O(0x236A, x) = rb((u16)(d + 3));
    O(0x2371, x) = rb((u16)(d + 4));
    O(0x2378, x) = rb((u16)(d + 5));
    O(0x231D, x) = rb((u16)(d + 6));
    O(0x2316, x) = rb((u16)(d + 7));
    O(0x232B, x) = rb((u16)(d + 8));
    O(0x2324, x) = rb((u16)(d + 9));
    O(S_HANL, x) = rb((u16)(d + 10));
    O(S_HANH, x) = rb((u16)(d + 11));
    O(S_ALTL, x) = rb((u16)(d + 12));
    O(S_ALTH, x) = rb((u16)(d + 13));
    O(S_SCRL, x) = rb((u16)(d + 14));
    O(S_SCRH, x) = rb((u16)(d + 15));
}

static void set_handler(u8 x, u16 h)
{
    O(S_HANL, x) = (u8)h;
    O(S_HANH, x) = (u8)(h >> 8);
}

/* slot position = player + 8 (byte carry chain) */
static void at_player(u8 x)
{
    u16 t = (u16)(P_XL + 8);
    O(S_XL, x) = (u8)t;
    O(S_XH, x) = (u8)(P_XH + (t >> 8));
    t = (u16)(P_YL + 8);
    O(S_YL, x) = (u8)t;
    O(S_YH, x) = (u8)(P_YH + (t >> 8));
}

/* vy += $0030 */
static void fall(u8 x)
{
    u16 t = (u16)(O(S_VYL, x) + 0x30);
    O(S_VYL, x) = (u8)t;
    O(S_VYH, x) = (u8)(O(S_VYH, x) + (t >> 8));
}

/* bank 3 $835B: map probe at the slot + ($234E, $2355) */
static u8 probe(u8 x)
{
    u16 px = (u16)(((O(S_XH, x) << 8) | O(S_XL, x)) + O(S_PRX, x));
    u16 py = (u16)(((O(S_YH, x) << 8) | O(S_YL, x)) + O(S_PRY, x));
    return ss2_probe_point(px, py);
}

/* bank 3 $8548: keep the slot fixed in the world as the camera moves */
static void follow_camera(u8 x)
{
    s8 d = (s8)(u8)(ZP(0x4C) - Z_CAMX_LO);
    u16 v = (u16)(((O(S_XH, x) << 8) | O(S_XL, x)) + d);
    O(S_XL, x) = (u8)v; O(S_XH, x) = (u8)(v >> 8);
    d = (s8)(u8)(ZP(0x4F) - Z_CAMY_LO);
    v = (u16)(((O(S_YH, x) << 8) | O(S_YL, x)) + d);
    O(S_YL, x) = (u8)v; O(S_YH, x) = (u8)(v >> 8);
}

/* 24-bit position += sign-extended 8.8 velocity. Returns the source's
 * off-screen test value: hi:lo + $30 plus the carry left by the top byte's
 * add (the source does not clear it before ADC #$30). */
static u16 step(u8 x, u16 h, u16 l, u16 f, u16 vh, u16 vl)
{
    u16 t = (u16)(O(f, x) + O(vl, x));
    O(f, x) = (u8)t;
    t = (u16)(O(l, x) + O(vh, x) + (t >> 8));
    O(l, x) = (u8)t;
    t = (u16)(O(h, x) + ((O(vh, x) & 0x80) ? 0xFF : 0x00) + (t >> 8));
    O(h, x) = (u8)t;
    return (u16)(((O(h, x) << 8) | O(l, x)) + 0x30 + (t >> 8));
}

/* $D09E: animation script, camera follow, motion, off-screen check */
static void run(u8 x)
{
    if (!--O(S_TIMER, x))
    {
        u16 p = (u16)((O(S_SCRH, x) << 8) | O(S_SCRL, x));
        u8 a = rb(p), b;
        if (!a)
        {
            /* $D0BA: 0, address: jump */
            O(S_SCRL, x) = rb((u16)(p + 1));
            O(S_SCRH, x) = rb((u16)(p + 2));
            p = (u16)((O(S_SCRH, x) << 8) | O(S_SCRL, x));
        }
        else if (a == 0xFF) { kill(x); return; }
        /* $D0CC: (flags | frames, pattern) */
        a = rb(p);
        O(S_TIMER, x) = (u8)(a & 0x3F);
        O(0x2324, x) = 2;
        b = rb((u16)(p + 1));
        if (b == 0xFF) { O(0x2324, x) = 0; b = 0x90; }
        O(0x232B, x) = b;
        O(0x2316, x) = (u8)((O(0x2316, x) & 0x77) | (a & 0x80) | ((a & 0x40) >> 3));
        p = (u16)(((O(S_SCRH, x) << 8) | O(S_SCRL, x)) + 2);
        O(S_SCRL, x) = (u8)p;
        O(S_SCRH, x) = (u8)(p >> 8);
    }
    /* $D111 */
    follow_camera(x);
    if (step(x, S_XH, S_XL, S_XF, S_VXH, S_VXL) >= 0x130) { kill(x); return; }
    if (step(x, S_YH, S_YL, S_YF, S_VYH, S_VYL) >= 0x130) { kill(x); return; }
    /* $D192: SATB entry -- presentation */
}

/* ---- magic 1: fireballs (slots 0-2) ---------------------------------- */

/* vx from the attack direction */
static void throw_x(u8 x)
{
    O(S_VXL, x) = 0x80;
    O(S_VXH, x) = ((P_ATKDIR + 1) & 4) ? 0xFE : 0x01;
}

/* $D26D: launch */
static void fire_launch(u8 x)
{
    O(S_VYL, x) = 0x80;
    O(S_VYH, x) = 0xFD;
    throw_x(x);
    set_handler(x, 0xD2A2);
    run(x);
}

/* $D32E: hit handler: end on the next script step ($D350 = $FF) */
static void fire_end(u8 x)
{
    O(S_STATE, x) = 0x40;
    O(S_TIMER, x) = 1;
    O(S_SCRL, x) = 0x50;
    O(S_SCRH, x) = 0xD3;
    run(x);
}

/* $D2A2: bounce along the floor, turn once at a wall */
static void fire_fly(u8 x)
{
    if ((s8)O(S_VYH, x) >= 0)
    {
        O(S_PRX, x) = 0x08;
        O(S_PRY, x) = 0x10;
        if (!probe(x)) goto gravity;
        if (Z_HIT_HURT) goto end;
        O(S_VYL, x) = 0x00;
        O(S_VYH, x) = 0xFD;
        run(x);
        return;
    }
    /* $D2C7 */
    O(S_PRX, x) = ((s8)O(S_VXH, x) < 0) ? 0x00 : 0x10;
    O(S_PRY, x) = 0x08;
    if (!probe(x)) goto gravity;
    if (O(S_V0, x)) goto end;
    O(S_V0, x) = 1;
    O(S_VXL, x) = 0x80;
    O(S_VXH, x) = ((s8)O(S_VXH, x) < 0) ? 0x01 : 0xFE;
    run(x);
    return;
end:
    /* $D306 */
    set_handler(x, 0xD32E);
    run(x);
    return;
gravity:
    /* $D313 */
    if (O(S_VYH, x) != 0x03) fall(x);
    run(x);
}

/* $D1FB */
static void magic_fire(void)
{
    u8 x;
    if (!P_MAGIC) return;
    for (x = 0; x < 3; x++) if (!O(S_STATE, x)) break;
    if (x == 3) return;
    if (R(0x22BE)) return;
    P_MAGIC--;
    if (!--R(0x22BD)) ZP(0x79) = 0x5A;
    desc(x, 0xD25D);
    at_player(x);
    O(S_V0, x) = 0;
    ss2_sfx(0x22);
}

/* ---- magic 2: freeze ------------------------------------------------- */

/* pay bank 1 $4CEA + n; returns 0 when $22BB is short */
static u8 pay(u8 n)
{
    u8 c = ss2_rom_byte((u16)(0x4CEA + n)), a;
    if (P_MAGIC < c) return 0;
    a = (u8)(P_MAGIC - c);
    P_MAGIC = a;
    R(0x22BD) = a;
    if (!a) ZP(0x79) = 0x5A;
    return 1;
}

/* $D351 */
static void magic_freeze(void)
{
    if (!pay(2)) return;
    ZP(0x71) = 0xF0;
    ss2_sfx(0x23);
}

/* ---- magic 3: fire pillar (slot 0, then slots 0-3) ------------------- */

/* $D375 */
static void magic_pillar(void)
{
    u8 c = ss2_rom_byte(0x4CED), a;
    if (P_MAGIC < c) return;
    a = (u8)(P_MAGIC - c);
    if (R(0x22E5) | R(0x22E6) | R(0x22E7) | R(0x22E8)) return;
    P_MAGIC = a;
    R(0x22BD) = a;
    if (!a) ZP(0x79) = 0x5A;
    R(0x22BE) = 8;
    desc(0, 0xD3DE);
    at_player(0);
    R(0x234E) = 0x08;
    R(0x2355) = 0x10;
}

/* $D42E: launch (slot 0) */
static void pillar_launch(u8 x)
{
    R(0x2347) = 0x80;
    R(0x2340) = 0xFB;
    throw_x(0);
    R(0x2386) = 0x63;
    R(0x237F) = 0xD4;
    run(x);
}

/* $D463: fly until the probe meets the map */
static void pillar_fly(u8 x)
{
    if (probe(x))
    {
        /* $D483 */
        ss2_sfx(0x24);
        R(0x237F) = R(0x238D);
        R(0x2386) = R(0x2394);
        run(x);
        return;
    }
    if (R(0x2340) != 0x04) fall(0);
    run(x);
}

/* $D4A9: burst into a four-slot column above the impact */
static void pillar_burst(void)
{
    static const u16 d[4] = { 0xD3EE, 0xD3FE, 0xD40E, 0xD41E };
    u8 x;
    u16 y0, t;
    for (x = 0; x < 4; x++) desc(x, d[x]);
    for (x = 0; x < 4; x++)
    {
        R(0x2332 + x) = 0; R(0x2339 + x) = 0;
        R(0x2340 + x) = 0; R(0x2347 + x) = 0;
    }
    R(0x22ED) = R(0x22EE) = R(0x22EF) = R(0x22EC);
    R(0x22F4) = R(0x22F5) = R(0x22F6) = R(0x22F3);
    y0 = (u16)((R(0x2301) << 8) | R(0x2308));
    for (x = 1; x < 4; x++)
    {
        t = (u16)(y0 - x * 0x10);
        R(0x2308 + x) = (u8)t;
        R(0x2301 + x) = (u8)(t >> 8);
    }
    R(0x22BE) = 0;
    sx = 0;
    run(0);
}

/* $D497: column part hit something: stays live */
static void pillar_hit(u8 x)
{
    O(S_STATE, x) = 0x80;
    set_handler(x, 0xD09E);
    run(x);
}

/* ---- magic 4: shield (slots 4-6) ------------------------------------- */

/* $D702: dismiss: each part ends on its next script step ($D78E = $FF) */
void ss2_shield_end(void)
{
    u8 x;
    ZP(0x79) = 0x5A;
    for (x = 4; x < 7; x++)
    {
        O(S_STATE, x) = 0x40;
        O(S_TIMER, x) = 1;
        O(S_SCRL, x) = 0x8E;
        O(S_SCRH, x) = 0xD7;
        set_handler(x, 0xD09E);
    }
}

/* $D6B9: orbit the player's trail point ($22E3, $22E4) */
static void shield_orbit(u8 x)
{
    if ((s8)--O(S_V1, x) >= 0) { run(x); return; }
    {
        u8 a, y;
        O(S_V1, x) = 0;
        a = (u8)((O(S_V0, x) + 5) & 0x1F);
        O(S_V0, x) = a;
        y = (u8)(a << 1);
        O(S_XL, x) = (u8)(B9(0xD749 + y) + R(0x22E3));
        O(S_YL, x) = (u8)(B9(0xD749 + y + 1) + R(0x22E4));
    }
    run(x);
}

/* $D6EB: hit handler: keep orbiting while magic lasts */
static void shield_hit(u8 x)
{
    if (P_MAGIC)
    {
        O(S_STATE, x) = 0x80;
        set_handler(x, 0xD6B9);
        shield_orbit(x);
        return;
    }
    ss2_shield_end();
}

/* $D622: raise the shield (bank 0 $FEAE, pause selection of magic 4) */
void ss2_shield_start(void)
{
    static const u16 d[3] = { 0xD689, 0xD699, 0xD6A9 };
    static const u8 phase[3] = { 0x30, 0x06, 0x1A };
    u8 i;
    if (!P_MAGIC) return;
    for (i = 0; i < 3; i++)
    {
        desc((u8)(4 + i), d[i]);
        O(S_V0, 4 + i) = phase[i];
        O(S_V1, 4 + i) = 0;
    }
    R(0x236E) = R(0x236F) = R(0x2370) = (u8)(R(0x22B4) << 1);
    R(0x23C2) = R(0x23C3) = R(0x23C4) = 0x02;
    R(0x23BB) = R(0x23BC) = R(0x23BD) = 0xD7;
}

/* ---- magic 5: sweep -------------------------------------------------- */

/* $D78F */
static void magic_sweep_start(void)
{
    if (ZP(0x74)) return;
    if (!pay(5)) return;
    ZP(0x74) = 1;
    ZP(0x75) = 0;
    ss2_sfx(0x25);
}

/* $D86A: every damageable object loses `amount` ($35) */
void ss2_magic_damage_all(u8 amount)
{
    u8 x;
    if (!ZP(0x6C)) return;
    for (x = 0; x < 0x20; x++)
    {
        u8 s = O(0x245A, x), a;
        if (!s || s == 0x40) continue;
        if (O(0x26BA, x) || (O(0x269A, x) & 0x01)) continue;
        if (O(0x281A, x) < 0x20) O(0x297A, x) = 1;
        a = O(0x26DA, x);
        O(0x26DA, x) = (u8)(a - amount);
        if (a <= amount)
        {
            ss2_score_award(O(0x279A, x));
            O(0x245A, x) = 0x40;
        }
        O(0x283A, x) = 0x0A;
    }
}

/* $D7DD: draw the sweep sprites ($D8D0 list of 4-byte SATB entries, groups
 * ended by $FF, list by $FF $FF; presentation). While $74 bit 7 is clear
 * the sweep grows by one group per frame ($75 groups); at the end of the
 * list every object is hit, then the whole list flashes until $75 = $3C. */
static void sweep_draw(void)
{
    u8 x = 0, n = 0;
    ZP(0x8B) = 2;                               /* $D8B6 / $BA4C(2) */
    for (;;)
    {
        x = (u8)(x + 4);
        if (B9(0xD8D0 + x) != 0xFF) continue;
        if (B9(0xD8D1 + x) == 0xFF) break;
        x++;
        if (ZP(0x74) & 0x80) continue;
        if (ZP(0x75) == ++n) return;
    }
    /* $D845 */
    if (!(ZP(0x74) & 0x80))
    {
        ZP(0x75) = 0;
        ZP(0x73) = 1;
        ss2_magic_damage_all(0x20);
    }
    if (ZP(0x75) < 0x3C) { ZP(0x74) |= 0x80; return; }
    R(0x2201) |= 0x80;                          /* $BA1E: VDC control shadow */
    ZP(0x8B) = 2;                               /* $D8B6 */
    ZP(0x74) = 0;
}

/* $D7BC (from the main body while $74 is set) */
void ss2_magic_sweep(void)
{
    u8 t;
    if (!(ZP(0x74) & 0x80))
    {
        ZP(0x75)++;
        sweep_draw();
        return;
    }
    ZP(0x74) &= (u8)~0x40;
    t = ZP(0x75)++;
    if (t & 4) return;
    ZP(0x74) |= 0x40;
    sweep_draw();
}

/* ---- casting --------------------------------------------------------- */

/* $D1E8: cast the selected magic (the caller continues at $CC35) */
void ss2_magic_cast(void)
{
    switch (P_MAGICSEL)
    {
        case 1: magic_fire(); return;
        case 2: magic_freeze(); return;
        case 3: magic_pillar(); return;
        case 4: return;                         /* the shield is cast on pause */
        case 5: magic_sweep_start(); return;
    }
    ss2_unported(0xD1EE);
}

/* $FE5A: weapon/magic box redraw, ported in src/ss2_game.c. This weak
 * stub (its RAM effects: $BA4C leaves $8B = 2; the box is presentation)
 * only lets this file link on its own. */
void ss2_weapon_box(void);
__attribute__((weak)) void ss2_weapon_box(void)
{
    ss2_hud_dirty |= 4;
    ZP(0x8B) = 2;
}

/* ---- slot loop ------------------------------------------------------- */

/* $D091: JMP ($2026) through the slot's handler */
static void dispatch(u8 x)
{
    u16 h = (u16)((O(S_HANH, x) << 8) | O(S_HANL, x));
    ZP(0x26) = (u8)h;
    ZP(0x27) = (u8)(h >> 8);
    switch (h)
    {
        case 0xD09E: run(x); return;
        case 0xD26D: fire_launch(x); return;
        case 0xD2A2: fire_fly(x); return;
        case 0xD32E: fire_end(x); return;
        case 0xD42E: pillar_launch(x); return;
        case 0xD463: pillar_fly(x); return;
        case 0xD497: pillar_hit(x); return;
        case 0xD4A9: pillar_burst(); return;
        case 0xD6B9: shield_orbit(x); return;
        case 0xD6EB: shield_hit(x); return;
    }
    ss2_unported(h);
}

/* $D03D: run the shot slots (only the shield's while paused), then count
 * the empty-magic delay down and drop the selection */
void ss2_shots_update(void)
{
    sx = ZP(0x6B) ? 4 : 0;
    ZP(0x8B) = 2;                               /* $BA4C(0), $BA4C(2) */
    do
    {
        if (O(S_STATE, sx)) dispatch(sx);
        else kill(sx);
    } while (++sx != 7);
    if (P_MAGIC) return;
    if (ZP(0x79)) { ZP(0x79)--; return; }
    P_MAGICSEL = 0;
    ss2_weapon_box();
}

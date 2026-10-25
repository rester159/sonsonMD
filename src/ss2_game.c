/* SON SON II gameplay frame, following the HuCard's bank 0 task order:
 *
 *   VBlank IRQ   $E0B9  pad ($FEDE), frame counter
 *   task 1       $EC3B  camera copy, player ($C000), frame select ($EC5D),
 *                       play clock ($ECA3), spawn scan ($E845 -> $BC10)
 *   task 2       $ECCF  objects ($ED0D), position trail ($ED22),
 *                       staff spark ($8732), respawn timers ($ED6D)
 *   sprites      bank 3 $8818 builds the sprite table (ss2_sat)
 *
 * Map streaming ($E841) is replaced by the native plane-ring renderer.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"
#include "ss2_task.h"

/* field a of slot x through o = &ss2_ram[x] (a fixed displacement) */
#define OF(a) (o[(u16)((a) - 0x2000)])


/* sprite table in the PCE SATB word layout: y, x, pattern, attribute */
u16 ss2_sat[64 * 4];
u8 ss2_sat_hi;          /* entries at and above this are empty (see sat_put) */
u8 ss2_hud_dirty;

/* $FEDE (player 1 only): held and newly pressed, PCE bit layout */
static void read_pad(u8 pad)
{
    u8 old = Z_PAD;
    ZP(0x1B) = old;
    Z_PAD = pad;
    Z_PRESS = (u8)((pad ^ old) & pad);
}

/* $EC5D: pick the player's pattern pointer and attribute from $DB23 */
static void select_frame(void)
{
    u16 set, st, e;
    u8 y;
    if (P_MODE == 0x40) return;
    ss2_map6(9);
    set = (u16)(ss2_rom_byte((u16)(0xDB23 + P_ANIMSET)) | (ss2_rom_byte((u16)(0xDB24 + P_ANIMSET)) << 8));
    st = (u16)(ss2_rom_byte((u16)(set + (u8)(P_DIR << 1))) | (ss2_rom_byte((u16)(set + (u8)(P_DIR << 1) + 1)) << 8));
    y = (u8)((P_ANIMTICK & 0x18) >> 2);
    y = (u8)(y + (y >> 1));
    e = (u16)(st + y);
    P_FRAMEL = ss2_rom_byte(e);
    P_FRAMEH = ss2_rom_byte((u16)(e + 1));
    P_ATTRH = ss2_rom_byte((u16)(e + 2));
}

/* $ECA3: BCD play clock, stops at 99 minutes */
static u8 bcd_inc(u8 v)
{
    v = (u8)(v + 1);
    if ((v & 0x0F) == 0x0A) v = (u8)(v + 6);
    return v;
}

static void play_clock(void)
{
    if (Z_TIME_M == 0x99) return;
    Z_TIME_F = bcd_inc(Z_TIME_F);
    if (Z_TIME_F != 0x60) return;
    Z_TIME_F = 0;
    Z_TIME_S = bcd_inc(Z_TIME_S);
    if (Z_TIME_S != 0x60) return;
    Z_TIME_S = 0;
    Z_TIME_M = bcd_inc(Z_TIME_M);
}

/* $ED22: last five player screen positions */
static void trail(void)
{
    u8 i;
    for (i = 0; i < 8; i++) R(0x22E4 - i) = R(0x22E2 - i);
    R(0x22DB) = P_XL;
    R(0x22DC) = P_YL;
}

/* bank 3 $8732: staff spark follows the camera while its timer runs */
static void spark(void)
{
    u16 v, d;
    if (R(0x2455))
    {
        R(0x2455)--;
        d = (u16)(((ZP(0x4B) << 8) | ZP(0x4C)) - ((Z_CAMX_HI << 8) | Z_CAMX_LO));
        v = (u16)(((R(0x2456) << 8) | R(0x2457)) + d);
        R(0x2457) = (u8)v; R(0x2456) = (u8)(v >> 8);
        d = (u16)(((ZP(0x4E) << 8) | ZP(0x4F)) - ((Z_CAMY_HI << 8) | Z_CAMY_LO));
        v = (u16)(((R(0x2458) << 8) | R(0x2459)) + d);
        R(0x2459) = (u8)v; R(0x2458) = (u8)(v >> 8);
    }
    /* $87BA: the Kinto'un cloud rides under the player */
    if (R(0x23C5) && P_ONCLOUD) R(0x23CA) = (u8)(P_YL + 0x1C);
}

/* ---- sprite table (bank 3 $8732 entry 0, $8818 entries 2-6 and 16+) --- */

static u8 sat_i;

static void sat_put(u16 y, u16 x, u16 pat, u16 attr)
{
    u16 *e = &ss2_sat[(sat_i & 63) * 4];
    e[0] = y; e[1] = x; e[2] = pat; e[3] = attr;
    sat_i++;
    /* the renderer scans entries below ss2_sat_hi only; it lowers the mark
     * to the last non-empty entry it saw */
    if ((y & 0x3FF) && (u8)(((sat_i - 1) & 63) + 1) > ss2_sat_hi) ss2_sat_hi = (u8)(((sat_i - 1) & 63) + 1);
}

/* $896A: one sprite per object */
static void sat_object(u8 x)
{
    u8 *const o = &ss2_ram[x];
    u8 lo;
    if (OF(0x245A) != 0x40 && !OF(0x26BA) && ZP(0x71) && ZP(0x71) < 0x28 && !(ZP(0x71) & 4))
        return;
    ZP(0x38)++;
    lo = OF(0x255A);
    if (OF(0x283A)) lo |= 0x0F;
    sat_put((u16)((((OF(0x24DA) << 8) | OF(0x24FA)) + 0x40)),
            (u16)((((OF(0x247A) << 8) | OF(0x249A)) + 0x20)),
            (u16)((OF(0x257A) << 8) | OF(0x259A)),
            (u16)((OF(0x253A) << 8) | lo));
}

/* $89D2: composite object: list of (dy, dx, frame, attr hi) */
static void sat_composite(u8 x)
{
    u8 *const o = &ss2_ram[x];
    u16 list, an;
    u8 n, y;
    if (OF(0x245A) != 0x40 && !OF(0x26BA) && ZP(0x71) && ZP(0x71) < 0x28 && !(ZP(0x71) & 4))
        return;
    ss2_map6(OF(0x275A));
    list = (u16)((OF(0x257A) << 8) | OF(0x259A));
    n = ss2_rom_byte((u16)(list + 1));
    if (!n) return;
    an = (u16)((OF(0x27BA) << 8) | OF(0x27DA));
    for (y = 2; n; n--)
    {
        u16 sy, sx, pat;
        u8 f, lo;
        sy = (u16)(((OF(0x24DA) << 8) | OF(0x24FA)) + (s8)ss2_rom_byte((u16)(list + y)) + 0x40);
        y++;
        sx = (u16)(((OF(0x247A) << 8) | OF(0x249A)) + (s8)ss2_rom_byte((u16)(list + y)) + 0x20);
        y++;
        f = ss2_rom_byte((u16)(list + y));
        if (f == 0xFF) pat = 0x0090;
        else
        {
            u16 t = (u16)(f + ss2_rom_byte((u16)(an + 1)));
            pat = (u16)((u8)t | ((u8)(ss2_rom_byte(an) + (t >> 8)) << 8));
        }
        y++;
        lo = OF(0x255A);
        if (OF(0x283A)) lo |= 0x0F;
        sat_put(sy, sx, pat, (u16)((ss2_rom_byte((u16)(list + y)) << 8) | lo));
        y++;
        ZP(0x38)++;
#ifdef SS2_TPROF
        { extern u16 ss2_tc_now[4]; ss2_tc_now[2]++; }   /* composite parts per pass */
#endif
    }
}

/* $8732's RAM side inside the main body ($ED5E): $89D2 maps the bank of
 * each drawable composite object (front ones, then rear ones), so $8E ends
 * on the last. build_sat() runs at the frame end on the target and keeps
 * $8E, so a main task suspended in a sequence (message, shop, door) is not
 * disturbed. */
static void sat_banks(void)
{
    /* only the last mapping's state ($8E, page 6) remains: the last drawable
     * composite of the rear pass if any, else of the front pass, mapped once.
     * One walk over the slots; if the unported placeholder bank occurs, the
     * source order of mappings (its diagnostic) is replayed instead. */
    u8 x, last0 = 0, last1 = 0, f0 = 0, f1 = 0, slow = 0, z71 = ZP(0x71);
    u8 blink = (u8)(z71 && z71 < 0x28 && !(z71 & 4));
    const u8 *o = &ss2_ram[0x045A];
    for (x = 0; x < 0x20; x++, o++)
    {
        u8 st = *o;
        if (!st || !(o[0x0240] & 0x20)) continue;            /* $269A */
        if (st != 0x40 && !o[0x0260] && blink) continue;     /* $26BA */
        {
            u8 b = o[0x0300];                                /* $275A */
            if (ss2_banks[b & 0x1F] == ss2_banks[0x0A]) slow = 1;
            if (o[0x0420]) { last1 = b; f1 = 1; }            /* $287A: rear */
            else { last0 = b; f0 = 1; }
        }
    }
    if (slow)
    {
        u8 pass;
        for (pass = 0; pass < 2; pass++)
            for (x = 0; x < 0x20; x++)
                if (O(0x245A, x) && !O(0x287A, x) == !pass && (O(0x269A, x) & 0x20)
                    && !(O(0x245A, x) != 0x40 && !O(0x26BA, x) && blink))
                    ss2_map6(O(0x275A, x));
        return;
    }
    if (f1) ss2_map6(last1);
    else if (f0) ss2_map6(last0);
}

/* the shop scene's sprites (bank 3 $9EF0-$9F87, bank 4 $A514-$A5F9):
 * shopkeeper, the four goods icons and the cursor's four corners */
static void build_shop_sat(void)
{
    u16 b = (u16)((R(0x2D9C) << 8) | R(0x2D9D)), g = ss2_shop_goods, cy, cx, cp;
    u8 i, t = ss2_shop_type;
    sat_i = 0;
    sat_put(0x0098, 0x0060, (u16)(((R(0x2DB6) << 8) | R(0x2DB7)) + (u8)(t << 3)),
            (u16)(0x1100 | ss2_rom_byte((u16)(0xA842 + t))));
    for (i = 0; i < 4; i++)
    {
        u16 a = (u16)(0xA853 + i * 4);
        sat_put((u16)(ss2_rom_byte(a) | (ss2_rom_byte((u16)(a + 1)) << 8)),
                (u16)(ss2_rom_byte((u16)(a + 2)) | (ss2_rom_byte((u16)(a + 3)) << 8)),
                (u16)(b + ss2_rom_byte((u16)(g + i * 4))),
                ss2_rom_byte((u16)(0xA845 + ss2_rom_byte((u16)(g + i * 4 + 1)))));
    }
    i = (u8)(ZP(0xB9) << 2);
    cy = (u16)(ss2_rom_byte((u16)(0xA86B + i)) | (ss2_rom_byte((u16)(0xA86C + i)) << 8));
    cx = (u16)(ss2_rom_byte((u16)(0xA86D + i)) | (ss2_rom_byte((u16)(0xA86E + i)) << 8));
    cp = (u16)(b + 0x3A);
    sat_put(cy, cx, cp, 0x0086);
    sat_put(cy, (u16)(cx + 0x10), cp, 0x0886);
    sat_put((u16)(cy + 0x10), cx, cp, 0x8086);
    sat_put((u16)(cy + 0x10), (u16)(cx + 0x10), cp, 0x8886);
    while (sat_i < 64) sat_put(0, 0, 0, 0);
}

/* bank 3 $8732 called outside the main body (title, intro, death): its
 * RAM effects; the table itself is built at the frame end */
void ss2_sat_update(void)
{
    spark();
    sat_banks();
    ZP(0x8B) = 0x02;                        /* its SATB writes select VWR */
}

static void build_sat(void)
{
    u8 x, bank = ZP(0x8E);
    if (ss2_shop_screen) { build_shop_sat(); return; }
    /* entry 0: staff spark */
    sat_i = 0;
    if (R(0x2455))
        sat_put((u16)(((R(0x2458) << 8) | R(0x2459)) + 0x40), (u16)(((R(0x2456) << 8) | R(0x2457)) + 0x20), 0x026C, 0x0080);
    else
        sat_put(0, (u16)(((R(0x2456) << 8) | R(0x2457)) + 0x20), 0x026C, 0x0080);
    /* entries 2-6: staff segments ($8818) */
    if (ss2_staff_sprites_hidden)
    {
        for (x = 2; x < 7; x++) { ss2_sat[x * 4] = 0; ss2_sat[x * 4 + 1] = 0; ss2_sat[x * 4 + 2] = 0; ss2_sat[x * 4 + 3] = 0; }
        ss2_staff_sprites_hidden = 0;
    }
    if (P_ATTACKING)
    {
        u8 dy = 0x06;
        if (!P_JUMPING && !P_ONVINE && (P_DIR & 3)) dy = 0x0F;
        sat_i = 2;
        for (x = 0; x < 5 && R(0x23DD + x) == 0x80; x++)
            sat_put((u16)((u8)(P_YL + dy) + 0x40),
                    (u16)((u16)(P_XL + ((R(0x23E2 + x) << 8) | R(0x23E7 + x))) + 0x20),
                    (u16)((R(0x240A + x) << 8) | R(0x240F + x)),
                    (u16)((R(0x2400 + x) << 8) | R(0x2405 + x)));
    }
    /* entry 1: the cloud ($87BA-$8815; left as it was while $23C5 = 0) */
    if (R(0x23C5))
    {
        sat_i = 1;
        sat_put((u16)(((R(0x23C9) << 8) | R(0x23CA)) + 0x40), (u16)(((R(0x23C6) << 8) | R(0x23C7)) + 0x20),
                (u16)((R(0x23CE) << 8) | R(0x23CF)), (u16)((R(0x23CC) << 8) | R(0x23CD)));
    }
    /* entries 7-13: shot slots ($D03D at $101C + 4x; while paused only the
     * shield slots 4-6 are refreshed) */
    for (x = ZP(0x6B) ? 4 : 0; x < 7; x++)
    {
        sat_i = (u8)(7 + x);
        if (R(0x22E5 + x))
            sat_put((u16)(((R(0x2301 + x) << 8) | R(0x2308 + x)) + 0x40),
                    (u16)(((R(0x22EC + x) << 8) | R(0x22F3 + x)) + 0x20),
                    (u16)((R(0x2324 + x) << 8) | R(0x232B + x)),
                    (u16)((R(0x2316 + x) << 8) | R(0x231D + x)));
        else
            sat_put(0, 0, 0, 0);
    }
    /* entries 16+: front objects, player, rear objects */
    sat_i = 16;
    ZP(0x38) = 0;
    {
        const u8 *o = &ss2_ram[0x045A];                      /* $245A */
        for (x = 0; x < 0x20; x++, o++)
            if (*o && !o[0x0420])                            /* $287A */
            {
                if (o[0x0240] & 0x20) sat_composite(x); else sat_object(x);
            }
    }
    if (P_MODE)
    {
        u16 pat = 0x0200;
        if (P_HURT && !(P_HITOBJ | P_C4) && (P_HURT & 2)) pat = 0x0090;
        sat_put((u16)(((P_YH << 8) | P_YL) + 0x40), (u16)(((P_XH << 8) | P_XL) + 0x20), pat,
                (u16)((P_ATTRH << 8) | P_ATTRL));
        ZP(0x38)++;
    }
    {
        const u8 *o = &ss2_ram[0x045A];
        for (x = 0; x < 0x20; x++, o++)
            if (*o && o[0x0420])
            {
                if (o[0x0240] & 0x20) sat_composite(x); else sat_object(x);
            }
    }
    /* $8949: clear entries left over from a busier previous frame */
    if (ZP(0x39) > ZP(0x38))
    {
        u8 n = (u8)(ZP(0x39) - ZP(0x38));
        for (; n; n--) sat_put(0, 0, 0, 0);
    }
    ZP(0x39) = ZP(0x38);
    ss2_map6(bank);                         /* see sat_banks() */
}

/* bank 2 $7649/$7688: life and magic bars. The shown value creeps up to
 * the real one, one unit every 8 frames; the bar drawn this frame uses the
 * value before the step (ss2_hud_life/magic). Low life beeps every 32
 * frames. */
u8 ss2_hud_life[2], ss2_hud_magic[2];      /* [0] shown units, [1] max units */

static u8 bar_step(u8 shown, u8 real)
{
    if (shown != real && !(Z_FRAME & 7)) shown++;
    return shown;
}

void ss2_hud_life_bar(void)                 /* $7649 */
{
    if (P_LIFE < 5 && P_MODE != 0x40 && !(Z_FRAME & 0x1F)) ss2_sfx(0x18);
    ss2_hud_life[0] = R(0x22BA);
    ss2_hud_life[1] = R(0x22B9);
    R(0x22BA) = bar_step(R(0x22BA), P_LIFE);
}

void ss2_hud_magic_bar(void)                /* $7688 */
{
    ss2_hud_magic[0] = R(0x22BD);
    ss2_hud_magic[1] = R(0x22BC);
    R(0x22BD) = bar_step(R(0x22BD), P_MAGIC);
}

void ss2_hud_bars(void)
{
    ss2_hud_life_bar();
    ss2_hud_magic_bar();
}

/* bank 4 $BA66: compare 8 digits at a against b: -1, 0, 1 */
static s8 cmp8(u16 a, u16 b)
{
    u8 y;
    for (y = 0; y < 8; y++)
    {
        u8 va = ss2_rom_byte((u16)(a + y)), vb = ss2_rom_byte((u16)(b + y));
        if (va < vb) return -1;
        if (va != vb) return 1;
    }
    return 0;
}

/* $FD05: task 3: advance the seed, apply one queued award, track the
 * high score. Awards < $20 add to the score digits ($2204-$220A) with
 * extra life thresholds at ($80); others add to the money digits. */
static void score_task(void)
{
    u8 id;
    ss2_random();
    id = R(0x2263 + R(0x2283));
    if (id != 0xFF)
    {
        u8 x, y, c;
        u16 tab, dig;
        R(0x2263 + R(0x2283)) = 0xFF;
        R(0x2283) = (u8)((R(0x2283) + 1) & 0x1F);
        x = (u8)((id << 3) | 7);
        if (id & 0x20) { tab = 0x4BEC; dig = 0x220B; }
        else { tab = 0x4AEC; dig = 0x2203; }
        for (y = 7, c = 0; y; y--, x--)
        {
            u8 d = (u8)(ss2_rom_byte((u16)(tab + x)) + R(dig + y) + c);
            c = 0;
            if (d >= 0x0A) { d = (u8)(d - 0x0A); c = 1; }
            R(dig + y) = d;
        }
        if (!(id & 0x20))
        {
            ss2_hud_dirty |= 1;
            ZP(0x28) = 0x03; ZP(0x29) = 0x22;
            ZP(0x2A) = ZP(0x80); ZP(0x2B) = ZP(0x81);
            if (cmp8((u16)((ZP(0x81) << 8) | ZP(0x80)), 0x2203) <= 0)
            {
                u16 t;
                ss2_sfx(0x21);
                t = (u16)(((ZP(0x81) << 8) | ZP(0x80)) + 8);
                ZP(0x80) = (u8)t; ZP(0x81) = (u8)(t >> 8);
                R(0x22BC) += 2;
            }
        }
        else ss2_hud_dirty |= 2;
    }
    /* $FD8B: high score */
    ZP(0x28) = 0xF8; ZP(0x29) = 0x2F;
    ZP(0x2A) = 0x03; ZP(0x2B) = 0x22;
    if (cmp8(0x2203, 0x2FF8) > 0)
    {
        u8 i;
        for (i = 0; i < 8; i++) R(0x2FF8 + i) = R(0x2203 + i);
    }
}

/* VBlank IRQ part ($E119-$E11E) */
void ss2_game_vblank(u8 pad)
{
    read_pad(pad);
    Z_FRAME++;
    ZP(0x14) = 0;
}

#if defined(SS2_PROF) || defined(SS2_TPROF)
void ss2_prof_mark(u8 sec);
#define PROF(n) ss2_prof_mark(n)
#else
#define PROF(n)
#endif

/* bank 4 $BD16: the attract demo's pad: while no game runs ($6C = 0) the
 * pad comes from (pad, frames) pairs at ($C4), $C6 counting; $FF ends the
 * demo through $F884 */
static void demo_input(void)
{
    u16 p;
    u8 v;
    if (ZP(0xC3)) { ss2_unported(0xBD1F); return; }     /* demo recording (development) */
    if (ZP(0x6C)) return;
    ZP(0x1B) = Z_PAD;
    if (!--ZP(0xC6))
    {
        p = (u16)(((ZP(0xC5) << 8) | ZP(0xC4)) + 2);
        ZP(0xC4) = (u8)p; ZP(0xC5) = (u8)(p >> 8);
        ZP(0xC6) = ss2_rom_byte((u16)(p + 1));
    }
    v = ss2_rom_byte((u16)((ZP(0xC5) << 8) | ZP(0xC4)));
    if (v == 0xFF) ss2_task_restart(ss2_death_task);    /* JMP $F884 */
    Z_PAD = v;
    Z_PRESS = (u8)((v ^ ZP(0x1B)) & v);
}

/* $EC42-$EDA2: one pass of the main loop body (player .. respawn/HUD) */
static void main_body(void)
{
    u8 i;
    /* task 1 */
    for (i = 0; i < 6; i++) ZP(0x4B + i) = ZP(0x45 + i);
    if (ZP(0x71)) ZP(0x71)--;               /* $EC49: freeze/flash timer */
    ss2_map6(9);
    demo_input();                           /* $BD16 */
    ss2_player_update();
    ss2_shots_update();
    select_frame();
    play_clock();
    PROF(1);
    ss2_spawn_scan();
    PROF(2);
    /* task 2 */
    ss2_objects_update();
    PROF(3);
    ZP(0x73) = 0;                           /* $ED20 (set by the sweep $D845) */
    trail();
    spark();
    PROF(18);
    sat_banks();                            /* $ED5E: $8732 bank mapping */
    if (ZP(0x74)) { ss2_map6(9); ss2_magic_sweep(); }   /* $ED61 */
    if (!(Z_FRAME & 0x0F))
        for (i = 0x20; i < 0xBF; i++)
            if (R(0x2C1A + i) && R(0x2C1A + i) != 0xFF) R(0x2C1A + i)--;
    PROF(19);
    ss2_hud_bars();
    PROF(20);
}

/* $FE5A: the weapon/magic box: frame (lit when the selected magic costs
 * more than the magic left), the selected magic's icon, and with no magic
 * selected the staff level digit */
void ss2_weapon_box(void)
{
    u8 x = R(0x22C0), cost = ss2_rom_byte((u16)(0x4CEA + x));
    ss2_hud_text((cost && cost > R(0x22BB)) ? 0x4E1E : 0x4E07);
    ss2_hud_text((u16)(ss2_rom_byte((u16)(0xFED2 + x * 2)) | (ss2_rom_byte((u16)(0xFED3 + x * 2)) << 8)));
    if (!R(0x22C0))
        ss2_hud_put(0x0156, (u16)(0x3200 | (u8)(ZP(0x72) + 0x1A)));
}

/* $FE47: select magic $30 if owned ($22BF bit from $FECC) */
static u8 magic_try(void)
{
    u8 x = ZP(0x30);
    if (!(ss2_rom_byte((u16)(0xFECC + x)) & R(0x22BF))) return 0;
    R(0x22C0) = x;
    ss2_weapon_box();
    return 1;
}

/* $FE0C: while paused, Right/Left step to the next owned magic */
static void magic_select(void)
{
    u8 y;
    if (!R(0x22BB)) return;
    ZP(0x30) = R(0x22C0);
    if (Z_PRESS == 0x20)
        for (y = 0; y < 6; y++)
        {
            if (++ZP(0x30) == 6) ZP(0x30) = 0;
            if (magic_try()) return;
        }
    else if (Z_PRESS == 0x80)
        for (y = 0; y < 6; y++)
        {
            if ((s8)--ZP(0x30) < 0) ZP(0x30) = 5;
            if (magic_try()) return;
        }
}

/* $FEAE: the selected magic's preview while paused (bank 9 shot code) */
static void magic_preview(void)
{
    if (R(0x22C0) == 4)
    {
        if (R(0x22E9)) ss2_shots_update();          /* $D03D */
        else ss2_shield_start();                    /* $D622 */
        return;
    }
    if (!R(0x22E9)) return;
    ss2_shield_end();                               /* $D702 */
    ss2_shots_update();                             /* $D03D */
}

/* $FDD9: pause on RUN while a game is running ($6C) */
static void pause_check(void)
{
    if (ZP(0x6C) && (Z_PRESS & 0x08))
    {
        ZP(0x6B) = Z_PRESS & 0x08;
        ss2_sfx(0x1A);
        ss2_page[6] = ss2_banks[9];                 /* TAM #$40 = 9 */
        ss2_mpr6 = 9;
        do
        {
            ss2_wait(1);
            magic_select();
            magic_preview();
        } while (!(Z_PRESS & 0x08));
        ss2_sfx(0x1A);
        ss2_page[6] = ss2_banks[ZP(0x8E) & 0x1F];
        ss2_mpr6 = ZP(0x8E);
    }
    ZP(0x6B) = 0;
    ZP(0x14) = 0x88;
}

/* bank 4 $B9F0-$BA4B: VDC control register shadow $2201 (bit 7 background,
 * bit 6 sprites); the renderer follows it */
static void cr_set(u8 v)
{
    ZP(0x8B) = 0x05;                        /* $BA4C: CR selected */
    R(0x2201) = v;
}
void ss2_cr_set(u8 v) { cr_set(v); }
void ss2_bg_on(void)   { cr_set((u8)(R(0x2201) | 0x80)); }      /* $BA1E */
void ss2_bg_off(void)  { cr_set((u8)(R(0x2201) & 0x7F)); }      /* $BA35 */
void ss2_spr_on(void)  { cr_set((u8)(R(0x2201) | 0x40)); }      /* $B9F0 */
void ss2_spr_off(void) { cr_set((u8)(R(0x2201) & 0xBF)); }      /* $BA07 */
#define bg_on ss2_bg_on
#define bg_off ss2_bg_off
#define spr_on ss2_spr_on
#define spr_off ss2_spr_off

/* $EDB7: freeze-magic flash: while $71 >= $C8, every 4 frames the picture
 * goes white (background and sprites off, colour $100 white) and back */
static void flash(void)
{
    if (!ZP(0x71) || ZP(0x71) < 0xC8) return;
    if (ZP(0x71) & 4)
    {
        ss2_vce[0x100] = 0x1FF;
        bg_off();
        spr_off();
    }
    else
    {
        ss2_vce[0x100] = 0;
        bg_on();
        spr_on();
    }
    ss2_vce_dirty = 1;
}

/* $ED9A-$EDAF: after the frame's wait: weapon box, flash, $74 BG switch */
static void main_tail(void)
{
    ss2_weapon_box();                       /* $FE5A */
    flash();                                /* $EDB7 */
    if (ZP(0x74))                           /* $EDA0: sweep magic hides the BG */
    {
        if (ZP(0x74) & 0x40) bg_off();
        else bg_on();
    }
}

/* slot 0, $EC3B: the main game loop */
void ss2_main_loop(void)
{
    for (;;)
    {
        pause_check();
        main_body();
        if (ZP(0x14))                       /* $ED8C: no VBlank yet this pass */
        {
            if (ZP(0x6A)) ss2_wait(ZP(0x6A));
            ss2_wait(1);
            main_tail();
        }
        ZP(0x14) = 0x88;                    /* $EDB0 */
    }
}

/* $EDB0: re-enter the main loop with a fresh stack (end of a door) */
static void main_task_edb0(void)
{
    ZP(0x14) = 0x88;
    ss2_main_loop();
}

void ss2_main_restart(void)
{
    ss2_task_restart(main_task_edb0);
}

/* slot 1, $E2A6: during the intro ($6D) I or II skips to the game:
 * $E2B5 restarts task 0 at $F5AA and this task ends */
void ss2_game_listener(void)
{
    for (;;)
    {
        if (ZP(0x6D) && (Z_PRESS & 0x03))
        {
            ZP(0x6C) = 0x88;
            ZP(0x10) = 0xAA; ZP(0x11) = 0xF5;
            ss2_task_create(0, ss2_game_start_task);
            ss2_task_exit();                /* $E212 */
        }
        ss2_wait(1);
    }
}
#define listener_task ss2_game_listener

/* slot 3, $FD05: score task */
void ss2_score_task(void)
{
    for (;;)
    {
        score_task();
        ss2_wait(1);
    }
}

/* Start the gameplay tasks as they run during a stage. */
void ss2_game_start(void)
{
    u8 i;
    for (i = 0; i < 16; i++) ZP(i) = 0;
    ss2_task_create(0, ss2_main_loop);
    ss2_task_create(1, listener_task);
    ss2_task_create(3, ss2_score_task);
}

/* One frame of game logic after the VBlank part: run the ready tasks. */
void ss2_game_frame(void)
{
    ss2_sched_vblank();
    ss2_sched_run();
    PROF(16);
    build_sat();
    PROF(17);
}

/* Host replay: one main-loop pass plus the score task, as one frame does
 * when nothing blocks (the frame-pair traces start at $EC42). */
void ss2_game_tick(void)
{
    main_body();
    score_task();
    main_tail();
    build_sat();
}

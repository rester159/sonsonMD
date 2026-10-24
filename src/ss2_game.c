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


/* sprite table in the PCE SATB word layout: y, x, pattern, attribute */
u16 ss2_sat[64 * 4];
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
    if (!R(0x2455)) return;
    R(0x2455)--;
    d = (u16)(((ZP(0x4B) << 8) | ZP(0x4C)) - ((Z_CAMX_HI << 8) | Z_CAMX_LO));
    v = (u16)(((R(0x2456) << 8) | R(0x2457)) + d);
    R(0x2457) = (u8)v; R(0x2456) = (u8)(v >> 8);
    d = (u16)(((ZP(0x4E) << 8) | ZP(0x4F)) - ((Z_CAMY_HI << 8) | Z_CAMY_LO));
    v = (u16)(((R(0x2458) << 8) | R(0x2459)) + d);
    R(0x2459) = (u8)v; R(0x2458) = (u8)(v >> 8);
}

/* ---- sprite table (bank 3 $8732 entry 0, $8818 entries 2-6 and 16+) --- */

static u8 sat_i;

static void sat_put(u16 y, u16 x, u16 pat, u16 attr)
{
    u16 *e = &ss2_sat[(sat_i & 63) * 4];
    e[0] = y; e[1] = x; e[2] = pat; e[3] = attr;
    sat_i++;
}

/* $896A: one sprite per object */
static void sat_object(u8 x)
{
    u8 lo;
    if (O(0x245A, x) != 0x40 && !O(0x26BA, x) && ZP(0x71) && ZP(0x71) < 0x28 && !(ZP(0x71) & 4))
        return;
    ZP(0x38)++;
    lo = O(0x255A, x);
    if (O(0x283A, x)) lo |= 0x0F;
    sat_put((u16)((((O(0x24DA, x) << 8) | O(0x24FA, x)) + 0x40)),
            (u16)((((O(0x247A, x) << 8) | O(0x249A, x)) + 0x20)),
            (u16)((O(0x257A, x) << 8) | O(0x259A, x)),
            (u16)((O(0x253A, x) << 8) | lo));
}

/* $89D2: composite object: list of (dy, dx, frame, attr hi) */
static void sat_composite(u8 x)
{
    u16 list, an;
    u8 n, y;
    if (O(0x245A, x) != 0x40 && !O(0x26BA, x) && ZP(0x71) && ZP(0x71) < 0x28 && !(ZP(0x71) & 4))
        return;
    ss2_map6(O(0x275A, x));
    list = (u16)((O(0x257A, x) << 8) | O(0x259A, x));
    n = ss2_rom_byte((u16)(list + 1));
    if (!n) return;
    an = (u16)((O(0x27BA, x) << 8) | O(0x27DA, x));
    for (y = 2; n; n--)
    {
        u16 sy, sx, pat;
        u8 f, lo;
        sy = (u16)(((O(0x24DA, x) << 8) | O(0x24FA, x)) + (s8)ss2_rom_byte((u16)(list + y)) + 0x40);
        y++;
        sx = (u16)(((O(0x247A, x) << 8) | O(0x249A, x)) + (s8)ss2_rom_byte((u16)(list + y)) + 0x20);
        y++;
        f = ss2_rom_byte((u16)(list + y));
        if (f == 0xFF) pat = 0x0090;
        else
        {
            u16 t = (u16)(f + ss2_rom_byte((u16)(an + 1)));
            pat = (u16)((u8)t | ((u8)(ss2_rom_byte(an) + (t >> 8)) << 8));
        }
        y++;
        lo = O(0x255A, x);
        if (O(0x283A, x)) lo |= 0x0F;
        sat_put(sy, sx, pat, (u16)((ss2_rom_byte((u16)(list + y)) << 8) | lo));
        y++;
        ZP(0x38)++;
    }
}

static void build_sat(void)
{
    u8 x;
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
    /* entries 16+: front objects, player, rear objects */
    sat_i = 16;
    ZP(0x38) = 0;
    for (x = 0; x < 0x20; x++)
        if (O(0x245A, x) && !O(0x287A, x))
        {
            if (O(0x269A, x) & 0x20) sat_composite(x); else sat_object(x);
        }
    if (P_MODE)
    {
        u16 pat = 0x0200;
        if (P_HURT && !(P_HITOBJ | P_C4) && (P_HURT & 2)) pat = 0x0090;
        sat_put((u16)(((P_YH << 8) | P_YL) + 0x40), (u16)(((P_XH << 8) | P_XL) + 0x20), pat,
                (u16)((P_ATTRH << 8) | P_ATTRL));
        ZP(0x38)++;
    }
    for (x = 0; x < 0x20; x++)
        if (O(0x245A, x) && O(0x287A, x))
        {
            if (O(0x269A, x) & 0x20) sat_composite(x); else sat_object(x);
        }
    /* $8949: clear entries left over from a busier previous frame */
    if (ZP(0x39) > ZP(0x38))
    {
        u8 n = (u8)(ZP(0x39) - ZP(0x38));
        for (; n; n--) sat_put(0, 0, 0, 0);
    }
    ZP(0x39) = ZP(0x38);
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

static void hud_bars(void)
{
    if (P_LIFE < 5 && P_MODE != 0x40 && !(Z_FRAME & 0x1F)) ss2_sfx(0x18);
    ss2_hud_life[0] = R(0x22BA);
    ss2_hud_life[1] = R(0x22B9);
    R(0x22BA) = bar_step(R(0x22BA), P_LIFE);
    ss2_hud_magic[0] = R(0x22BD);
    ss2_hud_magic[1] = R(0x22BC);
    R(0x22BD) = bar_step(R(0x22BD), P_MAGIC);
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

#ifdef SS2_PROF
void ss2_prof_mark(u8 sec);
#define PROF(n) ss2_prof_mark(n)
#else
#define PROF(n)
#endif

/* $EC42-$EDA2: one pass of the main loop body (player .. respawn/HUD) */
static void main_body(void)
{
    u8 i;
    /* task 1 */
    for (i = 0; i < 6; i++) ZP(0x4B + i) = ZP(0x45 + i);
    ss2_map6(9);
    ss2_player_update();
    select_frame();
    play_clock();
    PROF(1);
    ss2_spawn_scan();
    PROF(2);
    /* task 2 */
    ss2_objects_update();
    PROF(3);
    trail();
    spark();
    if (!(Z_FRAME & 0x0F))
        for (i = 0x20; i < 0xBF; i++)
            if (R(0x2C1A + i) && R(0x2C1A + i) != 0xFF) R(0x2C1A + i)--;
    hud_bars();
}

/* $FDD9: pause on RUN while a game is running ($6C) */
static void pause_check(void)
{
    if (ZP(0x6C) && (Z_PRESS & 0x08))
    {
        ZP(0x6B) = Z_PRESS & 0x08;
        ss2_sfx(0x1A);
        do
        {
            ss2_wait(1);
            ss2_unported(0xFE0C);           /* magic selection while paused */
        } while (!(Z_PRESS & 0x08));
        ss2_sfx(0x1A);
    }
    ZP(0x6B) = 0;
    ZP(0x14) = 0x88;
}

/* slot 0, $EC3B: the main game loop */
static void main_task(void)
{
    for (;;)
    {
        pause_check();
        main_body();
        if (ZP(0x14))                       /* $ED8C: no VBlank yet this pass */
        {
            if (ZP(0x6A)) ss2_wait(ZP(0x6A));
            ss2_wait(1);
            ss2_hud_dirty |= 4;             /* $FE5A weapon/magic box (presentation) */
            /* $EDB7 palette flash and $74 display switch: presentation */
        }
        ZP(0x14) = 0x88;                    /* $EDB0 */
    }
}

/* slot 1, $E2A6: title RUN handler, idle during play */
static void listener_task(void)
{
    for (;;)
    {
        if (ZP(0x6D) && (Z_PRESS & 0x03)) ss2_unported(0xE2B5);
        ss2_wait(1);
    }
}

/* slot 3, $FD05: score task */
static void score_task_loop(void)
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
    ss2_task_create(0, main_task);
    ss2_task_create(1, listener_task);
    ss2_task_create(3, score_task_loop);
}

/* One frame of game logic after the VBlank part: run the ready tasks. */
void ss2_game_frame(void)
{
    ss2_sched_vblank();
    ss2_sched_run();
    build_sat();
}

/* Host replay: one main-loop pass plus the score task, as one frame does
 * when nothing blocks (the frame-pair traces start at $EC42). */
void ss2_game_tick(void)
{
    main_body();
    score_task();
    build_sat();
}

/* SON SON II game flow, rebuilt from the HuCard: power-on (bank 0 $E000,
 * $E2D0), the title ($E330) with its listener ($E241), the attract demo
 * ($E61E -> $F5AA with $C7, demo pad bank 4 $BD16, end $F884), the intro
 * (bank 2 $63D8), the new game ($F5AA), the stage start ($F9F0) and the
 * return to the title ($FB97).
 *
 * Every step is a task as on the HuCard: slot 0 runs title / intro / game,
 * slot 1 the button listeners, slot 3 the score task. BAT/VRAM work is
 * presentation and goes to ss2_bat_queue / ss2_scene for the renderer;
 * RAM effects and waits follow the source.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"
#include "ss2_task.h"

u8 ss2_scene;
u8 ss2_pal_intro;
u8 ss2_stage_request;                        /* renderer: load this stage's graphics */                            /* sprite palettes 8-15 = $4A6C (intro) */

static u16 w16(u16 a)
{
    return (u16)(ss2_rom_byte(a) | (ss2_rom_byte((u16)(a + 1)) << 8));
}

static void zero(u16 a, u16 n)
{
    while (n--) R(a++) = 0;
}

/* one object update; returns the X the source continues from */
static u8 upd(u8 x)
{
    ss2_obj_camera();
    ss2_obj_next = x;
    ss2_obj_update(x);
    return ss2_obj_next;
}

static void upd_range(u8 n)
{
    u8 x;
    for (x = 0; x < n; x = (u8)(upd(x) + 1)) { }
}

/* $B9D8 x n ($B9E4): n seconds */
static void seconds(u8 n)
{
    while (n--) ss2_wait(0x3C);
}

/* $BAAF (SATB cleared: the native table is rebuilt from RAM) and $797E
 * (VRAM cleared): only the VDC register shadow */
static void vram_clear(void)
{
    ZP(0x8B) = 0x02;
}

/* bank 2 $7882: the stage's BG blocks into VRAM ($548F list of bank,
 * blocks, address); RAM: the last bank stays mapped, $2FA0 counts to 0 */
static void blocks_stage(void)
{
    u16 l = w16((u16)(0x548F + (u8)(Z_STAGE << 1)));
    u8 b;
    while ((b = ss2_rom_byte(l)) != 0xFF)
    {
        ss2_page[6] = ss2_banks[b & 0x1F];
        ss2_mpr6 = b;
        l = (u16)(l + 4);
    }
    R(0x2FA0) = 0;
    ZP(0x8B) = 0x02;
    ss2_scene = SS2_SCENE_PLAY;
}

/* bank 2 $7ADE: the revive picture's blocks (bank $1F) */
static void blocks_continue(void)
{
    R(0x2FA0) = 0;
    ZP(0x8B) = 0x02;
    ss2_page[6] = ss2_banks[ZP(0x8E) & 0x1F];
    ss2_mpr6 = ZP(0x8E);
    ss2_scene = SS2_SCENE_CONTINUE;
}

/* bank 2 $7B02: the title picture's blocks (bank $1E); MPR6 back to $8E */
static void blocks_title(void)
{
    R(0x2FA0) = 0;
    ZP(0x8B) = 0x02;
    ss2_page[6] = ss2_banks[ZP(0x8E) & 0x1F];
    ss2_mpr6 = ZP(0x8E);
    ss2_scene = SS2_SCENE_TITLE;
}

/* $763A: the HUD frame ($771B clears BAT rows 0-6, $BADE $4CF0 draws it);
 * the Genesis HUD is the window plane, drawn from RAM */
static void hud_frame(void)
{
    ZP(0x8B) = 0x02;
}

/* $BB99 / $BBC2: score and money digits */
static void hud_digits(void)
{
    ss2_hud_dirty |= 3;
    ZP(0x8B) = 0x02;
}

/* $EBC4: the stage's metatile base, the camera at (0, $700), the redraw */
static void stage_redraw(void)
{
    u16 m = w16((u16)(0xEC2D + (u8)(Z_STAGE << 1)));
    ZP(0x5D) = (u8)m; ZP(0x5E) = (u8)(m >> 8);
    ZP(0x45) = 0; ZP(0x46) = 0; ZP(0x47) = 0;
    ZP(0x48) = 7; ZP(0x49) = 0; ZP(0x4A) = 0;
    ss2_redraw_request = 1;                  /* $EBE1 */
    ss2_busy(5);
    ZP(0x8B) = 0x02;                         /* its BAT writes */
    ss2_page[6] = ss2_banks[ZP(0x8E) & 0x1F];
    ss2_mpr6 = ZP(0x8E);
}

/* $FAC1 / $63F5: the stage map ($C000 + (stage & 1) * $1000 of bank
 * $0C | stage >> 1) into work RAM $3000 */
static void map_copy(u8 bank, u16 src)
{
    u16 i;
    ss2_map6(bank);
    for (i = 0; i < 0x1000; i++) R(0x3000 + i) = ss2_rom_byte((u16)(src + i));
}

/* $E393-$E3D0: the high score ($2FF9, 7 digits, leading zeros blank) at
 * BAT $0590 */
static void hiscore(void)
{
    u8 y, lead = 1;
    for (y = 0; y < 7; y++)
    {
        u8 d = R(0x2FF9 + y);
        if (lead && !d) { ss2_bat_put((u8)(0x10 + y), 0x16, 0xF1A0); continue; }
        lead = 0;
        ss2_bat_put((u8)(0x10 + y), 0x16, (u16)(0xF1B0 + d));
    }
    ZP(0x30) = (u8)!lead;
}

/* ---- Genesis addition: the title menu (GAME START / OPTIONS) -----------
 * Not in the HuCard: after START on the title, a choice between the game
 * and an OPTIONS screen with a music selector. Drawn with the title's own
 * font (palette 15 BAT words) over its text rows; the title animation goes
 * on, but does not roll into the attract demo while the menu is up. */
u8 ss2_menu_active;

static const char *const track_name[0x15] = {
    "TITLE", "SHOP", "CONTINUE", "STAGE 1", "STAGE 2", "STAGE 3", "STAGE 4",
    "STAGE 5", "STAGE 6", "STAGE 7", "ROOM A", "ROOM B", "ROOM C", "ROOM D",
    "EXTRA", "MISS", "ENDING 1", "INTRO", "STAGE CLEAR", "ENDING 2", "ENDING 3" };

/* the font has no < or >: '/' ($2F, the text tables' line break) is a
 * right-pointing triangle, used as the cursor */
static void menu_text(u8 col, u8 row, const char *s, u8 width)
{
    u8 i = 0;
    for (; s[i] && i < width; i++) ss2_bat_put((u8)(col + i), row, (u16)(0xF180 + (u8)s[i]));
    for (; i < width; i++) ss2_bat_put((u8)(col + i), row, 0xF1A0);
}

static void menu_blank_rows(void)
{
    u8 r;
    for (r = 0x14; r < 0x1C; r++) menu_text(4, r, "", 24);
}

static u8 pressed(u8 mask)
{
    return (u8)(Z_PRESS & mask);
}

/* returns 1 for GAME START */
static u8 title_menu(void)
{
    u8 sel = 0;
    menu_text(6, 0x14, "", 20);
    menu_text(6, 0x16, "", 20);              /* the HISCORE line, while the menu is up */
    for (;;)
    {
        menu_text(9, 0x14, sel ? "  GAME START" : "/ GAME START", 12);
        menu_text(9, 0x16, sel ? "/ OPTIONS" : "  OPTIONS", 12);
        do ss2_wait(1); while (!Z_PRESS);
        if (pressed(0x50)) sel ^= 1;                 /* Up / Down */
        if (pressed(0x0B))                           /* START, A, B */
        {
            if (!sel) return 1;
            return 0;
        }
    }
}

static void options_screen(void)
{
    u8 sel = 0, track = 0, lang = ss2_language;   /* sel: 0 MUSIC, 1 LANGUAGE, 2 EXIT */
    menu_blank_rows();
    menu_text(10, 0x14, "OPTIONS", 7);
    for (;;)
    {
        char buf[24];
        u8 i = 0;
        const char *n = track_name[track];
        buf[i++] = sel ? ' ' : '/';
        buf[i++] = ' ';
        { const char *m = "MUSIC  "; while (*m) buf[i++] = *m++; }
        buf[i++] = "0123456789ABCDEF"[track >> 4];
        buf[i++] = "0123456789ABCDEF"[track & 15];
        buf[i++] = ' ';
        buf[i++] = ' ';
        while (*n && i < 23) buf[i++] = *n++;
        buf[i] = 0;
        menu_text(4, 0x17, buf, 24);
        menu_text(4, 0x19, sel == 1 ? "/ LANGUAGE " : "  LANGUAGE ", 11);
        menu_text(15, 0x19, lang == SS2_LANG_JAPANESE ? "JAPANESE" : "ENGLISH", 8);
        menu_text(4, 0x1B, sel == 2 ? "/ EXIT" : "  EXIT", 6);
        do ss2_wait(1); while (!Z_PRESS);
        if (pressed(0x10)) sel = (u8)(sel ? sel - 1 : 2);            /* Up */
        if (pressed(0x40)) sel = (u8)(sel == 2 ? 0 : sel + 1);       /* Down */
        if (sel == 1 && pressed(0xA3)) lang ^= 1;                    /* Left, Right, A, B */
        if (!sel && pressed(0x20)) track = (u8)(track == 0x14 ? 0 : track + 1);   /* Right */
        if (!sel && pressed(0x80)) track = (u8)(track ? track - 1 : 0x14);        /* Left */
        if (!sel && pressed(0x03)) { ss2_sfx(0xF0); ss2_sfx(track); }             /* A, B: play */
        if (!sel && pressed(0x04)) ss2_sfx(0xF0);                                 /* C: stop */
        if ((sel == 2 && pressed(0x0B)) || (sel != 2 && pressed(0x08)))          /* leave */
            break;
    }
    if (lang != ss2_language)
    {
        ss2_set_language(lang);
        ss2_options_save();                  /* cartridge SRAM */
    }
    ss2_sfx(0xF0);
    menu_blank_rows();
    ss2_bat_text(0xE7F7);                    /* the title's own text back */
    hiscore();
    ss2_sfx(0x00);                           /* title music */
}

/* ---- slot 1 listeners -------------------------------------------------- */

/* $E224: during the attract demo RUN ends it ($F884) */
static void demo_listener(void)
{
    do ss2_wait(1); while (!(Z_PRESS & 0x08));
    ZP(0x10) = 0x84; ZP(0x11) = 0xF8;
    ss2_task_create(0, ss2_death_task);
    ss2_task_exit();
}

static void intro_task(void);

/* $E241: the title waits for RUN; held buttons pick a mode ($89), and the
 * code Up Down Left Right I II ($E2C9) sets $8A; then the intro starts
 * and this task goes on as the game listener $E2A6 */
static void title_listener(void)
{
    u8 x;
restart:
    ZP(0x8A) = 0;
    x = 0;
    for (;;)                                 /* $E248: wait for a release */
    {
        ss2_wait(1);
        if (!Z_PAD) break;
        if (Z_PAD & 0x08) goto run;
    }
    for (;;)                                 /* $E255: the next press */
    {
        ss2_wait(1);
        if (!Z_PAD) continue;
        if (Z_PAD & 0x08) goto run;
        if (Z_PAD != ss2_rom_byte((u16)(0xE2C9 + x))) goto restart;
        x++;
        if (ss2_rom_byte((u16)(0xE2C9 + x)) != 0xFF)
        {
            for (;;)                         /* back to $E248 */
            {
                ss2_wait(1);
                if (!Z_PAD) break;
                if (Z_PAD & 0x08) goto run;
            }
            continue;
        }
        ZP(0x8A) = 0xFF;
        do ss2_wait(1); while (!(Z_PRESS & 0x08));
        break;
    }
run:                                         /* $E278 */
    ss2_menu_active = 1;                     /* Genesis: GAME START / OPTIONS */
    while (!title_menu())
    {
        options_screen();
        do ss2_wait(1); while (Z_PAD);
    }
    ss2_menu_active = 0;
    ZP(0x89) = 0;
    if (Z_PAD == 0x1B) { /* $0400 = $84: VCE (presentation) */ }
    else if (Z_PAD == 0x1D) ZP(0x89) = 1;
    else if (Z_PAD == 0x1E) ZP(0x89) = 2;
    ZP(0x10) = 0xD8; ZP(0x11) = 0x63;
    ss2_task_create(0, intro_task);
    ss2_game_listener();                     /* falls into $E2A6 */
}

/* ---- slot 0: the title ($E330) ----------------------------------------- */

static void title_task(void)
{
    ss2_sfx(0xF0);                           /* $BBEB */
    ss2_spr_off();
    ss2_bg_off();
    ss2_fade_out_all();                      /* $7297 */
    ZP(0x6E) = 0;
    ss2_cr_set((u8)(R(0x2201) & 0xFB));      /* raster split off: full screen */
    ss2_bat_clear();                         /* $BA91 */
    vram_clear();                            /* $BAAF */
    blocks_title();                          /* $7B02 */
    ss2_busy(1);                             /* (measured: VBlanks during) */
    ss2_sprset_5548();                       /* $77D0 */
    ss2_busy(4);
    zero(0x2045, 0x12);                      /* $45-$56 */
    ss2_bat_picture(0x0000, 0xE6D7, 0x20, 0x12);   /* $FBB1 */
    ss2_bat_text(0xE7F7);                    /* PUSH RUN BUTTON etc. */
    hiscore();
    ss2_spr_on();
    ss2_bg_on();
    ss2_fade_in_title();                     /* $722E */
    ss2_sfx(0x00);                           /* title music */
    Z_STAGE = 0;
    ZP(0x6C) = 0;
    ZP(0x10) = 0x41; ZP(0x11) = 0xE2;
    ss2_task_create(1, title_listener);
    seconds(3);                              /* $B9E4, 3 */
    /* the logo's clouds drop in (slots $1E, $1F) */
    ss2_obj_init(0x1F, 0xE64D);
    ss2_obj_init(0x1E, 0xE64D);
    R(0x24B9) = R(0x24B8) = 0x50;
    R(0x2499) = R(0x2498) = 0;
    R(0x2519) = R(0x2518) = 0xE0;
    R(0x24F9) = R(0x24F8) = 0xFF;
    for (;;)                                 /* $E430 */
    {
        u16 t;
        upd((u8)(upd(0x1E) + 1));
        ss2_sat_update();
        t = (u16)(R(0x2638) + 0x20);
        R(0x2638) = R(0x2639) = (u8)t;
        R(0x2618) = R(0x2619) = (u8)(R(0x2618) + (t >> 8));
        if (!R(0x24F8) && R(0x2518) >= 0x30) break;
        ss2_wait(1);
    }
    R(0x2639) = 0; R(0x2619) = 0;
    R(0x25F8) = 0x40; R(0x25D8) = 1;
    R(0x2638) = 0; R(0x2618) = 0xFD;
    for (;;)                                 /* $E47E */
    {
        u16 t;
        upd((u8)(upd(0x1E) + 1));
        ss2_sat_update();
        t = (u16)(R(0x2638) + 0x20);
        R(0x2638) = (u8)t;
        R(0x2618) = (u8)(R(0x2618) + (t >> 8));
        if (R(0x2518) >= 0x50) break;
        ss2_wait(1);
    }
    R(0x25F8) = 0; R(0x25D8) = 0;
    R(0x2638) = 0; R(0x2618) = 0xFD;
    for (;;)                                 /* $E4B7 */
    {
        u16 t;
        upd(0x1E);
        ss2_sat_update();
        t = (u16)(R(0x2638) + 0x40);
        R(0x2638) = (u8)t;
        R(0x2618) = (u8)(R(0x2618) + (t >> 8));
        if (!(R(0x2618) & 0x80) && R(0x2518) >= 0x50) break;
        ss2_wait(1);
    }
    R(0x25F8) = R(0x25D8) = R(0x2638) = R(0x2618) = 0;
    seconds(2);
    /* the two halves of the logo */
    ss2_obj_init(1, 0xE621);
    R(0x249B) = 0; R(0x247B) = 1; R(0x24FB) = 0x60; R(0x24DB) = 0;
    R(0x25DB) = 0x40; R(0x25BB) = 0xFB;
    ss2_obj_init(0, 0xE637);
    R(0x249A) = 0; R(0x247A) = 1; R(0x24FA) = 0x7C; R(0x24DA) = 0;
    R(0x25DA) = 0; R(0x25BA) = 0xFC;
    for (;;)                                 /* $E543 */
    {
        u16 t;
        upd(0);
        upd(1);
        ss2_sat_update();
        t = (u16)(R(0x25DB) + 0x10);
        R(0x25DB) = R(0x25DA) = (u8)t;
        R(0x25BB) = R(0x25BA) = (u8)(R(0x25BB) + (t >> 8));
        if (R(0x249B) < 0x50) break;
        ss2_wait(1);
    }
    R(0x245A) = 0;
    R(0x267B) = 1; R(0x295B) = 0x68; R(0x293B) = 0xE6; R(0x255B) = 0x84;
    R(0x25BB) = R(0x25DB) = R(0x25FB) = R(0x261B) = 0;
    R(0x297B) = 0;
    for (;;)                                 /* $E598 */
    {
        if (R(0x297B)++ == 0x20) break;
        upd(1);
        ss2_sat_update();
        ss2_wait(1);
    }
    R(0x267B) = 1; R(0x255B) = 0x89; R(0x295B) = 0x70; R(0x293B) = 0xE6;
    R(0x27DB) = 0xDE; R(0x27BB) = 0x2D;
    {
        u8 y;
        u16 n;
        for (y = 5; y; y--)                  /* $E5CD: 5 x 256 frames */
            for (n = 0; n < 0x100; n++)
            {
                upd(1);
                ss2_sat_update();
                ss2_wait(1);
            }
    }
    while (ss2_menu_active) ss2_wait(1);     /* Genesis: no demo under the menu */
    ss2_fade_out_title();                    /* $7251 */
    ss2_wait(30);                            /* $B9D4 */
    ss2_cr_set((u8)(R(0x2201) | 0x04));      /* raster split on */
    /* the attract demo */
    ZP(0xC6) = 1; ZP(0xC7) = 1;
    ZP(0x8A) = 0;
    ZP(0x10) = 0x24; ZP(0x11) = 0xE2;
    ss2_task_create(1, demo_listener);
    ZP(0xC4) = 0x69; ZP(0xC5) = 0xBD;
    ss2_task_restart(ss2_game_start_task);   /* JMP $F5AA (LDX #$30 / TXS) */
}

/* ---- the stage start ($F9F0) ------------------------------------------- */

static void stage_init(void)
{
    u8 i, g;
    u16 t;
    ss2_bg_off();
    zero(0x245A, 0x700);
    zero(0x2B5A, 0x240);
    zero(0x2297, 0x1B0);
    ZP(0x66) = ZP(0x67) = ZP(0x68) = ZP(0xBA) = ZP(0x7D) = 0;
    ss2_sprset_stage();                      /* $774F */
    ss2_busy(3);
    blocks_stage();                          /* $7882 */
    ss2_busy(3);
    vram_clear();                            /* $797E */
    /* the stage's 32 spawn-order bytes, shuffled in groups of 8 */
    t = (u16)(0x4EBC + (Z_STAGE << 5));
    ZP(0x26) = (u8)t; ZP(0x27) = (u8)(t >> 8);
    for (i = 0; i < 0x20; i++) R(0x2223 + i) = ss2_rom_byte((u16)(t + i));
    for (g = 0; g < 4; g++)
    {
        u16 b = (u16)(0x2223 + g * 8);
        for (i = 8; i; i--)
        {
            u8 y1, y2, v1, v2;
            ss2_random();
            y1 = (u8)(ZP(0x8D) & 7);
            v1 = R(b + y1);
            ss2_random();
            y2 = (u8)(ZP(0x8D) & 7);
            v2 = R(b + y2);
            R(b + y2) = v1;
            R(b + y1) = v2;
        }
    }
    ss2_map6(9);
    for (i = 0; i < 0x40; i++) R(0x2297 + i) = ss2_bank_byte(9, (u16)(0xD9FB + i));
    R(0x22C1) = ZP(0x77);
    R(0x22BF) = ZP(0x78);
    R(0x22B8) = R(0x22BA) = ZP(0x83);
    R(0x22B9) = ZP(0x82);
    R(0x22BB) = R(0x22BD) = ZP(0x85);
    R(0x22BC) = ZP(0x84);
    map_copy((u8)((Z_STAGE >> 1) | 0x0C), (u16)(0xC000 + ((Z_STAGE & 1) << 12)));
    ss2_busy(1);
    ss2_bat_clear();
    vram_clear();
    hud_frame();
    ss2_hud_bars();                          /* $7649, $7688 */
    hud_digits();
    R(0x22C0) = 0;
    ss2_weapon_box();                        /* $FE5A */
    {
        u8 a = R(0x22C1);
        s8 x;
        for (x = 0x0C; x >= 0; x = (s8)(x - 2))
        {
            u8 c = (u8)(a & 0x80);
            a = (u8)(a << 1);
            if (c) ss2_hud_text(w16((u16)(0xFC5C + x)));
        }
    }
    if (ZP(0x76) == 1) ss2_hud_text(0x4E62);
    ss2_staff_art();                         /* $79F7 */
    ss2_stage_request = 1;
    ss2_vce_reset();                         /* $FF46 */
    ss2_busy(1);
    ss2_bg_on();
    ss2_sfx((u8)(3 + Z_STAGE));
    if (!ZP(0xC7))
    {
        ss2_bat_text(0xFCCC);                /* STAGE */
        ss2_bat_put(0x11, 0x10, (u16)(0x51B1 + Z_STAGE));
    }
    seconds(1);                              /* $B9D8 */
    ss2_fade_out();                          /* $71D7 */
    stage_redraw();                          /* $EBC4 */
    ss2_fade_in();                           /* $71BA */
    ZP(0x64) = 0;
    if (ZP(0xC7))
    {
        Z_FRAME = 0xB4;
        ZP(0x8C) = 0x19;
        ZP(0x8D) = 0x0D;                     /* the demo's fixed seed */
    }
    ss2_main_loop();                         /* JMP $EC3B */
}

/* ---- slot 0: a new game ($F5AA) ----------------------------------------- */

void ss2_game_start_task(void)
{
    u8 i;
    ZP(0x6D) = 0;
    ZP(0x74) = 0;
    for (i = 0; i < 0x20; i++) ZP(0x98 + i) = ss2_rom_byte((u16)(0xFC6A + i));
    zero(0x2203, 8);
    zero(0x220B, 8);
    ZP(0x78) = 1;
    ZP(0x77) = 0;
    ZP(0x72) = 0;
    ZP(0x76) = 0;
    ZP(0x7A) = ZP(0x7B) = ZP(0x7C) = 0;
    ZP(0x82) = ZP(0x83) = 0x0E;
    ZP(0x84) = ZP(0x85) = 0x0C;
    ZP(0x80) = 0xB2; ZP(0x81) = 0x4C;
    zero(0x2045, 6);
    if (ZP(0x89) == 1 || ZP(0x89) == 2)
    {
        ZP(0x77) = ZP(0x89) == 1 ? 4 : 2;
        ZP(0x89) = 0;
    }
    stage_init();                            /* JMP $F9F0 */
}

/* ---- slot 0: the intro (bank 2 $63D8) ----------------------------------- */

static void intro_actor(u8 x, u16 rec, u8 attr, u16 pat, u16 scr, u8 xh, u8 xl, u8 yl)
{
    ss2_obj_init(x, rec);
    if (attr) O(0x255A, x) = attr;
    O(0x27DA, x) = (u8)pat; O(0x27BA, x) = (u8)(pat >> 8);
    O(0x295A, x) = (u8)scr; O(0x293A, x) = (u8)(scr >> 8);
    O(0x247A, x) = xh; O(0x249A, x) = xl;
    O(0x24DA, x) = 0; O(0x24FA, x) = yl;
}

static void intro_task(void)
{
    ss2_sfx(0xF0);
    ss2_fade_out_title();                    /* $7251 */
    ss2_bat_clear();
    vram_clear();
    zero(0x245A, 0x700);
    blocks_stage();                          /* $7882 (stage 1) */
    ss2_busy(3);
    ss2_stage_request = 1;
    map_copy(0x0C, 0xC000);
    stage_redraw();                          /* $EBC4 */
    /* five villagers and the shadow */
    intro_actor(0, 0x6701, 0x81, 0x2DB6, 0x672D, 0, 0x10, 0xA0);
    intro_actor(1, 0x6701, 0x86, 0x2DB6, 0x6732, 0, 0x30, 0xA0);
    intro_actor(2, 0x6701, 0x80, 0x2DDC, 0x6737, 0, 0x50, 0xA0);
    intro_actor(3, 0x6701, 0x86, 0x2DDE, 0x673C, 0, 0x70, 0xA0);
    intro_actor(4, 0x6701, 0x83, 0x2DDE, 0x6741, 1, 0x28, 0x80);
    intro_actor(5, 0x6717, 0, 0x2DE0, 0x67E0, 1, 0x38, 0x90);
    R(0x22B8) = R(0x22BA) = R(0x22B9) = 0x0E;
    R(0x22BB) = R(0x22BD) = R(0x22BC) = 0x0C;
    zero(0x2203, 8);
    zero(0x220B, 8);
    hud_frame();
    ss2_hud_bars();
    hud_digits();
    R(0x22C0) = 0;
    ss2_weapon_box();
    ss2_cr_set((u8)(R(0x2201) | 0x04));      /* raster split on */
    ZP(0x6D) = 0x88;
    upd_range(6);
    ss2_sat_update();
    ss2_sfx(0x11);                           /* intro music */
    ss2_fade_late = 1;                       /* starts on line 226 */
    ss2_fade_in_all();                       /* $7274 */
    {
        u8 i;                                /* $FF8E: sprite palettes 8-15 from $4A6C */
        for (i = 0; i < 0x40; i++)
            ss2_vce[0x180 + (i >> 3) * 16 + (i & 7)] = (u16)(w16((u16)(0x4A6C + i * 2)) & 0x1FF);
        ss2_vce_dirty = 1;
        ss2_pal_intro = 1;
        ZP(0x28) = 0x6C; ZP(0x29) = 0x4A; ZP(0x30) = 0x80;
    }
    R(0x255D) = 0x88;
    ss2_sat_update();
    seconds(1);
    R(0x25DE) = R(0x25DF) = 0xC0;
    R(0x25BE) = R(0x25BF) = 0xFF;
    Z_FRAME = 0;
    for (;;)                                 /* $65FC: the giant walks in */
    {
        Z_CAMY_LO = 0;
        if (!(Z_FRAME & 0x1F))
        {
            Z_CAMY_LO = 4;                   /* the ground shakes */
            ss2_sfx(0x2A);
        }
        if (R(0x249F) == 0xC8) break;
        upd_range(6);
        ss2_sat_update();
        ZP(0x4F) = Z_CAMY_LO;
        ss2_wait(1);
    }
    ss2_sfx(0xF1);
    seconds(1);
    R(0x25BE) = R(0x25DE) = R(0x25BF) = R(0x25DF) = 0;
    R(0x267F) = 1; R(0x295F) = 0xF3; R(0x293F) = 0x68;
    ss2_sfx(0x25);
    ZP(0x74) = 1;
    ZP(0x75) = 0;
    for (;;)                                 /* $6655: lightning */
    {
        upd_range(6);
        ss2_sat_update();
        if (!ZP(0x74)) break;
        for (;;)
        {
            ss2_map6(9);
            ss2_magic_sweep();               /* $D7BC */
            ss2_wait(1);
            if (ZP(0x74) & 0x40) { ss2_bg_off(); continue; }
            ss2_bg_on();
            break;
        }
    }
    R(0x267A) = 1; R(0x295A) = 0x46; R(0x293A) = 0x67;
    R(0x267B) = 1; R(0x295B) = 0x7F; R(0x293B) = 0x67;
    R(0x267C) = 1; R(0x295C) = 0xD9; R(0x293C) = 0x67;
    R(0x267D) = 1; R(0x295D) = 0xB0; R(0x293D) = 0x67;
    while (R(0x245A) | R(0x245B) | R(0x245D))     /* $66BC: everyone runs off */
    {
        u8 x;
        for (x = 0; x < 6; x++)
            if (O(0x245A, x)) x = upd(x);
        ss2_sat_update();
        ss2_wait(1);
    }
    R(0x297C) = 0;
    while (++R(0x297C) != 0x3C)
    {
        upd(2);
        ss2_sat_update();
        ss2_wait(1);
    }
    ss2_wait(30);                            /* $B9D4 */
    ZP(0x6C) = 0x88;
    ss2_pal_intro = 0;
    ss2_task_restart(ss2_game_start_task);   /* JMP $F5AA */
}

/* ---- slot 0: the stage clear tally ($F617) ------------------------------ */

/* 7 BCD digits at d (index 1-7) += the table at t (index 1-7) */
static void bcd_add7(u16 d, u16 t)
{
    u8 i, c = 0;
    for (i = 7; i; i--)
    {
        u8 a = (u8)(ss2_rom_byte((u16)(t + i)) + R(d + i) + c);
        c = (u8)(a >= 10);
        if (c) a = (u8)(a - 10);
        R(d + i) = a;
    }
}

/* digits 3-7 at d, leading zeros skipped, from BAT address a (palette 5) */
static void bat_digits(u16 a, u16 d)
{
    u8 x = 3;
    while (x < 8 && !R(d + x)) x++;
    for (; x < 8; x++, a++) ss2_bat_put((u8)(a & 63), (u8)(a >> 6), (u16)(0x51B0 + R(d + x)));
}

/* a BCD byte as two digits */
static u16 bat_bcd(u16 a, u8 v)
{
    ss2_bat_put((u8)(a & 63), (u8)(a >> 6), (u16)(0x51B0 + (v >> 4)));
    a++;
    ss2_bat_put((u8)(a & 63), (u8)(a >> 6), (u16)(0x51B0 + (v & 15)));
    return (u16)(a + 1);
}

/* bank 4 $AD5A: the ending (src/ss2_ending.c); a weak stand-in until then */
__attribute__((weak)) void ss2_ending_task(void)
{
    ss2_unported(0xAD5A);
    for (;;) ss2_wait(1);
}
#define ending_task ss2_ending_task

static void stage_clear_task(void)
{
    u16 t;
    if ((u8)(Z_STAGE + 1) == 7) ss2_task_restart(ending_task);     /* JMP bank 4 $AD5A */
    ss2_fade_out();                          /* $71D7 */
    ss2_bat_clear();                         /* $A649 */
    vram_clear();
    ss2_sfx(0xF0);
    ss2_wait(30);
    zero(0x2045, 6);
    ss2_bat_text(0xFC8A);                    /* BONUS SCORE / LIFE x 500 */
    ss2_bat_text(0xFCA9);                    /* BONUS ZENNY / TIME */
    ss2_sfx(0x12);                           /* stage clear */
    ss2_fade_in();                           /* $71BA */
    zero(0x2213, 0x10);
    ZP(0x83) = R(0x22B8);
    ss2_sfx(0x1B);
    for (;;)                                 /* $F66F: 500 points per life unit */
    {
        R(0x22BA)--;
        if ((s8)--R(0x22B8) < 0) break;
        ss2_hud_life_bar();
        bcd_add7(0x2213, 0x4B1C);
        bat_digits(0x0355, 0x2213);
        ss2_wait(3);                         /* $B9C0 */
    }
    ss2_sfx(0xF1);
    t = bat_bcd(0x04CD, ZP(0x66));           /* the time: minutes */
    ss2_bat_put((u8)(t & 63), (u8)(t >> 6), 0x520E);
    t = bat_bcd((u16)(t + 1), ZP(0x67));     /* seconds */
    ss2_bat_put((u8)(t & 63), (u8)(t >> 6), 0x01A0);
    t++;
    ss2_bat_put((u8)(t & 63), (u8)(t >> 6), 0x520D);
    if (ZP(0x66) >= 0x0A) ss2_bat_text(0xFCC0);      /* NO BONUS */
    else
    {
        u16 tab = w16((u16)(0xFCD5 + (u8)(Z_STAGE << 1)));
        u8 x = ss2_rom_byte((u16)(tab + ZP(0x66)));
        ss2_sfx(0x1B);
        do
        {
            u8 y;
            for (y = 0x0A; y; y--)
            {
                bcd_add7(0x221B, 0x4BFC);
                bat_digits(0x04D5, 0x221B);
                ss2_wait(1);
            }
        } while (--x);
    }
    ss2_sfx(0xF1);
    seconds(1);
    {
        u8 i, c = 0;                         /* score += bonus, money += zenny */
        for (i = 7; i; i--)
        {
            u8 a = (u8)(R(0x2203 + i) + R(0x2213 + i) + c);
            c = (u8)(a >= 10); if (c) a = (u8)(a - 10);
            R(0x2203 + i) = a;
        }
        hud_digits();
        c = 0;
        for (i = 7; i; i--)
        {
            u8 a = (u8)(R(0x220B + i) + R(0x221B + i) + c);
            c = (u8)(a >= 10); if (c) a = (u8)(a - 10);
            R(0x220B + i) = a;
        }
        hud_digits();
    }
    ss2_score_award(0);                      /* $BB89 */
    ss2_wait(2);
    ss2_hud_magic_bar();                     /* $7688 */
    seconds(1);
    R(0x22BA) = 0;
    {
        u8 a = (u8)(ZP(0x83) + 5);
        R(0x22B8) = a < R(0x22B9) ? a : R(0x22B9);
        a = (u8)(R(0x22BB) + 5);
        R(0x22BB) = a < R(0x22BC) ? a : R(0x22BC);
    }
    ss2_sfx(0xF1);
    ss2_wait(30);
    if (R(0x22B8) != R(0x22BA))              /* the life bar fills */
    {
        u8 ne;
        ss2_sfx(0x20);
        do
        {
            ne = (u8)(R(0x22B8) != R(0x22BA));
            ss2_wait(1);
            ss2_hud_life_bar();
        } while (ne);
    }
    ss2_sfx(0xF1);
    ss2_wait(30);
    if (R(0x22BB) != R(0x22BD))              /* the magic bar fills */
    {
        u8 ne;
        ss2_sfx(0x20);
        do
        {
            ne = (u8)(R(0x22BB) != R(0x22BD));
            ss2_wait(1);
            ss2_hud_magic_bar();
        } while (ne);
    }
    ss2_sfx(0xF1);
    seconds(1);
    ss2_fade_out_all();                      /* $7297 */
    Z_STAGE++;
    ZP(0x83) = R(0x22B8);
    ZP(0x82) = R(0x22B9);
    ZP(0x85) = R(0x22BB);
    ZP(0x84) = R(0x22BC);
    ZP(0x77) = R(0x22C1);
    ZP(0x78) = R(0x22BF);
    stage_init();                            /* JMP $F9F0 */
}

void ss2_stage_clear_start(void)
{
    ZP(0x10) = 0x17; ZP(0x11) = 0xF6;
    ss2_task_create(0, stage_clear_task);
}

/* ---- slot 0: back to the title ($FB97) ---------------------------------- */

void ss2_title_return(void)
{
    ZP(0x6C) = 0;
    Z_STAGE = 0;
    ss2_sfx(0xF0);
    zero(0x2297, 0x40);
    ss2_fade_out_all();                      /* $7297 */
    vram_clear();
    ss2_task_restart(title_task);            /* JMP $E330 (LDX #$30 / TXS) */
}

/* slot 0, $F884: after a miss (and at the demo's end) */
void ss2_death_task(void)
{
    ss2_fade_out();                          /* $71D7 */
    ss2_bat_clear();                         /* $A649 */
    ZP(0x04 * 2) = 0; ZP(0x04 * 2 + 1) = 0;  /* $E1F3: task 2 ends */
    zero(0x2045, 6);
    zero(0x22E5, 0xE0);
    zero(0x23C5, 0x18);
    zero(0x245A, 0x700);
    ss2_sat_update();
    ZP(0x83) = ZP(0x82) = R(0x22B9);
    ZP(0x85) = ZP(0x84) = R(0x22BC);
    if (ZP(0xC7))
    {
        ZP(0xC7) = 0;
        ZP(0x6C) = 0;
        Z_STAGE = 0;
        ss2_title_return();                      /* JMP $FB97 */
    }
    if (!(R(0x22C1) & 0x04))
    {
        /* $F8D8: no revive charm: the last words, then the title */
        ss2_sfx(0xF0);
        ss2_bat_box(0x028A, 0xA7C6);         /* $A4CA */
        ss2_fade_in_alt();                   /* $71F4 */
        ZP(0x8F) = 0x0A; ZP(0x91) = 0x0A;
        ZP(0x93) = 0xF3; ZP(0x94) = 0x58;
        ss2_msg_text();                      /* $69EE */
        do ss2_wait(1); while (!(Z_PRESS & 0x01));
        ss2_title_return();
    }
    /* $F917: revived? */
    blocks_continue();                       /* $7ADE */
    ss2_bat_picture(0x0296, 0xFC32, 0x06, 0x0E);   /* $FBB1 */
    ss2_bat_box(0x0286, 0xA7C6);
    ss2_sfx(0x02);                           /* continue */
    ss2_fade_late = 1;                       /* (measured: its colours cross a VBlank) */
    ss2_fade_in_alt();
    ZP(0x8F) = 0x06; ZP(0x91) = 0x0A;
    ZP(0x93) = 0x6A; ZP(0x94) = 0x59;
    ss2_msg_text();                          /* ends in a yes/no choice ($96) */
    do ss2_wait(1); while (!(Z_PRESS & 0x01));
    if (ZP(0x96))
    {
        ss2_fade_out_alt();                  /* $7211 */
        ss2_bg_off();
        ss2_title_return();
    }
    {
        u8 x;                                /* $F979: four white flashes */
        for (x = 4; x; x--)
        {
            ss2_wait(6);                     /* $B9C4 */
            ss2_vce[0x100] = 0x1FF;
            ss2_vce_dirty = 1;
            ss2_bg_off();
            ss2_spr_off();
            ss2_wait(6);
            ss2_vce[0x100] = 0;
            ss2_bg_on();
            ss2_spr_on();
        }
    }
    R(0x22C1) &= (u8)~0x04;                  /* the charm is used */
    ZP(0x77) = R(0x22C1);
    ZP(0x78) = R(0x22BF);
    zero(0x2203, 8);
    ZP(0x7A) &= (u8)~0x30;
    ss2_fade_out_cont();                     /* $72BA */
    seconds(1);
    stage_init();                            /* JMP $F9F0 */
}

/* ---- power-on (bank 0 $E000 with Y = 0, $E065, $E2D0) ------------------- */

/* the rest of the reset code ($E2D0), run as slot 0 so it can hold the
 * CPU: the BAT clear and the stage-1 block upload take 4 VBlanks before
 * the tasks start */
static void title_task(void);

static void boot_task(void)
{
    u8 i;
    ZP(0x8B) = 0x06;
    ZP(0x25) = 1;
    R(0x2283) = 0; R(0x2284) = 0;
    for (i = 0; i < 0x20; i++) R(0x2263 + i) = 0xFF;
    R(0x2295) = 0; R(0x2296) = 0;
    for (i = 0; i < 0x10; i++) R(0x2285 + i) = 0xFF;
    ss2_bat_clear();
    vram_clear();
    blocks_stage();                          /* $7882 (stage 1) */
    vram_clear();                            /* $797E */
    ss2_busy(4);
    ZP(0x8C) = 0x01; ZP(0x8D) = 0xC3;
    ZP(0x10) = 0x01; ZP(0x11) = 0xFD;        /* $E30B: task 0 = $E330, then */
    ss2_task_create(3, ss2_score_task);      /* task 3 = $FD01 ($10/$11 last) */
    ss2_task_restart(title_task);
}

void ss2_power_on(void)
{
    zero(0x2000, 0x2000);
    R(0x2FFB) = 2;
    ZP(0x8E) = 5;                            /* TAM #$40 = 5 */
    ss2_page[6] = ss2_banks[5];
    ss2_mpr6 = 5;
    R(0x2201) = 0x0C;
    R(0x2202) = 0;
    ss2_task_create(0, boot_task);
}

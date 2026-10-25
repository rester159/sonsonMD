/* SON SON II player logic, rebuilt from HuCard bank 9 $C000-$D03D.
 *
 * Control flow follows the source routine boundaries (a source JMP is a tail
 * call here, JSR a call) so each block can be checked against the trace.
 * Comparisons and carries are reproduced at byte width.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"

#define B9(a)  (ss2_bank9[(u16)(a) - 0xC000])


static void move(void);
static void main_body(void);
static void mode_other(void);
static void walk(void);
static void jump(void);
static void attack(void);
static void jump_attack(void);
static void cloud(void);
static void land(void);
static void jump_start(void);
static void stop_on_ground(void);
static void hurt_mode(void);
static void buttons(void);
static void cloud_steer(void);
static void cloud_face(void);
static void demo_knock(void);

/* ---- shared helpers ---------------------------------------------------- */

/* top:mid:frac += sign-extended (vh:vl); top masked as the source does */
static void add3(u16 top, u16 mid, u16 fr, u8 vh, u8 vl, u8 mask)
{
    u16 t;
    u8 sign = (vh & 0x80) ? 0xFF : 0x00;
    t = (u16)R(fr) + vl;
    R(fr) = (u8)t;
    t = (u16)R(mid) + vh + (t >> 8);
    R(mid) = (u8)t;
    R(top) = (u8)((R(top) + sign + (t >> 8)) & mask);
}

/* $BBF5: queue a sound request in the 16-entry ring if the slot is free */
void ss2_sfx(u8 id)
{
    u8 x = SFX_HEAD;
    if (SFX_RING(x) == 0xFF)
    {
        SFX_RING(x) = id;
        SFX_HEAD = (u8)((x + 1) & 0x0F);
    }
}

/* $C3A0/$C50D/$CDF9: hide the staff segment sprites (SATB 2-6) and clear
 * the segment table. The VRAM part is a presentation request. */
static void staff_clear(void)
{
    u8 x;
    ss2_staff_sprites_hidden = 1;
    for (x = 0; x < 5; x++) R(0x23DD + x) = 0;
}

/* bank 3 $8307: collision probe at player + ($22A6, $22A7) */
u8 ss2_probe(u8 ox, u8 oy)
{
    P_PROBEX = ox;
    P_PROBEY = oy;
    return ss2_probe_point((u16)(((P_XH << 8) | P_XL) + ox), (u16)(((P_YH << 8) | P_YL) + oy));
}

#define PROBE_RIGHT()   ss2_probe(0x18, 0x10)   /* $C6CC */
#define PROBE_FOOT_R()  ss2_probe(0x14, 0x20)   /* $C6D9 */
#define PROBE_FOOT()    ss2_probe(0x10, 0x20)   /* $C6E6 */
#define PROBE_FOOT_L()  ss2_probe(0x0C, 0x20)   /* $C6F3 */
#define PROBE_LEFT()    ss2_probe(0x08, 0x10)   /* $C700 */
#define PROBE_HEAD()    ss2_probe(0x10, 0x08)   /* $C71A */
#define PROBE_BODY()    ss2_probe(0x10, 0x10)   /* $C734 */

/* $C565: face the object that hit the player (D8/D9 = 0 or 4).
 * Returns the source's Z flag: 1 when facing right (0). */
static u8 face_hitter(void)
{
    u8 x = (u8)(P_HITOBJ - 1), a;
    if (R(0x269A + x) & 0x20) a = R(0x249A + x);
    else a = (u8)(((R(0x253A + x) & 1) << 3) + 8 + R(0x249A + x));
    ZP(0x37) = a;
    if ((u8)(P_XL + 0x10) >= a)
    {
        P_DIR = 4; P_LASTDIR = 4;
        return 0;
    }
    P_DIR = 0; P_LASTDIR = 0;
    return 1;
}

/* ---- $C000: player update --------------------------------------------- */
void ss2_player_update(void)
{
    if (P_CLOUDREQ) { cloud(); return; }
    if (P_MODE != 0x80) { mode_other(); return; }
    main_body();
}

/* $C012 */
static void main_body(void)
{
    PROBE_FOOT();
    if (Z_HIT_HURT)
    {
        P_C4 = 1;
        P_JUMPREQ = 1;
        if (!P_HURT || P_HURT >= 5)
        {
            P_MODE = 0x20;
            P_HURT = 5;
            if ((u8)(R(0x22E9) | R(0x22EA) | R(0x22EB)) == 0x80)
            {
                R(0x22BD)--;
                if ((s8)--R(0x22BB) < 0) { R(0x22BD) = 0; R(0x22BB) = 0; }
                R(0x22E9) = 0x40;
                R(0x238A) = R(0x2398);
                R(0x2383) = R(0x2391);
            }
            else
            {
                if ((s8)--R(0x22BA) < 0) R(0x22BA) = 0;
                if ((s8)--P_LIFE <= 0)
                {
                    R(0x22BA) = 0;
                    P_LIFE = 0;
                    P_MODE = 0x40;
                    mode_other();
                    return;
                }
            }
        }
    }
    /* $C081: direction input */
    if (!Z_INLOCK2 && (Z_PAD & 0xF0))
    {
        u8 x = (u8)((Z_PAD & 0xF0) >> 3), was;
        if (!P_JUMPING) P_JUMPKIND = B9(0xDA5B + x);
        P_DIR = B9(0xDA5B + x + 1);
        was = P_NOPAD;
        P_NOPAD = 0;
        if (was) P_ANIMTICK = 7;
        P_ANIMTICK++;
    }
    else
    {
        P_NOPAD = 0x88;
        P_ANIMTICK++;
    }
    buttons();
}

/* $C0BB: sample the buttons and dispatch on (attack + jump) requests */
static void buttons(void)
{
    if (!(Z_INLOCK2 | Z_INLOCK1))
    {
        if (Z_PRESS & 0x01) P_JUMPREQ = 1;
        if (!(P_C4 | P_HITOBJ) && (Z_PRESS & 0x02)) P_ATTACKREQ = 2;
    }
    if (P_ONCLOUD) { cloud(); return; }
    switch ((u8)((P_ATTACKREQ + P_JUMPREQ) * 2))
    {
        case 0: walk(); return;
        case 2: jump(); return;
        case 4: attack(); return;
        case 6: jump_attack(); return;
    }
    ss2_unported(0xC0F0);
}

/* $C0FB: apply velocity, scrolling the camera or moving the sprite */
static void move(void)
{
    u8 scroll_x, move_y;
    if (!Z_HSCROLL)
    {
        if (Z_CAMLOCK) { scroll_x = 0; move_y = 1; }
        else if (Z_VLOCK) { scroll_x = 1; move_y = 1; }
        else { scroll_x = 1; move_y = 0; }
    }
    else
    {
        move_y = 1;
        if ((s8)P_VXH < 0) scroll_x = P_XL < 0x40;
        else scroll_x = P_XL >= 0xA0;
    }
    if (scroll_x) add3(0x2045, 0x2046, 0x2047, P_VXH, P_VXL, 0x07);
    else add3(0x2298, 0x2299, 0x229A, P_VXH, P_VXL, 0xFF);
    if (move_y) add3(0x229B, 0x229C, 0x229D, P_VYH, P_VYL, 0xFF);
    else add3(0x2048, 0x2049, 0x204A, P_VYH, P_VYL, 0x07);
    ZP(0x30) = 0;   /* sign temp left by the last add (positive path) */
    if ((s8)P_VYH < 0) ZP(0x30) = 0xFF;

    /* $C1FB */
    if (P_PUSHOBJ) ss2_platform_carry();     /* bank 2 $7FD8 */
    if (P_JUMPING || !P_SETTLE) return;
    {
        u8 d, c;
        if (P_YL < 0xA0)
        {
            d = (u8)(0xA0 - P_YL);
            if (d > 4) d = 4;
            P_YL = (u8)(P_YL + d);
        }
        else if (!(P_ONVINE | P_STANDING | P_PUSHOBJ) && (d = (u8)((Z_CAMY_LO + P_YL) & 0x0F)) != 0)
        {
            if (d > 4) d = 4;
        }
        else
        {
            P_YL = 0xA0;
            P_SETTLE = 0;
            P_YF = 0;
            return;
        }
        /* $C235: the camera rises by d so the player keeps his world y */
        ZP(0x30) = d;
        c = (u8)(Z_CAMY_LO < d);
        Z_CAMY_LO = (u8)(Z_CAMY_LO - d);
        Z_CAMY_HI = (u8)((Z_CAMY_HI - c) & 0x07);
    }
}

/* $C278: modes other than normal ($40 dying, $20 hurt) */
static void mode_other(void)
{
    u8 y;
    if (Z_DEMOFX) { demo_knock(); return; }
    if (P_MODE == 0x20) { hurt_mode(); return; }
    if (P_FLAGSC1 & 0x02)
    {
        /* $C4B8: clear the item box ($BADE $4E8F), then hurt with half
         * the maximum life */
        ss2_hud_text(0x4E8F);
        P_MODE = 0x20;
        P_LIFE = (u8)(R(0x22B9) >> 1);
        P_FLAGSC1 &= (u8)~0x02;
        mode_other();
        return;
    }
    if (!P_DEATH)
    {
        /* $C33F: start the death leap */
        ss2_set_vx(0x0180);
        P_ATTRH &= (u8)~0x08;
        if (face_hitter())
        {
            ss2_set_vx(0xFE80);
            P_ATTRH |= 0x08;
        }
        ss2_set_vy(0xFB70);
        P_FRAMEL = ss2_rom_byte((u16)((P_DEATHPTRH << 8) | P_DEATHPTRL));
        P_FRAMEH = ss2_rom_byte((u16)(((P_DEATHPTRH << 8) | P_DEATHPTRL) + 1));
        P_DEATH = 1;
        P_ONCLOUD = 0;
        {
            u8 x = P_YL;
            if (P_SETTLE) x = 0xA0;
            if (!P_JUMPING) P_JUMPY = x;
        }
        Z_VLOCK = 1;
        staff_clear();
        ss2_sfx(0xF0);
        ss2_sfx(0x0F);
        move();
        return;
    }
    /* $C29B: dying leap */
    if ((s8)P_VXH >= 0) { if (PROBE_RIGHT()) ss2_set_vx(0); }
    else if (PROBE_LEFT()) ss2_set_vx(0);
    if (P_VYH | P_VYL)
    {
        if (P_VYH != 0x05)
        {
            u16 t = (u16)P_VYL + 0x3F;
            P_VYL = (u8)t;
            P_VYH = (u8)(P_VYH + (t >> 8));
            if ((s8)P_VYH < 0)
            {
                if (ss2_probe(0x10, 0x08)) { P_VYH = 0; P_VYL = 1; }
                move();
                return;
            }
        }
        if (!ss2_probe(0x10, 0x1C))
        {
            if (P_YL >= P_JUMPY) Z_VLOCK = 0;
            y = 2;
            goto set_frame;
        }
    }
    ss2_set_vx(0);
    ss2_set_vy(0);
    if (++P_DEATH == 0xF0) { ss2_player_died(); return; }
    y = 4;
set_frame:
    {
        u16 p = (u16)((P_DEATHPTRH << 8) | P_DEATHPTRL);
        P_FRAMEL = ss2_rom_byte((u16)(p + y));
        P_FRAMEH = ss2_rom_byte((u16)(p + y + 1));
    }
    move();
}

/* $C3CD: hurt / knockback */
static void hurt_mode(void)
{
    u8 t;
    if (P_ONCLOUD)
    {
        /* $C460 */
        t = P_HURT--;
        if (!P_HURT) { P_MODE = 0x80; main_body(); return; }
        if (t == 0x5A)
        {
            ss2_set_vx(0x0500);
            if (face_hitter()) ss2_set_vx(0xFB00);
            ss2_set_vy(0);
            P_ATTRL |= 0x0F;
            ss2_sfx(0x15);
            cloud_steer();
            return;
        }
        if (t == 0x53)
        {
            P_ATTRL &= (u8)~0x0F;
            ss2_set_vx(0);
            P_HITOBJ = 0;
            main_body();
            return;
        }
        if (t > 0x53) { cloud_steer(); return; }
        main_body();
        return;
    }
    t = P_HURT--;
    if (!P_HURT) { P_MODE = 0x80; main_body(); return; }
    if (t != 0x5A)
    {
        if (t >= 0x56) { main_body(); return; }
        P_ATTRL &= (u8)~0x0F;
        if (P_JUMPKIND >= 7) P_HURT++;
        main_body();
        return;
    }
    /* $C3FD: knockback starts */
    P_ATTRL |= 0x0F;
    PROBE_BODY();
    if (!Z_HIT_VINE)
    {
        PROBE_FOOT();
        if (!Z_HIT_VINE)
        {
            u8 x;
            P_ONVINE = 0;
            P_JUMPREQ = 1;
            x = face_hitter() ? 8 : 7;
            P_JUMPKIND = x;
            P_JUMPDIR = P_DIR;
            P_STANDING = 0;
            x = (u8)(x << 2);
            P_VXL = B9(0xDAF7 + x);
            P_VXH = B9(0xDAF7 + x + 1);
            P_VYL = B9(0xDAF7 + x + 2);
            P_VYH = B9(0xDAF7 + x + 3);
            P_ANIMSET = 0x0C;
            ss2_sfx(0x15);
            buttons();
            return;
        }
    }
    P_HITOBJ = 0;
    ss2_sfx(0x15);
    buttons();
}

/* $C5A1: standing on something: vines end the stance */
static void stand_check(void)
{
    P_C6++;
    PROBE_BODY();
    if (Z_HIT_VINE)
    {
        ss2_set_vx(0);
        ss2_set_vy(0);
        P_STANDING = 0;
        P_JUMPING = 0;
        P_JUMPREQ = 0;
        P_JUMPHIGH = 0;
        Z_VLOCK = 0;
        P_HITOBJ = 0;
        P_SETTLE = 1;
        P_ONVINE = 1;
        ss2_center_camera_x();
        return;
    }
    /* $C5D3 */
    if (P_NOPAD)
    {
        P_DIR = P_LASTDIR;
        ss2_set_vx(0);
    }
    else
    {
        u8 x;
        if (P_DIR == 5 || P_DIR == 2) goto stop;
        x = (u8)(((P_DIR + 1) & 4) >> 1);
        if (!x) { if (PROBE_RIGHT()) goto stop; }
        else if (PROBE_LEFT()) goto stop;
        P_VXL = B9(0xDB1F + x);
        P_VXH = B9(0xDB1F + x + 1);
        x = P_DIR;
        goto set_dir;
stop:
        ss2_set_vx(0);
        x = P_LASTDIR;
set_dir:
        P_DIR = (u8)((x + 1) & 4);
        P_LASTDIR = P_DIR;
    }
    /* $C629 */
    if (P_JUMPING && P_YL >= P_JUMPY)
    {
        Z_VLOCK = 0;
        P_YL = 0xA0;
    }
    if (PROBE_FOOT_R() || PROBE_FOOT_L() || P_PUSHOBJ)
    {
        ss2_set_vx(0);
        ss2_set_vy(0);
        P_STANDING = 0;
        P_JUMPING = 0;
        P_JUMPREQ = 0;
        P_JUMPHIGH = 0;
        P_ONVINE = 0;
        P_C4 = 0;
        P_HITOBJ = 0;
        Z_VLOCK = 0;
        P_SETTLE = 1;
        return;
    }
    if (P_VYH != 0x04)
    {
        u16 t = (u16)P_VYL + 0x40;
        P_VYL = (u8)t;
        P_VYH = (u8)(P_VYH + (t >> 8));
    }
}

/* $C690: stop and stand */
static void stop_on_ground(void)
{
    ss2_set_vx(0);
    ss2_set_vy(0);
    P_C6 = 0;
    P_STANDING = 1;
    move();
}

/* $C6A7: centre the camera x on the player's 16 px column */
void ss2_center_camera_x(void)
{
    u8 d = (u8)(8 - ((P_XL + Z_CAMX_LO) & 0x0F));
    u16 t;
    ZP(0x30) = (u8)((P_XL + Z_CAMX_LO) & 0x0F);
    ZP(0x26) = d;
    ZP(0x27) = (s8)d < 0 ? 0xFF : 0x00;
    t = (u16)Z_CAMX_LO + d;
    Z_CAMX_LO = (u8)t;
    Z_CAMX_HI = (u8)(Z_CAMX_HI + ZP(0x27) + (t >> 8));
}

/* ---- $C741: no jump/attack request ------------------------------------ */
static void walk(void)
{
    u8 x;
    if (P_STANDING)
    {
        stand_check();
        if (P_ATTACKREQ | P_ATTACKING) { attack(); return; }
        move();
        return;
    }
    P_ANIMSET = 0;
    if (P_NOPAD)
    {
        x = P_LASTDIR;
        if (x & 3) x = (u8)((x & 2) << 1);
        P_DIR = x;
        P_ANIMTICK = 0;
        ss2_set_vx(0);
        ss2_set_vy(0);
        P_JUMPKIND = 0;
    }
    else
    {
        x = (u8)(P_DIR << 2);
        P_VXL = B9(0xDABB + x);
        P_VXH = B9(0xDABB + x + 1);
        P_VYL = B9(0xDABB + x + 2);
        P_VYH = B9(0xDABB + x + 3);
    }
    switch (P_DIR)
    {
        case 0:     /* $C7BC: right */
            P_LASTDIR = P_DIR;
            P_CLIMBSTEP = 0;
            if (P_ONVINE) { P_ANIMSET = 0x08; goto halt_x; }
            if (PROBE_RIGHT()) goto halt_x;
            if (PROBE_FOOT_R() || PROBE_FOOT_L() || P_PUSHOBJ) { move(); return; }
            P_ANIMSET = 0x04;
            stop_on_ground();
            return;
        case 1:     /* $C7FD: down-right */
        case 3:     /* $C7FD: down-left */
            P_LASTDIR = P_DIR;
            if (P_ONVINE) { P_ANIMSET = 0x08; move(); return; }
            P_CLIMBSTEP = 1;
            P_ANIMTICK = 0;
            move();
            return;
        case 2:     /* $C81B: down */
            if (!P_ONVINE)
            {
                x = (u8)(P_LASTDIR >> 1);
                P_DIR = (u8)((P_DIR >> 1) + x);
                ss2_set_vy(0);
                P_CLIMBSTEP = 1;
                move();
                return;
            }
            PROBE_BODY();
            if (!Z_HIT_VINE)
            {
                PROBE_FOOT();
                if (!Z_HIT_VINE)
                {
                    P_ANIMSET = 0x04;
                    stop_on_ground();
                    return;
                }
            }
            P_CLIMBSTEP = 0;
            P_ANIMSET = 0x08;
            move();
            return;
        case 4:     /* $C85D: left */
            P_LASTDIR = P_DIR;
            P_CLIMBSTEP = 0;
            if (P_ONVINE) { P_ANIMSET = 0x08; goto halt_x; }
            if (PROBE_LEFT()) goto halt_x;
            if (PROBE_FOOT_L() || PROBE_FOOT_R() || P_PUSHOBJ) { move(); return; }
            P_ANIMSET = 0x04;
            stop_on_ground();
            return;
        case 5:     /* $C89E: up */
            P_CLIMBSTEP = 0;
            if (!P_ONVINE)
            {
                x = P_LASTDIR;
                if (x & 3) x = (u8)((x & 2) << 1);
                P_DIR = x;
                ss2_set_vy(0);
                P_ANIMTICK = 0;
                move();
                return;
            }
            P_ANIMSET = 0x08;
            PROBE_BODY();
            if (!Z_HIT_VINE)
            {
                PROBE_HEAD();
                if (!Z_HIT_VINE)
                {
                    x = P_LASTDIR;
                    if (x & 3) x = (u8)((x & 2) << 1);
                    P_DIR = x;
                    ss2_set_vy(0);
                    move();
                    return;
                }
            }
            move();
            return;
    }
    ss2_unported(0xC7AD);
    return;
halt_x:
    ss2_set_vx(0);
    move();
}

/* ---- $C8F1: jump ------------------------------------------------------ */
static void jump(void)
{
    u8 x;
    if (P_STANDING)
    {
        if (P_ATTACKREQ | P_ATTACKING) { attack(); return; }
        stand_check();
        move();
        return;
    }
    if (!P_NOPAD)
    {
        x = P_DIR;
        if (x == 2 || x == 5) x = P_LASTDIR;
        if (x & 3) x = (u8)((x & 2) << 1);
        P_DIR = x;
        P_LASTDIR = x;
    }
    else P_DIR = P_LASTDIR;
    if (P_JUMPING) { ss2_jump_air(); return; }
    P_JUMPDIR = P_DIR;
    if (!P_ONVINE)
    {
        if (P_C4) P_JUMPKIND = 0;
        if (!P_JUMPKIND) { P_DIR = P_LASTDIR; jump_start(); return; }
        if (P_HITOBJ) { jump_start(); return; }
        if (P_JUMPKIND & 1)
        {
            PROBE_RIGHT();
            if (Z_HIT_3) P_JUMPKIND = 5;
        }
        else
        {
            PROBE_LEFT();
            if (Z_HIT_3) P_JUMPKIND = 6;
        }
        jump_start();
        return;
    }
    /* $C98E: leaving a vine */
    if (!P_NOPAD)
    {
        if (!P_JUMPKIND && (Z_PAD & 0x50))
        {
            if ((Z_PAD & 0x50) == 0x10)
            {
                /* $C9C3: up */
                if (PROBE_HEAD() || Z_HIT_VINE) goto cancel;
                P_JUMPKIND = 0;
                P_DIR = 5;
                jump_start();
                return;
            }
            if ((Z_PAD & 0x50) == 0x40) goto cancel;
        }
        if (PROBE_HEAD()) goto cancel;
        P_JUMPKIND = ((P_DIR >> 1) & 3) ? 6 : 5;
        jump_start();
        return;
    }
cancel:
    P_JUMPREQ = 0;
    P_JUMPING = 0;
    move();
}

/* $C9E0: take-off */
static void jump_start(void)
{
    u8 x = 0x0C;
    if (!(P_C4 | P_HITOBJ))
    {
        x = 0x04;
        if (P_ATTACKING) x = 0x06;
    }
    P_ANIMSET = x;
    x = P_YL;
    if (P_SETTLE) x = 0xA0;
    P_JUMPY = x;
    P_JUMPING = 1;
    Z_VLOCK = 1;
    x = (u8)(P_JUMPKIND << 2);
    P_VXL = B9(0xDAF7 + x);
    P_VXH = B9(0xDAF7 + x + 1);
    P_VYL = B9(0xDAF7 + x + 2);
    P_VYH = B9(0xDAF7 + x + 3);
    ss2_sfx(0x16);
    ss2_jump_air();
}

/* $CA30: airborne */
void ss2_jump_air(void)
{
    if ((s8)P_VYH >= 0)
    {
        /* $CB09: falling */
        P_ONVINE = 0;
        if (P_YL >= P_JUMPY) { Z_VLOCK = 0; P_YL = 0xA0; }
        if (P_JUMPKIND)
        {
            if ((s8)P_VXH >= 0) { if (PROBE_RIGHT()) { land(); return; } }
            else if (PROBE_LEFT()) { land(); return; }
        }
        PROBE_BODY();
        if (Z_HIT_VINE) { ss2_grab_vine(); return; }
        if (PROBE_FOOT_R() || PROBE_FOOT_L() || P_PUSHOBJ) { ss2_touch_down(); return; }
        goto steer;
    }
    /* rising */
    if (ss2_probe(0x0C, 0x08) || ss2_probe(0x14, 0x08) || P_YL < 0x38) { land(); return; }
    if (!P_JUMPKIND)
    {
        if (!P_JUMPHIGH)
        {
            if ((u8)(P_JUMPY - P_YL) >= 0x30)
            {
                u8 d = (u8)(Z_PAD & 0xF0);
                P_JUMPHIGH = 1;
                if (d && d != 0x10 && d != 0x40)
                {
                    u8 x;
                    if (!(d & 0x80))
                    {
                        if (PROBE_RIGHT()) goto vine_or_steer;
                        P_JUMPDIR = 0;
                        P_JUMPKIND = 1;
                    }
                    else
                    {
                        if (PROBE_LEFT()) goto vine_or_steer;
                        P_JUMPDIR = 4;
                        P_JUMPKIND = 2;
                    }
                    x = (u8)(P_JUMPKIND & 2);
                    P_VXL = B9(0xDB1B + x);
                    P_VXH = B9(0xDB1B + x + 1);
                }
            }
        }
    }
    else if ((s8)P_VXH >= 0)
    {
        if ((P_ONVINE || P_JUMPKIND != 5) && PROBE_RIGHT()) { land(); return; }
    }
    else if ((P_ONVINE || P_JUMPKIND != 6) && PROBE_LEFT()) { land(); return; }
vine_or_steer:
    if (!P_ONVINE)
    {
        PROBE_BODY();
        if (Z_HIT_VINE) { ss2_grab_vine(); return; }
    }
steer:
    /* $CB5B: mid-air direction changes */
    if (P_JUMPKIND && P_DIR != P_JUMPDIR && !(P_ATTACKING | P_ATTACKREQ))
    {
        P_JUMPDIR = P_DIR;
        if ((P_DIR + 1) & 4) ss2_set_vx(0xFF00);
        else ss2_set_vx(0x0100);
    }
    if (P_VYH != 0x04)
    {
        u16 t = (u16)P_VYL + 0x40;
        P_VYL = (u8)t;
        P_VYH = (u8)(P_VYH + (t >> 8));
    }
    if (P_ATTACKREQ | P_ATTACKING) { attack(); return; }
    move();
}

/* $CBB7: touched ground while falling */
void ss2_touch_down(void)
{
    P_ANIMSET = 0;
    ss2_set_vx(0);
    ss2_set_vy(0);
    P_JUMPING = 0;
    P_JUMPREQ = 0;
    P_JUMPHIGH = 0;
    P_JUMPKIND = 0;
    P_C4 = 0;
    P_HITOBJ = 0;
    Z_VLOCK = 0;
    P_SETTLE = 1;
    if (P_ATTACKREQ | P_ATTACKING) { attack(); return; }
    move();
}

/* $CBEF: head or wall contact */
static void land(void)
{
    P_JUMPKIND = 0;
    P_SETTLE = 1;
    stop_on_ground();
}

/* $CBFA: caught a vine */
void ss2_grab_vine(void)
{
    ss2_center_camera_x();
    P_ANIMSET = 0x08;
    P_JUMPING = 0;
    P_JUMPREQ = 0;
    P_JUMPHIGH = 0;
    ss2_set_vx(0);
    ss2_set_vy(0);
    P_C4 = 0;
    P_HITOBJ = 0;
    Z_VLOCK = 0;
    P_SETTLE = 1;
    P_ONVINE = 1;
    if (P_ATTACKREQ | P_ATTACKING) { attack(); return; }
    move();
}

/* ---- $CC35: staff attack ---------------------------------------------- */
static void attack(void)
{
    u8 x;
    if (P_STANDING) stand_check();
    if (P_C4 | P_HITOBJ) goto finish;
    if (!P_ATTACKING)
    {
        ss2_sfx(0x19);
        if (!(P_JUMPING | P_ONCLOUD))
        {
            ss2_set_vx(0);
            if (!P_STANDING) ss2_set_vy(0);
        }
        R(0x2432) = 0;
        R(0x2437) = 0;
        if (P_NOPAD)
        {
            P_DIR = (u8)((P_LASTDIR + 1) & 4);
            P_LASTDIR = P_DIR;
        }
        else if (P_DIR == 2)
        {
            if (P_LASTDIR & 3) P_DIR = P_LASTDIR;
            else P_DIR = (u8)((P_LASTDIR >> 1) + 1);
        }
        else if (P_DIR == 5) P_DIR = P_LASTDIR;
        else if (!(P_DIR & 3)) P_LASTDIR = P_DIR;
        P_ATKDIR = P_DIR;
        P_ATTACKING = 1;
    }
    /* $CCBD */
    P_DIR = P_ATKDIR;
    x = 0x02;
    if (P_JUMPING) x = 0x06;
    else if (P_ONVINE) x = 0x0A;
    P_ANIMSET = x;
    if (R(0x23DD) != 0x80)
    {
        if (!R(0x2432)) goto step;
        R(0x2432) = 0;      /* $CCE7 */
    }
    if (R(0x243C)) goto retract;
    if (R(0x2432) == P_STAFFMAX)
    {
        R(0x2432) = 0;
        R(0x243C) = 1;
        goto retract;
    }
    {
        u16 p;
        x = 0;
        if (!R(0x2432))
        {
            /* $CD0B: first segment */
            p = ((P_DIR + 1) & 4) ? 0xFFF0 : 0x0020;
        }
        else
        {
            /* $CD29: grow by one segment */
            R(0x2437)++;
            x = (u8)(R(0x2432) - 1);
            if (!((P_DIR + 1) & 4))
            {
                p = (u16)(x * 16 + 0x20);
                R(0x23E7 + x) = (u8)p;
                R(0x23E2 + x) = (u8)(p >> 8);
                R(0x23DD + x) = 0x80;
                p = (u16)(p + 0x10);
            }
            else
            {
                p = (u16)(0xFFF0 - x * 16);
                R(0x23E7 + x) = (u8)p;
                R(0x23E2 + x) = (u8)(p >> 8);
                R(0x23DD + x) = 0x80;
                p = (u16)(p - 0x10);
            }
            ZP(0x26) = (u8)p;
            ZP(0x27) = (u8)(p >> 8);
            R(0x240F + x) = 0x0A;
            R(0x240A + x) = 0x02;
            R(0x2405 + x) = 0x80;
            R(0x2400 + x) = 0x00;
            x++;
        }
        ZP(0x26) = (u8)p;
        ZP(0x27) = (u8)(p >> 8);
        /* $CD9C: tip segment */
        R(0x23DD + x) = 0x80;
        R(0x23E7 + x) = (u8)p;
        R(0x23E2 + x) = (u8)(p >> 8);
        R(0x2405 + x) = B9(0xDAE9 + (u8)(Z_STAFFPAL << 1));
        R(0x2400 + x) = B9(0xDAE9 + (u8)(Z_STAFFPAL << 1) + 1);
        R(0x2400 + x) |= ((P_DIR + 1) & 4) ? 0x00 : 0x08;
        R(0x240F + x) = 0x08;
        R(0x240A + x) = 0x02;
        goto step;
    }
retract:
    if (R(0x2432) == 4) goto finish;
step:
    R(0x2432)++;
    if (P_STANDING) stand_check();
    move();
    return;
finish:
    /* $CDF0 */
    P_ATTACKING = 0;
    P_ATTACKREQ = 0;
    R(0x243C) = 0;
    staff_clear();
    if (P_C4 | P_HITOBJ) x = P_ONCLOUD ? 0x00 : 0x0C;
    else if (P_JUMPING | P_STANDING) x = 0x04;
    else x = P_ONVINE ? 0x08 : 0x00;
    P_ANIMSET = x;
    move();
}

/* ---- $CE4B: jump pressed while attacking ------------------------------ */
static void jump_attack(void)
{
    if (P_MAGICSEL && ss2_rom_byte((u16)(0x4CEA + P_MAGICSEL)) <= P_MAGIC && (Z_PRESS & 0x01))
    {
        if (!P_JUMPING) { P_JUMPREQ = 0; P_JUMPING = 0; }
        ss2_magic_cast();                   /* $D1E8 ... JMP $CC35 */
        attack();
        return;
    }
    jump();
}

/* ---- $C4D7: thrown by the hitter ($86 set) --------------------------- */
static void demo_knock(void)
{
    P_ANIMTICK++;
    if (P_HURT == 0x5A)
    {
        u8 x;
        P_HURT--;
        P_ANIMSET = 0x0C;
        P_ATTRL &= (u8)~0x0F;
        x = (u8)(P_HITOBJ - 1);
        P_VXH = R(0x25BA + x);
        P_VXL = R(0x25DA + x);
        ss2_set_vy(0);
        P_ATTACKING = 0;
        P_ATTACKREQ = 0;
        R(0x243C) = 0;
        ZP(0x8B) = 0x02;                    /* $C50D: SATB 2-6 cleared */
        staff_clear();
    }
    /* $C531 */
    if (!((s8)P_VXH < 0 ? ss2_probe(0x00, 0x10) : ss2_probe(0x20, 0x10))) { move(); return; }
    P_MODE = 0x80;
    P_HURT = 0;
    stop_on_ground();
}

/* ---- $CE74: Kinto'un cloud -------------------------------------------- */

/* $CF4A: hop up onto the arriving cloud, then gravity */
static void cloud_hop(void)
{
    u16 t;
    if (!Z_VLOCK)
    {
        Z_VLOCK = 1;
        P_ANIMSET = 0x04;
        ss2_set_vy(0xFD00);
        ss2_set_vx(0);
    }
    t = (u16)(P_VYL + 0x30);
    P_VYL = (u8)t;
    P_VYH = (u8)(P_VYH + (t >> 8));
}

static void cloud(void)
{
    u16 t;
    R(0x23CF) = B9(0xDAF5 + ((Z_FRAME >> 3) & 1));
    P_ANIMTICK = 0;
    if (P_ONCLOUD)
    {
        /* $CF79: riding: eight-way flight */
        if (Z_INLOCK2 || !(Z_PAD & 0xF0))
        {
            ss2_set_vx(0);
            ss2_set_vy(0);
            cloud_face();
            return;
        }
        {
            u8 x = (u8)((Z_PAD & 0xF0) >> 2);
            P_VXL = B9(0xDA7B + x);
            P_VXH = B9(0xDA7B + x + 1);
            P_VYL = B9(0xDA7B + x + 2);
            P_VYH = B9(0xDA7B + x + 3);
        }
        cloud_steer();
        return;
    }
    if (R(0x23C5) != 0x80)
    {
        /* $CE94: summon: cloud record from $DA3B */
        u8 i;
        for (i = 0; i < 0x18; i++) R(0x23C5 + i) = B9(0xDA3B + i);
        ss2_set_vx(0);
        ss2_set_vy(0);
        P_JUMPREQ = 0;
        P_JUMPING = 0;
        P_ATTACKREQ = 0;
        P_ATTACKING = 0;
        ss2_sfx(0x1F);
        move();
        return;
    }
    /* $CEBB: flying in */
    if (R(0x23CA) >= 0xB0)
    {
        R(0x23D2) = 0;
        R(0x23D3) = 0;
        R(0x23CC) |= 0x08;
        if ((s8)R(0x23D0) >= 0) cloud_hop();
        t = (u16)(R(0x23D1) + 0x38);
        R(0x23D1) = (u8)t;
        R(0x23D0) = (u8)(R(0x23D0) + (t >> 8));
    }
    /* $CEE5 */
    t = (u16)(R(0x23C8) + R(0x23D1));
    R(0x23C8) = (u8)t;
    R(0x23C7) = (u8)(R(0x23C7) + R(0x23D0) + (t >> 8));
    t = (u16)(R(0x23CB) + R(0x23D3));
    R(0x23CB) = (u8)t;
    R(0x23CA) = (u8)(R(0x23CA) + R(0x23D2) + (t >> 8));
    if ((R(0x23D2) | R(0x23D3)) || R(0x23C7) < 0x70) { move(); return; }
    /* $CF20: aboard */
    Z_VLOCK = 0;
    P_ANIMSET = 0;
    Z_HSCROLL = 1;
    P_ONCLOUD = 1;
    P_CLOUDREQ = 0;
    ss2_set_vy(0);
    R(0x23D0) = 0;
    R(0x23D1) = 0;
    P_YL = 0x96;
    R(0x23C7) = 0x6B;
    move();
}

/* $CFAF: keep the cloud between y $38 and $A0 and out of walls */
static void cloud_steer(void)
{
    if ((s8)P_VYH < 0) { if (P_YL < 0x38) ss2_set_vy(0); }
    else if (P_YL >= 0xA0) ss2_set_vy(0);
    if ((s8)P_VXH >= 0) { if (PROBE_RIGHT()) ss2_set_vx(0); }
    else if (PROBE_LEFT()) ss2_set_vx(0);
    cloud_face();
}

/* $CFE1: facing, cloud offset, staff or magic */
static void cloud_face(void)
{
    u8 a = P_DIR;
    if (a != 2 && a != 5)
    {
        a = (u8)((a + 1) & 4);
        P_LASTDIR = a;
        if (!a)
        {
            R(0x23CC) |= 0x08;
            R(0x23C7) = (u8)(P_XL - 2);
        }
        else
        {
            R(0x23CC) &= (u8)~0x08;
            R(0x23C7) = (u8)(P_XL + 2);
        }
    }
    P_DIR = P_LASTDIR;
    if (!(P_ATTACKREQ | P_ATTACKING)) { move(); return; }
    if (P_MAGICSEL && (Z_PRESS & 0x01))
    {
        P_JUMPREQ = 0;
        P_JUMPING = 0;
        ss2_magic_cast();                   /* $D1E8 ... JMP $CC35 */
    }
    attack();
}

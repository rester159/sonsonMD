/* SON SON II doors, rebuilt from HuCard bank 2 $7DB5-$7F71 (item kind 1).
 *
 * Holding Up on a door for 40 frames enters it: a locked door ($297A)
 * first needs the key ($22C1 bit 0) and slides open; then the stage fades
 * out, the player is placed at the destination from table $4F9C, all
 * objects are dropped and respawned around it, the screen is redrawn and
 * faded in, and the destination's sprite sets and music ($4FAA) load.
 * The sequence runs inside the main task, waiting frames like the source,
 * and ends by restarting that task at $EDB0.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"
#include "ss2_task.h"

u8 ss2_redraw_request;                       /* $EBE1: full playfield redraw */

static u16 rom_word(u16 a)
{
    return (u16)(ss2_rom_byte(a) | (ss2_rom_byte((u16)(a + 1)) << 8));
}

/* $FF8E with VCE address $01D0, $30 = $10: one 8-colour palette into
 * sprite palette 13 (the other 8 entries cleared) */
static void sprite_pal13(u16 src)
{
    u8 i;
    for (i = 0; i < 8; i++)
        ss2_vce[0x1D0 + i] = (u16)(rom_word((u16)(src + i * 2)) & 0x1FF);
    for (i = 8; i < 16; i++) ss2_vce[0x1D0 + i] = 0;
    ss2_vce_dirty = 1;
}

/* $7DE3: go through the door in slot x */
static void enter(u8 x)
{
    u16 t;
    u8 d, y, code, mus;
    if (O(0x297A, x))
    {
        /* locked: slide the door up, print the opened message, use the key */
        ss2_sfx(0x1D);
        for (y = 0x23; y; y--)
        {
            ss2_wait(1);                     /* $B9B8 */
            O(0x24FA, x)--;
            /* $8732: extra SATB entries -- presentation */
        }
        if (O(0x297A, x) == 2) ZP(0x7F) = 1;     /* boss door: arm the boss */
        ss2_hud_text(0x4E86);                /* $BADE: clear the key box */
        R(0x2C1A + O(0x281A, x)) = 0xFF;
        R(0x22C1) &= (u8)~1;
        ss2_wait(30);                        /* $B9D4 */
    }
    ss2_sfx(0x1E);
    ZP(0xBA) = (u8)((ZP(0xBA) & ~1) | (O(0x297A, x) == 2));
    ZP(0x6F) = 0;
    if (!ZP(0x64)) ss2_fade_out(); else ss2_fade_out_all();

    /* destination: 4 bytes (x hi, x page, y hi, y page) per door id */
    t = rom_word((u16)(0x4F9C + (u8)(Z_STAGE << 1)));
    ZP(0x26) = (u8)t; ZP(0x27) = (u8)(t >> 8);
    d = (u8)(O(0x281A, x) - 0x20);
    y = (u8)(d << 2);
    ZP(0x46) = ZP(0x4C) = ss2_rom_byte((u16)(t + y));
    ZP(0x45) = ZP(0x4B) = ss2_rom_byte((u16)(t + (u8)(y + 1)));
    ZP(0x47) = ZP(0x4D) = 0;
    ZP(0x49) = ZP(0x4F) = ss2_rom_byte((u16)(t + (u8)(y + 2)));
    ZP(0x48) = ZP(0x4E) = ss2_rom_byte((u16)(t + (u8)(y + 3)));
    ZP(0x4A) = ZP(0x50) = 0;

    for (y = 0; y < 0x20; y++)
        if (O(0x245A, y))
        {
            R(0x2B5A + O(0x281A, y))--;
            O(0x245A, y) = 0;
        }
    R(0x2299) = R(0x22E3) = 0x70;
    R(0x229C) = R(0x22E4) = 0xA0;
    R(0x22E5) = R(0x22E6) = R(0x22E7) = R(0x22E8) = 0;
    ss2_shots_update();                      /* $D03D with bank 9 mapped; MPR6 back to $8E */

    ZP(0x88)++;
    for (Z_FRAME = 0; ; Z_FRAME++)
    {
        ss2_spawn_scan();
        if (Z_FRAME == 2) break;
    }
    ZP(0x88) = 0;
    for (y = 0; y < 0x20; y++)
        if (O(0x245A, y))
        {
            ss2_obj_next = y;
            ss2_obj_update(y);
            y = ss2_obj_next;
        }
    /* $8732: presentation */
    ss2_wait(12);                            /* $B9C8 */
    ss2_redraw_request = 1;                  /* $EBE1: 32 map rows take */
    ss2_busy(5);                             /* ~5.5 frames: 5 VBlanks pass */
    if (!ZP(0x64)) ss2_fade_in();
    else { ss2_fade_in_all(); ZP(0x64) = 0; }

    /* $4FAA: per door (sprite sets, music) */
    t = rom_word((u16)(0x4FAA + (u8)(Z_STAGE << 1)));
    ZP(0x26) = (u8)t; ZP(0x27) = (u8)(t >> 8);
    y = (u8)(d << 1);
    mus = ss2_rom_byte((u16)(t + (u8)(y + 1)));
    code = ss2_rom_byte((u16)(t + y));
    if (code == 0xFF)
    {
        ss2_sprset_stage();                  /* back outside */
        ss2_staff_art();
    }
    else if (code)
    {
        u16 l = rom_word((u16)(0x53E4 + (u8)((code & 0x0F) - 2)));
        ZP(0x28) = (u8)l; ZP(0x29) = (u8)(l >> 8);
        ss2_sprset_interior();
        if (code & 0xF0)
        {
            l = (u16)(0x493C + (code & 0xF0) - 0x10);
            ZP(0x28) = (u8)l; ZP(0x29) = (u8)(l >> 8);
            ZP(0x30) = 0x10;
            sprite_pal13(l);
            ss2_pal13_variant = (u8)((code >> 4) - 1);
        }
    }
    if (mus)
        ss2_sfx(mus == 0xFF ? (u8)(Z_STAGE + 3) : mus);
    ss2_wait(2);                             /* $B9BC */
    ss2_main_restart();                      /* PLA x4, JMP $EDB0 */
}

/* $7DB5: item kind 1, a door the player is touching */
void ss2_door(u8 x)
{
    if ((R(0x22AB) | R(0x22A9))
        || (O(0x297A, x) && !(R(0x22C1) & 1))
        || (Z_PAD & 0x10) != 0x10 || (Z_PRESS & 0x10))
    {
        ZP(0x6F) = 0;
        return;
    }
    if (++ZP(0x6F) >= 0x28) enter(x);
}

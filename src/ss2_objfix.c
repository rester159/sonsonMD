/* SON SON II objects in fixed bank 3 ($8000-$9FFF) outside the item system
 * of ss2_items.c, rebuilt from HuCard bank 3 $8AAE-$8FF5, $935E-$9427 and
 * $9516-$959D.
 *
 *   $8AAE-$8AC2  moving platforms (fixed, bobbing vertically/horizontally,
 *                falling); $297A swing period, $299A swing timer
 *   $8C8A-$8C94  scenery objects ($297A variant)
 *   $8D1C-$8D52  breakable blocks hiding an item ($297A item kind);
 *                $76 counts the player's remaining block breaks
 *   $935E-$93B2  items of kind $0A and above placed in the level
 *
 * Presentation side effects (BAT writes of the broken block and of the
 * status texts through $BADE) are reported to the renderer through
 * ss2_map_cell_dirty (and ss2_hud_text) instead of touching VRAM.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"

#define OX16(x)     ((u16)((O(0x247A, x) << 8) | O(0x249A, x)))
#define OY16(x)     ((u16)((O(0x24DA, x) << 8) | O(0x24FA, x)))

/* presentation requests: map RAM offset ($3000+) of the last cell cleared
 * by a broken block (0xFFFF none) and the last $BADE text table pointer */
u16 ss2_map_cell_dirty = 0xFFFF;

static void set_ox16(u8 x, u16 v) { O(0x247A, x) = (u8)(v >> 8); O(0x249A, x) = (u8)v; }
static void set_oy16(u8 x, u16 v) { O(0x24DA, x) = (u8)(v >> 8); O(0x24FA, x) = (u8)v; }
static void set_vx(u8 x, u16 v) { O(0x25BA, x) = (u8)(v >> 8); O(0x25DA, x) = (u8)v; }
static void set_vy(u8 x, u16 v) { O(0x25FA, x) = (u8)(v >> 8); O(0x261A, x) = (u8)v; }
static void set_script(u8 x, u16 p) { O(0x295A, x) = (u8)p; O(0x293A, x) = (u8)(p >> 8); }
static void set_handler(u8 x, u16 h) { O(0x28DA, x) = (u8)h; O(0x28BA, x) = (u8)(h >> 8); }
static void set_hit(u8 x, u16 h) { O(0x291A, x) = (u8)h; O(0x28FA, x) = (u8)(h >> 8); }

static void place(u8 x)
{
    O(0x247A, x) = ZP(0x40); O(0x249A, x) = ZP(0x41);
    O(0x24DA, x) = ZP(0x42); O(0x24FA, x) = ZP(0x43);
}

/* ---- items (copies of the bank 3 item setup used by ss2_items.c) ------ */

/* $9125/$94DB: item kind setup */
static void item_kind(u8 x, u8 kind)
{
    O(0x2A1A, x) = (u8)(kind & 0x80);
    kind &= 0x7F;
    O(0x26BA, x) = kind;
    if (kind < 0x0A)
    {
        u8 i = (u8)(kind - 3), t = (u8)(i * 6);
        O(0x295A, x) = ss2_rom_byte((u16)(0x921A + t));
        O(0x293A, x) = ss2_rom_byte((u16)(0x921B + t));
        O(0x29BA, x) = ss2_rom_byte((u16)(0x921C + t));
        O(0x299A, x) = ss2_rom_byte((u16)(0x921D + t));
        O(0x29FA, x) = ss2_rom_byte((u16)(0x921E + t));
        O(0x29DA, x) = ss2_rom_byte((u16)(0x921F + t));
        O(0x255A, x) |= ss2_rom_byte((u16)(0x9244 + i));
    }
    else
    {
        u8 i = (u8)(kind - 0x0A), t = (u8)(i << 2);
        O(0x295A, x) = ss2_rom_byte((u16)(0x95CA + t));
        O(0x293A, x) = ss2_rom_byte((u16)(0x95CB + t));
        O(0x29FA, x) = ss2_rom_byte((u16)(0x95CC + t));
        O(0x29DA, x) = ss2_rom_byte((u16)(0x95CD + t));
        O(0x255A, x) |= ss2_rom_byte((u16)(0x9616 + i));
    }
}

/* $9032/$93D6 (entered after the spawner check): an item of kind ZP($35)
 * at ($40-$43) for spawner $44. Returns the slot, or $20 when the alloc
 * failed (the source leaves X = $20). */
static u8 spawn_item(void)
{
    u8 s = ZP(0x44), kind = ZP(0x35), x;
    x = ss2_obj_alloc();
    if (x == 0xFF) return 0x20;
    ss2_obj_init(x, (kind & 0x7F) < 0x0A ? 0x91EE : 0x959E);
    set_vx(x, 0);
    set_vy(x, 0);
    place(x);
    O(0x287A, x) = 0;
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    item_kind(x, kind);
    return x;
}

/* $935E-$93CC: an item of kind $0B-$1C placed in the level */
static void place_item(u8 kind)
{
    ZP(0x35) = kind;
    if (O(0x2B5A, ZP(0x44))) return;
    spawn_item();
}

/* $9416: Z clear when this stage's life vessel was already taken */
static u8 vessel_taken(void)
{
    u8 b = 1, k;
    for (k = Z_STAGE; k; k--) b = (u8)(b << 1);
    ZP(0x37) = b;
    return (u8)(b & ZP(0x7C));
}

/* $9516: falling item of kind $0A+ (same logic as $9173) */
static void item_fall(u8 x)
{
    O(0x263A, x) = 0x08;
    O(0x265A, x) = (s8)O(0x25FA, x) < 0 ? 0x00 : 0x10;
    if (!ss2_obj_probe(x))
    {
        u16 t = (u16)(O(0x261A, x) + 0x40);
        O(0x261A, x) = (u8)t;
        O(0x25FA, x) = (u8)(O(0x25FA, x) + (t >> 8));
    }
    else if ((s8)O(0x25FA, x) < 0)
    {
        set_vy(x, 0);
    }
    else if (!O(0x297A, x))
    {
        set_vy(x, 0xFC00);
        O(0x297A, x) = 1;
    }
    else
    {
        set_vy(x, 0);
        if (!O(0x2A1A, x))
        {
            O(0x267A, x) = 1;
            O(0x293A, x) = O(0x29DA, x);
            O(0x295A, x) = O(0x29FA, x);
        }
        set_handler(x, 0xEE67);
    }
    ss2_obj_common(x);
}

/* $9591: item of kind $0A+ struck: play the $983D script */
static void item_hit(u8 x)
{
    set_script(x, 0x983D);
    ss2_obj_common(x);
}

/* ---- moving platforms ($8AAE-$8C74) ------------------------------------ */

/* $8C2F: leave the world 0x60 px beyond the screen sides / 0x30 above */
static void platform_bounds(u8 x)
{
    if ((u16)(OX16(x) + 0x60) >= 0x1C0 || (u16)(OY16(x) + 0x30) >= 0x160)
    {
        /* $8C66 */
        O(0x245A, x) = 0;
        O(0x2B5A, O(0x281A, x)) = 0;
        return;
    }
    ss2_obj_common(x);
}

/* $8BD7: horizontal swing */
static void platform_h(u8 x)
{
    if (!--O(0x299A, x))
    {
        O(0x299A, x) = O(0x297A, x);
        set_vx(x, (u16)(0 - (u16)((O(0x25BA, x) << 8) | O(0x25DA, x))));
    }
    platform_bounds(x);
}

/* $8BF3: vertical swing */
static void platform_v(u8 x)
{
    if (!--O(0x299A, x))
    {
        O(0x299A, x) = O(0x297A, x);
        set_vy(x, (u16)(0 - (u16)((O(0x25FA, x) << 8) | O(0x261A, x))));
    }
    platform_bounds(x);
}

/* $8C0F: falls (accelerating to 5 px/frame) once something set it moving */
static void platform_fall(u8 x)
{
    if ((O(0x261A, x) | O(0x25FA, x)) && O(0x25FA, x) != 0x05)
    {
        u16 t = (u16)(O(0x261A, x) + 0x20);
        O(0x261A, x) = (u8)t;
        O(0x25FA, x) = (u8)(O(0x25FA, x) + (t >> 8));
    }
    platform_bounds(x);
}

/* $8AAE-$8AC2 -> $8ACC: platform type y (0 fixed, 1 down, 2 up, 3 right,
 * 4 left, 5 falling; type 5 waits for the spawner's respawn timer) */
static void spawn_platform(u8 type)
{
    u8 s = ZP(0x44), x;
    if (type == 5 && O(0x2C1A, s)) return;
    if (O(0x2B5A, s)) return;
    x = ss2_obj_alloc();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0x8BC1);
    place(x);
    switch (type)                       /* JMP ($8AFE,X) */
    {
        case 0:                         /* $8B0A */
            set_vx(x, 0);
            set_vy(x, 0);
            O(0x297A, x) = 0;
            O(0x299A, x) = 0;
            set_handler(x, 0x8C2F);
            break;
        case 1:                         /* $8B2A */
        case 2:                         /* $8B37 */
            set_vy(x, type == 1 ? 0x0100 : 0xFF00);
            set_vx(x, 0);               /* $8B42 */
            O(0x297A, x) = 0x40;
            O(0x299A, x) = 0x20;
            set_handler(x, 0x8BF3);
            break;
        case 3:                         /* $8B5E */
        case 4:                         /* $8B6B */
            set_vx(x, type == 3 ? 0x0100 : 0xFF00);
            set_vy(x, 0);               /* $8B76 */
            O(0x297A, x) = 0x60;
            O(0x299A, x) = 0x30;
            set_handler(x, 0x8BD7);
            break;
        default:                        /* $8B92 */
            set_vx(x, 0);
            set_vy(x, 0);
            set_handler(x, 0x8C0F);
            O(0x299A, x) = 0x88;
            break;
    }
    /* $8BAE */
    O(0x287A, x) = 1;
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    ss2_obj_common(x);
}

/* ---- scenery ($8C8A-$8CFB) -------------------------------------------- */

static void spawn_scenery(u8 v)
{
    u8 s = ZP(0x44), x;
    ZP(0x35) = v;
    if (O(0x2B5A, s)) return;
    if (O(0x2C1A, s)) ZP(0x35) = 0;
    x = ss2_obj_alloc_top();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0x8CFC);            /* $8CAB */
    set_vx(x, 0);
    set_vy(x, 0);
    place(x);
    if (ZP(0x35)) set_script(x, 0x8D17);
    O(0x297A, x) = ZP(0x35);
    O(0x287A, x) = 1;
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    ss2_obj_common(x);
}

/* ---- breakable blocks ($8D1C-$8FF5) ----------------------------------- */

static void spawn_block(u8 kind)
{
    u8 s = ZP(0x44), x;
    ZP(0x35) = kind;
    if (O(0x2B5A, s)) return;
    if (O(0x2C1A, s))
    {
        /* $8DAA: already broken: its item, 8 px in */
        x = spawn_item();
        set_ox16(x, (u16)(OX16(x) + 8));
        set_oy16(x, (u16)(OY16(x) + 8));
        return;
    }
    x = ss2_obj_alloc_top();
    if (x == 0xFF) return;
    ss2_obj_init(x, 0x8DD9);            /* $8D67 */
    set_vx(x, 0);
    set_vy(x, 0);
    place(x);
    O(0x287A, x) = 1;
    O(0x297A, x) = ZP(0x35);
    O(0x281A, x) = s;
    O(0x2B5A, s)++;
    ss2_obj_common(x);
}

/* $8DEF: solid only while the player can break blocks */
static void block(u8 x)
{
    O(0x269A, x) = ZP(0x76) ? 0x0D : 0x0F;
    ss2_obj_common(x);
}

/* $8E07: not breakable now: recover */
static void block_recover(u8 x)
{
    O(0x245A, x) = 0x80;
    O(0x267A, x) = 1;
    O(0x26DA, x) = 1;
    ss2_obj_common(x);
}

/* $8E03: block struck */
static void block_hit(u8 x)
{
    u16 wx, wy, off;
    if (!ZP(0x76)) { block_recover(x); return; }
    if (Z_STAGE == 6 && O(0x297A, x) == 0x19 && Z_STAFFPAL != 4) { block_recover(x); return; }
    /* $8E2A */
    if (!--ZP(0x76))
    {
        ss2_hud_text(0x4EAA);           /* $BADE */
        P_FLAGSC1 &= (u8)~0x10;
    }
    else if (ZP(0x76) == 1)
    {
        ss2_hud_text(0x4E62);
    }
    /* $8E55 */
    O(0x267A, x) = 1;
    O(0x269A, x) |= 0x02;
    set_hit(x, 0xEE67);
    set_script(x, 0x8FFB);
    if (O(0x297A, x))
    {
        u8 s = O(0x281A, x);
        O(0x2B5A, s) = 0;
        O(0x2C1A, s) = 0xFF;
    }
    /* $8E8B: clear the map cell under the block's centre */
    wx = (u16)(OX16(x) + 0x10 + ((Z_CAMX_HI << 8) | Z_CAMX_LO));
    wy = (u16)(OY16(x) + 0x10 + ((Z_CAMY_HI << 8) | Z_CAMY_LO));
    off = (u16)((((wy >> 8) & 7) << 9) | (((wx >> 8) & 7) << 6) | ((wy & 0xE0) >> 2) | ((wx & 0xE0) >> 5));
    R(0x3000 + off) = 0;
    ss2_map_cell_dirty = off;
    /* $8EFA-$8FE1 redraws the 32x32 block in the BAT: presentation */
    /* $8FE2 */
    set_oy16(x, (u16)(OY16(x) - 8));
    ss2_obj_common(x);
}

u8 ss2_objfix_handler(u16 addr, u8 x)
{
    switch (addr)
    {
        case 0x8BD7: platform_h(x); return 1;
        case 0x8BF3: platform_v(x); return 1;
        case 0x8C0F: platform_fall(x); return 1;
        case 0x8C2F: platform_bounds(x); return 1;
        case 0x8DEF: block(x); return 1;
        case 0x8E03: block_hit(x); return 1;
        case 0x9516: item_fall(x); return 1;
        case 0x9591: item_hit(x); return 1;
    }
    return 0;
}

u8 ss2_spawnfix_handler(u16 addr)
{
    switch (addr)
    {
        case 0x8AAE: spawn_platform(0); return 1;
        case 0x8AB2: spawn_platform(1); return 1;
        case 0x8AB6: spawn_platform(2); return 1;
        case 0x8ABA: spawn_platform(3); return 1;
        case 0x8ABE: spawn_platform(4); return 1;
        case 0x8AC2: spawn_platform(5); return 1;
        case 0x8C8A: spawn_scenery(0); return 1;
        case 0x8C8E: spawn_scenery(1); return 1;
        case 0x8C94: spawn_scenery(2); return 1;
        case 0x8D1C: spawn_block(0x00); return 1;
        case 0x8D22: spawn_block(0x18); return 1;
        case 0x8D28: spawn_block(0x10); return 1;
        case 0x8D2E: if (!vessel_taken()) spawn_block(0x0F); return 1;
        case 0x8D3A: spawn_block(0x19); return 1;
        case 0x8D40: spawn_block(0x06); return 1;
        case 0x8D46: spawn_block(0x07); return 1;
        case 0x8D4C: spawn_block(0x08); return 1;
        case 0x8D52: spawn_block(0x09); return 1;
        case 0x9376: if (!vessel_taken()) place_item(0x0F); return 1;
    }
    if (addr >= 0x935E && addr <= 0x9370 && !((addr - 0x935E) % 6))
    {
        place_item((u8)(0x0B + (addr - 0x935E) / 6));
        return 1;
    }
    if (addr >= 0x9382 && addr <= 0x93CA && !((addr - 0x9382) % 6))
    {
        place_item((u8)(0x10 + (addr - 0x9382) / 6));
        return 1;
    }
    return 0;
}

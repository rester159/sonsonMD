/* SON SON II sprite pattern sets, rebuilt from HuCard bank 2 $774F-$787F.
 *
 * A set list is a run of 6-byte records (bank, cells, source lo/hi, RAM
 * slot lo/hi) ended by $FF. Each record stores the current pattern base
 * ($2FA1:$2FA2, a SATB pattern word = cell * 2) into its RAM slot -- the
 * animation bases objects read -- advances the base by cells * 2, and
 * streams the 3-plane cells (0x60 bytes each, plane 3 zero) into VRAM.
 * The VRAM side is recorded natively: ss2_cell_bank/src name the ROM
 * pattern now held by each PCE sprite cell, for the sprite renderer.
 */
#include "ss2_ram.h"
#include "ss2_logic.h"

u8 ss2_cell_bank[0x200];
u16 ss2_cell_src[0x200];
u8 ss2_cells_dirty;

/* $7803: load the set list at `list` (a bank 1-4 / current-bank address)
 * starting at PCE sprite cell `cell` (the caller's MAWR / 64); returns the
 * cell after the last one written (where MAWR has auto-incremented to). */
static u16 load_sets(u16 list, u16 cell)
{
    ZP(0x8B) = 0x02;                            /* $BA4C: VDC register VWR */
    for (;;)
    {
        u8 bank = ss2_rom_byte(list), n;
        u16 src, slot, t;
        if (bank == 0xFF) return cell;
        ss2_page[6] = ss2_banks[bank & 0x1F];   /* TAM #$40 (no $8E, no restore) */
        ss2_mpr6 = bank;
        n = ss2_rom_byte((u16)(list + 1));
        R(0x2FA0) = n;
        src = (u16)(ss2_rom_byte((u16)(list + 2)) | (ss2_rom_byte((u16)(list + 3)) << 8));
        slot = (u16)(ss2_rom_byte((u16)(list + 4)) | (ss2_rom_byte((u16)(list + 5)) << 8));
        ZP(0x2A) = (u8)src; ZP(0x2B) = (u8)(src >> 8);
        ZP(0x2C) = (u8)slot; ZP(0x2D) = (u8)(slot >> 8);
        R(slot) = R(0x2FA1);
        R(slot + 1) = R(0x2FA2);
        t = (u16)((u8)(n << 1) + R(0x2FA2) + (n >> 7));
        R(0x2FA2) = (u8)t;
        R(0x2FA1) = (u8)(R(0x2FA1) + (t >> 8));
        do
        {
            ss2_cell_bank[cell & 0x1FF] = bank;
            ss2_cell_src[cell & 0x1FF] = src;
            cell++;
            src = (u16)(src + 0x60);
        } while (--R(0x2FA0));
        ZP(0x2A) = (u8)src; ZP(0x2B) = (u8)(src >> 8);
        ss2_cells_dirty = 1;
        list = (u16)(list + 6);
        ZP(0x28) = (u8)list; ZP(0x29) = (u8)(list >> 8);
    }
}

/* $774F: the stage's default sets at cell $104 (base $0208) */
void ss2_sprset_stage(void)
{
    u8 y = (u8)(Z_STAGE << 1);
    u16 cell;
    R(0x2FA2) = 0x08;
    R(0x2FA1) = 0x02;
    cell = load_sets(ZP(0x8A) ? 0x55B5 : 0x52DA, 0x104);
    load_sets((u16)(ss2_rom_byte((u16)(0x52CC + y)) | (ss2_rom_byte((u16)(0x52CD + y)) << 8)), cell);
    R(0x2D9B) = 0;
    R(0x2D9A) = 2;
}

/* $77A5: the interior set list at ($28) at cell $15A (base $02B4) */
void ss2_sprset_interior(void)
{
    u16 list = (u16)(ZP(0x28) | (ZP(0x29) << 8));
    R(0x2FA2) = 0xB4;
    R(0x2FA1) = 0x02;
    load_sets(list, 0x15A);
    R(0x2D9B) = 0;
    R(0x2D9A) = 2;
}

/* $77D0: set list $5548 at cell $108 (base $0210) */
void ss2_sprset_5548(void)
{
    R(0x2FA2) = 0x10;
    R(0x2FA1) = 0x02;
    load_sets(0x5548, 0x108);
    R(0x2D9B) = 0;
    R(0x2D9A) = 2;
}

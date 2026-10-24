/* SON SON II logic support: RAM image, ROM table mapping, flow hooks. */
#include "ss2_ram.h"
#include "ss2_logic.h"

u8 ss2_ram[0x2000];
u8 ss2_staff_sprites_hidden;
u16 ss2_unported_last;
u16 ss2_unported_count;
u8 ss2_died;

/* banks without exported data read as zeros; ss2_map6 reports them */
static const u8 missing[0x2000];

const u8 *const ss2_banks[0x20] = {
    ss2_bank0, ss2_bank1, ss2_bank2, ss2_bank3, ss2_bank4, ss2_bank5, ss2_bank6, ss2_bank7,
    ss2_bank8, ss2_bank9, missing, missing, missing, missing, missing, ss2_bankF,
    missing, missing, missing, missing, missing, missing, missing, ss2_bank17,
    missing, missing, missing, missing, missing, missing, missing, ss2_bank1F };

const u8 *ss2_page[8] = {
    missing, ss2_ram, ss2_bank1, ss2_bank2, ss2_bank3, ss2_bank4, ss2_bank9, ss2_bank0 };

u8 ss2_unported_bank;

void ss2_unported(u16 source_addr)
{
    ss2_unported_last = source_addr;
    ss2_unported_bank = ss2_mpr6;
    ss2_unported_count++;
}

void ss2_player_died(void)
{
    ss2_died = 1;
}

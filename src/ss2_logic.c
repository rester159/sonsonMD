/* SON SON II logic support: RAM image, ROM table mapping, flow hooks. */
#include "ss2_ram.h"
#include "ss2_logic.h"

u8 ss2_ram[0x2000];
u8 ss2_staff_sprites_hidden;
u16 ss2_unported_last;
u16 ss2_unported_count;
u8 ss2_died;

u8 ss2_rom_byte(u16 addr)
{
    u16 o = (u16)(addr & 0x1FFF);
    switch (addr >> 13)
    {
        case 2: return ss2_bank1[o];
        case 3: return ss2_bank2[o];
        case 4: return ss2_bank3[o];
        case 5: return ss2_bank4[o];
        case 6: return ss2_bank9[o];
        case 7: return ss2_bank0[o];
    }
    ss2_unported(addr);
    return 0;
}

u8 ss2_metatile_byte(u16 addr, u8 index)
{
    u16 o = (u16)((addr - 0xC000 + index) & 0x1FFF);
    return (Z_STAGE & 4) ? ss2_bank7[o] : ss2_bank6[o];
}

void ss2_unported(u16 source_addr)
{
    ss2_unported_last = source_addr;
    ss2_unported_count++;
}

void ss2_player_died(void)
{
    ss2_died = 1;
}

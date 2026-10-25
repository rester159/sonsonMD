/* SON SON II logic support: RAM image, ROM table mapping, flow hooks. */
#include "ss2_ram.h"
#include "ss2_logic.h"
#include "ss2_task.h"

u8 ss2_ram[0x2000];
u8 ss2_staff_sprites_hidden;
u16 ss2_unported_last;
u16 ss2_unported_count;
u8 ss2_died;

/* banks without exported data read as zeros; ss2_map6 reports them */
static const u8 missing[0x2000];

const u8 *ss2_banks[0x20] = {          /* banks 1-2: ss2_set_language */
    ss2_bank0, ss2_bank1, ss2_bank2, ss2_bank3, ss2_bank4, ss2_bank5, ss2_bank6, ss2_bank7,
    ss2_bank8, ss2_bank9, missing, missing, ss2_bankC, ss2_bankD, ss2_bankE, ss2_bankF,
    ss2_bank10, ss2_bank11, ss2_bank12, ss2_bank13, ss2_bank14, ss2_bank15, ss2_bank16, ss2_bank17,
    ss2_bank18, ss2_bank19, ss2_bank1A, ss2_bank1B, ss2_bank1C, ss2_bank1D, ss2_bank1E, ss2_bank1F };

const u8 *ss2_page[8] = {
    missing, ss2_ram, ss2_bank1, ss2_bank2, ss2_bank3, ss2_bank4, ss2_bank9, ss2_bank0 };

u8 ss2_unported_bank;

void ss2_unported(u16 source_addr)
{
    ss2_unported_last = source_addr;
    ss2_unported_bank = ss2_mpr6;
    ss2_unported_count++;
}

/* bank 9 $C322: JMP $F884 from inside the main task (outside a task, as in
 * the per-frame host replay, only the flag) */
void ss2_player_died(void)
{
    ss2_died = 1;
    if (ss2_task_current() != 0xFF) ss2_task_restart(ss2_death_task);
}

/* SON SON II objects in fixed bank 3 ($8000-$9FFF), excluding the item system in ss2_items.c.
 * Not yet rebuilt: every entry reports 0 so the engine records the
 * address through ss2_unported(). */
#include "ss2_ram.h"
#include "ss2_logic.h"

u8 ss2_objfix_handler(u16 addr, u8 x)
{
    (void)addr;
    (void)x;
    return 0;
}

u8 ss2_spawnfix_handler(u16 addr)
{
    (void)addr;
    return 0;
}

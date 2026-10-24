/* SON SON II objects in HuCard bank $09 at $C000.
 * Not yet rebuilt: every entry reports 0 so the engine records the
 * address through ss2_unported(). */
#include "ss2_ram.h"
#include "ss2_logic.h"

u8 ss2_obj09_handler(u16 addr, u8 x)
{
    (void)addr;
    (void)x;
    return 0;
}

u8 ss2_spawn09_handler(u16 addr)
{
    (void)addr;
    return 0;
}

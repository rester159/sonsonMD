#include "arcade_scroll.h"
u8 arcade_scroll_step(u16 *camera,u16 *fraction,u8 *column_latch)
{
    u16 sum=(u16)((*fraction&255)+0x8d);
    *camera=(u16)(*camera+(sum>>8));
    *fraction=sum&255;
    if((*camera&8)!=*column_latch)return 0;
    *column_latch^=8;
    return 1;
}

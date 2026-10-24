#ifndef ARCADE_SCROLL_H
#define ARCADE_SCROLL_H
#include "arcade_types.h"
/* A697 running-scroll arithmetic. Caller owns stop states and column events. */
u8 arcade_scroll_step(u16 *camera,u16 *fraction,u8 *column_latch);
#endif

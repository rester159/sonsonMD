#ifndef ARCADE_PRELUDE_DISPLAY_H
#define ARCADE_PRELUDE_DISPLAY_H
#include "arcade_prelude_render.h"
/* Cold title-scene resource ownership. Uses the existing captured title
 * background, with bottom UI rows fitted into224lines; source command-by-command background reconstruction is separate.
 * Begin flushes prior DMA with display disabled. Publish is called at VBlank
 * by the owner; synchronous transfers have no borrowed-buffer lifetime. */
void arcade_prelude_display_begin(void);
void arcade_prelude_display_hud(u32 high,u16 credits,u8 free_play,u8 press_start);
u8 arcade_prelude_display_publish(const ArcadePrelude *);
#endif

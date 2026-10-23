#ifndef ARCADE_PRELUDE_RENDER_H
#define ARCADE_PRELUDE_RENDER_H
#include "arcade_prelude.h"
#include "arcade_cell_sat.h"
/* Append source-order players then decorations using resident life/bonus art.
 * Caller loads art and terminates the final SAT link. Returns65535 without
 * writes if capacity or pose is invalid, otherwise the emitted entry count. */
u16 arcade_prelude_sat(const ArcadePrelude *,const ArcadeSpriteView *,u16 *sat,u16 first,u16 limit);
#endif

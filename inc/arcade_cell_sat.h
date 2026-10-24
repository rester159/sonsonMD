#ifndef ARCADE_CELL_SAT_H
#define ARCADE_CELL_SAT_H
#include "arcade_large.h"
/* Explicit arcade-coordinate viewport; its dimensions must fit the selected
 * Genesis display mode. Preserves original independent X/Y wrap copies. */
typedef struct {s16 x,y;u16 width,height;} ArcadeSpriteView;
/* Convert one source hardware cell to up to three Genesis SAT entries.
 * tile is a ready 2x2 pattern block. Returns 65535 on insufficient capacity,
 * without writes; otherwise returns the number emitted. Appended entries link
 * onward; the renderer must terminate the final entry of the complete list. */
u16 arcade_cell_sat(const ArcadeLargeCell *cell,u16 tile,
                    const ArcadeSpriteView *view,u16 *sat,u16 first,u16 limit);
#endif

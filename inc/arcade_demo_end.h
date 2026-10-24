#ifndef ARCADE_DEMO_END_H
#define ARCADE_DEMO_END_H
#include "arcade_types.h"
typedef struct {u8 phase,timer;} ArcadeDemoEnd;
enum {ARCADE_DEMO_END_SOUND=1,ARCADE_DEMO_END_DONE=2};
/* Set phase1 after97AB detects both out. Call only on subsequent non-frozen
 * source updates, instead of player/input/world dispatch. 97C6 falls through
 * to97D2 on its setup tick;97E3 exits on the tick after timer expiry. */
u8 arcade_demo_end_step(ArcadeDemoEnd *,u8 clock);
#endif

#ifndef ARCADE_PACING_H
#define ARCADE_PACING_H
#include "arcade_types.h"
/* Source55.4Hz over a non-interlaced Genesis refresh clock. Pending updates are
 * never discarded by a caller's per-display work limit. game_run currently consumes at most two per display pass. */
typedef struct {u32 phase,pending,numerator,denominator;u16 last_refresh;} ArcadePacing;
void arcade_pacing_init(ArcadePacing *p,u8 pal,u16 refresh);
/* Call at least once per65535 refreshes, including throughout Home pause.
 * paused advances the host anchor only, preserving source phase and prior debt. */
void arcade_pacing_observe(ArcadePacing *p,u16 refresh,u8 paused);
u8 arcade_pacing_take(ArcadePacing *p);
/* Shared Arcade scene owner. First entry initializes; later entries preserve
 * fractional phase and pending work, re-anchoring only the refresh counter.
 * Scene setup intervals remain excluded until original IRQ/queue timing is
 * recovered. Region is fixed for the cartridge session. Home uses its own p. */
ArcadePacing *arcade_pacing_enter(u8 pal,u16 refresh);
#endif

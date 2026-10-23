#ifndef ARCADE_RESULTS_DISPLAY_H
#define ARCADE_RESULTS_DISPLAY_H
#include "arcade_results.h"
/* Cold entry/exit only: display disabled, prior DMA flushed. Uses the bonus
 * art arena; gameplay bonus rendering must be suspended until restore. */
void arcade_results_display_begin(void);
void arcade_results_display_restore(void);
void arcade_results_display_event(const ArcadeResultsEvent *event);
void arcade_results_display_background(void);
u8 arcade_results_display_pending(void);
/* Append decorations after retained player/marker cells. Caller terminates SAT. */
u16 arcade_results_display_sat(u16 *sat,u16 first,u16 limit);
#endif

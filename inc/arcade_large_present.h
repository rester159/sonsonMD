#ifndef ARCADE_LARGE_PRESENT_H
#define ARCADE_LARGE_PRESENT_H
#include "arcade_cell_sat.h"
#include "arcade_large_upload.h"
/* cells/mask use stable ownership: actor0 cells0..3, actor1 cells4..7,
 * actor2 cells8..11, shared projectile slots12..15. Source sprite-budget
 * admission happens before this function. limit is the exclusive SAT limit.
 * Returns appended count or65535. Failure leaves caller SAT unchanged.
 * Must run after the previous presentation queue flushed. Reservations cover
 * later work, including the SAT upload. Caller owns final chain termination. */
u16 arcade_large_present(const ArcadeLargeCell cells[16],u16 visible,
                         const ArcadeSpriteView *view,u16 *sat,u16 first,
                         u16 limit,u16 reserve_bytes,u16 reserve_ops);
/* Shared K owners may use already-resident art. Four tile bases correspond to
 * cells12..15;65535 selects dynamic art. NULL selects dynamic art for all. */
u16 arcade_large_present_mixed(const ArcadeLargeCell cells[16],u16 visible,
                         const u16 resident_children[4],const ArcadeSpriteView *view,
                         u16 *sat,u16 first,u16 limit,u16 reserve_bytes,u16 reserve_ops);
#endif

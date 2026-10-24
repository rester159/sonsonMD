#ifndef ARCADE_POWERUP_H
#define ARCADE_POWERUP_H
#include "arcade_types.h"
/* Shared pickup fields; variant grade is owned separately by its spawner. */
typedef struct {
    u16 x;
    u8 y,active,state,phase,kind,placement,timer,anim_timer,cursor,code,attr,exit_latched,owner;
} ArcadePowerup;
void arcade_powerup_step(ArcadePowerup *a,u16 camera);
/* Ordinary kinds0..9: returns 1/2 for a missed-fruit orb request (kind0/1).
 * Caller must bind the orb to this pickup; kind10 has a separate score pose. */
u8 arcade_pickup_step(ArcadePowerup *a,u16 camera);
/* B213 kind10, normal section-clear grade0..6, owner0/1. */
u8 arcade_variant_pickup_step(ArcadePowerup *a,u16 camera,u8 variant);
/* E7F3/E868, FF only. Caller owns player eligibility and source slot order.
 * Conversion is a shared one-update signal, cleared after all enemy updates. */
u8 arcade_powerup_collect_conversion(ArcadePowerup *a,u16 x,u8 y,u8 owner,
                                    u8 *limit,u8 *next,u8 *conversion);
#endif

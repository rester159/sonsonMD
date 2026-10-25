#ifndef ARCADE_FOOD_BONUS_H
#define ARCADE_FOOD_BONUS_H
#include "arcade_rewards.h"
#include "arcade_orb.h"
extern ArcadeOrb arcade_food_carrier;
void arcade_food_carrier_step(u16 camera);
extern ArcadeSpecialSpawn arcade_food_conversion_spawn;
extern ArcadePowerup arcade_food_conversion;
extern u8 arcade_food_orb,arcade_food_conversion_pulse;
void arcade_food_bonus_reset(void);
/* A7D0 request: caller passes the pre-increment section-clear grade0..6. */
void arcade_food_bonus_section(u8 variant);
void arcade_food_bonus_step(u16 camera,u8 blocked,u8 ready,u8 advance);
u8 arcade_food_bonus_contact(u16 x,u8 y,u8 owner,u16 combat,ArcadeBonusAward *award);
void arcade_food_bonus_background(u8 plane_phase);
u16 arcade_food_bonus_sat(u16 camera,u16 *sat,u16 first,u16 limit);
#endif

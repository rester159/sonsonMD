#ifndef ARCADE_FOOD_WORLD_H
#define ARCADE_FOOD_WORLD_H
#include "arcade_food.h"
#include "arcade_shots.h"
#include "arcade_player.h"
#include "arcade_stop.h"
void arcade_food_world_stop(ArcadeStop *stop,u16 camera,u8 clock,ArcadeStopEvent *event);
u16 arcade_food_world_shot(ArcadePlayerShot *shot);
u8 arcade_food_world_player(ArcadePlayer *player,u16 camera);
/* Pending shared pickup request, consumed by the special-pickup owner. */
extern ArcadeSpecialSpawn arcade_food_spawn;
extern ArcadePowerup arcade_food_pickup;
void arcade_food_world_begin(void);
void arcade_food_world_step(u16 camera,u8 advance);
void arcade_food_world_step_clock(u16 camera,u8 advance,u8 clock);
u16 arcade_food_world_contact(u16 camera,u16 x,u8 y,u8 active,u8 owner,u8 grade);
/* Called after background streaming. Plane A is64x32; phase is in columns. */
void arcade_food_world_render(u8 phase,u8 cold);
#endif

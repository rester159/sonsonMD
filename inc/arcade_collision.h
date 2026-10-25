#ifndef ARCADE_COLLISION_H
#define ARCADE_COLLISION_H
#include "arcade_types.h"
#include "arcade_foes.h"
/* G/H shot collision, in source world X and upward internal Y coordinates. */
u8 arcade_gh_shot_overlap(u16 enemy_x,u8 enemy_y,u16 shot_x,u8 shot_y);
u8 arcade_gh_shot_target(const ArcadeFoe *foes,u8 family,u16 shot_x,u8 shot_y);
/* Ordinary hostile contact only; bonus-object collection is separate. */
u8 arcade_body_contact(u16 player_x,u8 player_y,u16 enemy_x,u8 enemy_y,
                       u8 player_active,u8 enemy_active);
#endif

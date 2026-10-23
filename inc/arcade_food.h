#ifndef ARCADE_FOOD_H
#define ARCADE_FOOD_H
#include "arcade_powerup.h"
#include "arcade_special_spawn.h"
typedef struct {u8 count,total;} ArcadeFoodCounter;
typedef struct {u16 x;u8 active,phase,y,food,kind;} ArcadeFoodTile;
typedef struct {u16 points,event;u8 sound;} ArcadeFoodAward;
/* E569/E5BB, one kind0 tile. Caller scans the20 stable source slots in order
 * and passes each successful contact to arcade_food_collect. */
u8 arcade_food_contact(ArcadeFoodTile *tile,u16 camera,u16 x,u8 y,
                       u8 player_active,u8 owner,ArcadeFoodAward *award);
/* Original E607. Food codes F4..07 are signed indices into the source table.
 * Returns 1 for a new descriptor request, 0 otherwise. Invalid inputs are
 * rejected untouched; callers supply a reachable counter (count0..5). */
u8 arcade_food_collect(ArcadeFoodCounter *counter,u8 food,u8 y,u8 grade,
                       ArcadeSpecialSpawn *spawn,ArcadePowerup *pickup);
#endif

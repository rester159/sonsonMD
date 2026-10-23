#ifndef ARCADE_FOOD_STEP_H
#define ARCADE_FOOD_STEP_H
#include "arcade_world_objects.h"
/* 0=no write,1=first streamed column,2=second column,3=full 2x2 patch.
 * Strip row is y>>3. Character/attribute data belongs to source palette roles;
 * the Genesis renderer translates it rather than using it as tile numbers. */
typedef struct {u8 write,row;const u8 *pattern;} ArcadeFoodDraw;
void arcade_food_step(ArcadeWorldObject *object,u16 camera,u8 advance,ArcadeFoodDraw *draw);
/* B922/B952: restore visible kind0 objects on initial background redraw. */
void arcade_food_redraw(ArcadeWorldObject *object,u16 camera,ArcadeFoodDraw *draw);
#endif

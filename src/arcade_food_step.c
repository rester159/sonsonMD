#include "arcade_food_step.h"
#include "arcade_food_patterns.h"
void arcade_food_redraw(ArcadeWorldObject *object,u16 camera,ArcadeFoodDraw *draw)
{
    u8 index=(u8)(object->food+12);
    draw->write=0;
    if(!(object->active&4) || object->kind || index>=20 ||
       (u16)(camera+256)<=object->x)return;
    draw->write=3;draw->row=object->y>>3;
    draw->pattern=arcade_food_patterns[index];
    object->active=1;object->phase=3;
}
void arcade_food_step(ArcadeWorldObject *object,u16 camera,u8 advance,ArcadeFoodDraw *draw)
{
    u8 index=(u8)(object->food+12);
    draw->write=0;
    if(!object->active || object->kind || index>=20)return;
    draw->row=object->y>>3;draw->pattern=arcade_food_patterns[index];
    switch(object->phase)
    {
        case 0:
            if(!object->y || (u16)(camera+256)<=object->x)return;
            object->phase++;
            /* Source falls through to first-column handling on this update. */
        case 1:
            if(!advance)return;
            draw->write=1;object->phase++;
            break;
        case 2:
            if(!advance)return;
            draw->write=2;object->active=1;object->phase=3;
            break;
        case 4:
            draw->write=3;draw->pattern=arcade_food_eaten_patterns[index];
            object->active=4;object->phase=5;
            break;
    }
}

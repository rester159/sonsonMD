#include "arcade_large.h"
/* FF6A emits column-major cells and admits the whole actor atomically. */
u8 arcade_large_cells(const ArcadeLargeActor *actor,u16 camera,u8 *remaining,
                       ArcadeLargeCell cells[4])
{
    u8 k,x,y;
    if(!actor->body.active || *remaining<4)return 0;
    *remaining-=4;
    x=(u8)(actor->x-camera);y=(u8)(-actor->y-16);
    for(k=0;k<4;k++)
    {
        cells[k].y=(u8)(y-((k&1)?16:0));
        cells[k].attr=actor->body.animation.attr[k];
        cells[k].code=actor->body.animation.code[k];
        cells[k].x=(u8)(x+((k&2)?16:0));
    }
    return 4;
}

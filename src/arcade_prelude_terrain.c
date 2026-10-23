#include "arcade_prelude_terrain.h"
s16 arcade_prelude_floor(u16 x,u8 y,void *context)
{
    const ArcadePreludeTerrain *terrain=context;
    u8 column=(u8)((((u8)(x+8))>>3)+terrain->ring)&31;
    u8 row=y>>3,low=y&15;
    if(!(arcade_prelude_terrain[column*4+(row>>3)]&(128>>(row&7))) || low<2)
        return ARCADE_NO_FLOOR;
    return 7-(s16)low;
}

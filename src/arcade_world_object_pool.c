#include "arcade_world_object_pool.h"
void arcade_world_object_begin(ArcadeWorldObjectPool *pool)
{
    u16 i;
    for(i=0;i<16;i++)pool->objects[i]=arcade_world_objects[i];
    for(;i<20;i++)pool->objects[i].active=0;
    pool->block=4;pool->quarter=4;
}
u8 arcade_world_object_load(ArcadeWorldObjectPool *pool)
{
    u16 i,source,destination;
    if(pool->block>=87 || pool->quarter>=5)return 0;
    source=(u16)pool->block*4;destination=(u16)pool->quarter*4;
    for(i=0;i<4;i++)pool->objects[destination+i]=arcade_world_objects[source+i];
    pool->block++;
    if(++pool->quarter==5)pool->quarter=0;
    return 1;
}

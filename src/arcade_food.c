#include "arcade_food.h"
u8 arcade_food_contact(ArcadeFoodTile *tile,u16 camera,u16 x,u8 y,
                       u8 player_active,u8 owner,ArcadeFoodAward *award)
{
    u8 index=(u8)(tile->food+12),value;
    if(player_active!=1 || !(tile->active&1) || tile->kind || index>=20)return 0;
    if((u16)(tile->x-camera)>=256)return 0;
    if((u8)(y-tile->y+6)>12 || (u16)(x-tile->x+6)>12)return 0;
    tile->active=4;tile->phase=4;
    value=(u8)(1+(index>>1));
    award->points=(u16)value*10;
    award->event=(u16)(0x400|(value<=8?value:value+41)|((u16)owner<<7));
    award->sound=4;
    return 1;
}
u8 arcade_food_collect(ArcadeFoodCounter *counter,u8 food,u8 y,u8 grade,
                       ArcadeSpecialSpawn *spawn,ArcadePowerup *pickup)
{
    /* E652 pointer table, eight ten-entry rows E662..E6B1. */
    static const u8 kinds[8][10]={
        {0,1,2,3,4,5,6,7,8,9},
        {2,3,4,5,5,5,6,7,8,9},
        {3,4,5,6,6,6,6,7,8,9},
        {4,5,6,7,7,7,7,7,8,9},
        {5,6,7,8,8,8,8,8,9,9},
        {6,7,8,8,8,8,8,9,9,9},
        {7,8,8,8,8,8,9,9,9,9},
        {8,8,8,8,8,9,9,9,9,9}
    };
    u8 index=(u8)(food+12),total,average;
    if(index>=20 || counter->count>=6 || counter->total<counter->count ||
       counter->total>(u16)counter->count*10)return 0;
    counter->count++;
    counter->total+=(u8)(1+(index>>1));
    if(counter->count<6)return 0;
    total=counter->total;counter->count=counter->total=0;
    /* C29C division: round the six-food average to nearest, halves upward. */
    average=(u8)((total+3)/6-1);
    if(grade>7)grade=7;
    pickup->kind=kinds[grade][average];pickup->exit_latched=0;
    spawn->active=1;spawn->phase=0;spawn->placement=y;
    return 1;
}

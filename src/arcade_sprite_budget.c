#include "arcade_sprite_budget.h"
u8 arcade_sprite_budget(const u8 active[ARCADE_SPRITE_ACTORS],u8 suppress_p2,
                         u8 admitted[9])
{
    u8 i,remaining=24;
    for(i=0;i<9;i++)admitted[i]=0;
    for(i=0;i<ARCADE_SPRITE_ACTORS;i++)
    {
        u8 cost=(i>=64 && i<67)?4:1;
        if(!active[i] || (cost==1 && (active[i]&128)) ||
           (i==8 && suppress_p2) || remaining<cost)continue;
        remaining-=cost;admitted[i>>3]|=(u8)(1u<<(i&7));
    }
    return remaining;
}

/* Order among single-cell actors changes priority, but not the remainder. */
u8 arcade_sprite_remaining(u8 singles,const u8 large[3],const u8 children[4])
{
    u8 k,remaining=singles<24?(u8)(24-singles):0;
    for(k=0;k<3;k++)if(large[k] && remaining>=4)remaining-=4;
    for(k=0;k<4;k++)if(children[k] && !(children[k]&128) && remaining)remaining--;
    return remaining;
}

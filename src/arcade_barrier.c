#include "arcade_barrier.h"
u8 arcade_barrier_player(const ArcadeWorldObjectPool *pool,ArcadePlayer *player,
                         u16 camera)
{
    u16 i,x=player->x;u8 y=player->y,crushed=0;
    if(player->active!=1)return 0;
    for(i=0;i<20;i++)
    {
        const ArcadeWorldObject *o=&pool->objects[i];
        u16 d;
        if(!(o->active&1) || o->kind!=2 || (u16)(o->x-camera)>=256)continue;
        if((u8)(y-o->y+5)>10)continue;
        d=(u16)(x-o->x+12);if(d>24)continue;
        if(d<12)player->x=(u16)(player->x-d);
        else player->x=(u16)(player->x+24-d);
        if((u16)(player->x-camera)<=6)
        {
            crushed=1;player->state=player->phase=0;
        }
    }
    return crushed;
}
u8 arcade_barrier_shots(ArcadeWorldObjectPool *pool,const u8 next_phase[20],
                       ArcadePlayerShot *shot)
{
    u16 i;u8 events=0;
    if(shot->active!=1)return 0;
    for(i=0;i<20;i++)
    {
        ArcadeWorldObject *o=&pool->objects[i];
        if(!(o->active&2) || o->kind!=2)continue;
        if((u8)(o->y-shot->y+10)>20 || (u16)(o->x-shot->x+14)>22)continue;
        o->phase=4;shot->active=2;shot->state=2;shot->phase=0;
        if(next_phase[i]==4)events++;
    }
    /* Each event queues042F with the caller's owner bit, in scan order. */
    return events;
}

u8 arcade_barrier_player_indexed(const ArcadeWorldObjectPool *pool,ArcadePlayer *player,
                         u16 camera,const u8 *slots,u8 count)
{
    u16 i,x=player->x;u8 y=player->y,crushed=0;
    if(player->active!=1)return 0;
    for(i=0;i<count;i++)
    {
        const ArcadeWorldObject *o=&pool->objects[slots[i]];
        u16 d;
        if(!(o->active&1) || o->kind!=2 || (u16)(o->x-camera)>=256)continue;
        if((u8)(y-o->y+5)>10)continue;
        d=(u16)(x-o->x+12);if(d>24)continue;
        if(d<12)player->x=(u16)(player->x-d);
        else player->x=(u16)(player->x+24-d);
        if((u16)(player->x-camera)<=6)
        {
            crushed=1;player->state=player->phase=0;
        }
    }
    return crushed;
}

u8 arcade_barrier_shots_indexed(ArcadeWorldObjectPool *pool,const u8 next_phase[20],
                       ArcadePlayerShot *shot,const u8 *slots,u8 count)
{
    u16 i;u8 events=0;
    if(shot->active!=1)return 0;
    for(i=0;i<count;i++)
    {
        ArcadeWorldObject *o=&pool->objects[slots[i]];
        if(!(o->active&2) || o->kind!=2)continue;
        if((u8)(o->y-shot->y+10)>20 || (u16)(o->x-shot->x+14)>22)continue;
        o->phase=4;shot->active=2;shot->state=2;shot->phase=0;
        if(next_phase[slots[i]]==4)events++;
    }
    /* Each event queues042F with the caller's owner bit, in scan order. */
    return events;
}

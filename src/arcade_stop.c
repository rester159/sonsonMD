#include "arcade_stop.h"
void arcade_stop_step(ArcadeStop *s,const ArcadeWorldObjectPool *pool,
                      u16 camera,u8 clock,ArcadeStopEvent *event)
{
    u16 i,edge=(u16)(camera+256);
    event->release=event->bonus=event->variant=0;
    if(!(clock&63) && !--s->timer)
    {
        event->release=1;return;
    }
    for(i=0;i<20;i++)
    {
        const ArcadeWorldObject *o=&pool->objects[i];
        if(o->active && o->kind==2 && edge>=o->x)return;
    }
    event->release=event->bonus=1;event->variant=s->grade;
    if(s->grade<6)s->grade++;
}

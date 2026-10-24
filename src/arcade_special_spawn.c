#include "arcade_special_spawn.h"
/* Original descriptor B0D1/B102..B189 and four B18A tile pairs. */
void arcade_special_spawn_step(ArcadeSpecialSpawn *s,u8 blocked,u8 ready,u8 advance,
                               ArcadeSpecialSpawnEvent *event)
{
    event->write=event->spawn=0;
    if(blocked || !s->active)return;
    if(s->phase==0)
    {
        if(!ready)return;
        s->row=s->placement>>3;s->phase++;
    }
    else if(s->phase<=4)
    {
        if(!advance)return;
        event->write=1;event->row=s->row;
        event->codes[0]=(u8)(0x19+s->phase);
        event->codes[1]=(u8)(9+s->phase);
        event->attributes[0]=event->attributes[1]=0x42;
        event->spawn=(s->phase==2);
        s->phase++;
    }
    else if(s->phase==5)s->active=0;
}

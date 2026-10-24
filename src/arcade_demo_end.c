#include "arcade_demo_end.h"
u8 arcade_demo_end_step(ArcadeDemoEnd *s,u8 clock)
{
    u8 flags=0;
    if(s->phase==3)return ARCADE_DEMO_END_DONE;
    if(s->phase==1)
    {
        s->timer=4;s->phase=2;flags=ARCADE_DEMO_END_SOUND;
    }
    if(s->phase==2 && !(clock&63) && !--s->timer)s->phase=3;
    return flags;
}

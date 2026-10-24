#include "arcade_release.h"
void arcade_release_begin(ArcadeRelease *s)
{
    /* A787 writes only the first word; phase0 initializes the trailing bytes. */
    s->active=1;s->phase=0;
}
u8 arcade_release_step(ArcadeRelease *s,u8 clock)
{
    u8 pose;
    if(!s->active)return 255;
    if(!s->phase)
    {
        s->remaining=8;s->frame=0;s->phase=1;
        return 255;
    }
    if(((u8)(clock+3)&3)!=0)return 255;
    pose=s->frame++;
    if(!--s->remaining)s->active=s->phase=0;
    return pose;
}

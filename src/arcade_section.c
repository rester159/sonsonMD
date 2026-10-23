#include "arcade_section.h"
void arcade_section_course_begin(ArcadeSection *s,u8 difficulty)
{
    s->index=s->column=s->stop=0;s->checkpoint=0;
    s->remaining=arcade_sections[0].screens-1;
    s->timer=s->next_timer;
    s->next_timer=arcade_sections[0].timer;
    s->next_stop=arcade_sections[0].stop;
    s->difficulty=difficulty;
    s->speed=(u16)(((u16)difficulty<<8)|(s->speed&255));
}
u8 arcade_section_column(ArcadeSection *s,u16 camera)
{
    const ArcadeSectionDescriptor *d;
    if(s->column&31)return 0;
    s->column=0;
    if(!--s->remaining)
    {
        if(s->difficulty<28)
        {
            ++s->difficulty;
            s->speed=(u16)(((u16)s->difficulty<<8)|(s->speed&255));
        }
        if(++s->index>=26)s->index=0;
        d=&arcade_sections[s->index];
        s->stop=s->next_stop;
        s->remaining=d->screens;
        s->next_stop=d->stop;
        s->timer=s->next_timer;
        s->next_timer=d->timer;
        s->checkpoint=camera;
    }
    return 1;
}

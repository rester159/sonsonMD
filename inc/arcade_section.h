#ifndef ARCADE_SECTION_H
#define ARCADE_SECTION_H
#include "arcade_types.h"
typedef struct {u8 screens,stop,timer;} ArcadeSectionDescriptor;
extern const ArcadeSectionDescriptor arcade_sections[26];
typedef struct {
    u16 speed,checkpoint;
    u8 index,remaining,column,difficulty,stop,next_stop,timer,next_timer;
} ArcadeSection;
/* 9A18 then8FE7 for a new course at descriptor0. Retains speed low byte and
 * copies the previous lookahead timer before replacing descriptor lookahead. */
void arcade_section_course_begin(ArcadeSection *s,u8 difficulty);
/* A730: return 1 on a screen boundary (load four world objects and refresh
 * background/collision selection), 0 for other columns. Called only when
 * the source column signal advances, not every frame. Index must be 0..25. */
u8 arcade_section_column(ArcadeSection *s,u16 camera);
#endif

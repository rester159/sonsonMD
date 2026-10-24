#include "arcade_results_decor.h"
void arcade_results_decor_step(ArcadeResultsDecor *s,u8 index)
{
    if(s->timer){--s->timer;return;}
    s->pose=s->next;
    s->timer=arcade_results_poses[index][s->pose].timer;
    s->next^=1;
}
void arcade_results_decor_begin(ArcadeResultsDecor *s,u8 index)
{
    s->next=s->timer=0;
    arcade_results_decor_step(s,index);
}

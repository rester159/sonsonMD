#include "arcade_large.h"
void arcade_large_animate(ArcadeLargeAnimation *a)
{
    const ArcadeLargeFrame *f;
    u8 k;
    if(a->timer){a->timer--;return;}
    f=&arcade_large_frames[a->cursor];
    for(k=0;k<4;k++)a->code[k]=f->code[k];
    a->timer=f->duration;a->cursor=f->next;
}
void arcade_large_damage_pose(ArcadeLargeAnimation *a,u8 hp,u8 facing,
                              u8 subtype,u8 *pending)
{
    u8 k,attr;
    if(!*pending)return;
    *pending=0;a->cursor=arcade_large_walk[(facing&2)!=0][hp];
    a->timer=0;arcade_large_animate(a);
    if(!(facing&1))return;
    attr=(u8)(((facing&2)?0xfa:0xba)+subtype);
    for(k=0;k<4;k++)a->attr[k]=attr;
}

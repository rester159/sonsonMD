#include "arcade_large.h"
static void select_pose(ArcadeLargeAnimation *a,u16 cursor,u8 attr)
{
    u8 k;
    for(k=0;k<4;k++)a->attr[k]=attr;
    a->cursor=cursor;a->timer=0;arcade_large_animate(a);
}
void arcade_large_death_step(ArcadeLargeBody *b)
{
    if(!b->active)return;
    switch(b->phase)
    {
        case 0:
            select_pose(&b->animation,arcade_large_death,0xea);
            b->hp=32;b->phase=1;break;
        case 1:
            if(--b->hp)arcade_large_animate(&b->animation);
            else
            {
                select_pose(&b->animation,arcade_large_score[b->variant][b->subtype],b->owner?0xe3:0xe4);
                b->timer=12;b->phase=3;
            }
            break;
        case 2:b->active=b->state=b->phase=0;break;
        case 3:
            if(--b->timer)arcade_large_animate(&b->animation);
            else b->phase=2;
            break;
        case 4:
            select_pose(&b->animation,arcade_large_exit,(u8)(0xfa+b->subtype));
            b->timer=16;b->phase=5;break;
        case 5:
            if(--b->timer)arcade_large_animate(&b->animation);
            else b->phase=2;
            break;
    }
}

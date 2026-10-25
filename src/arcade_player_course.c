#include "arcade_player.h"
static void begin(ArcadePlayer *p,ArcadePlayerAnimation *a,u16 camera,u8 owner)
{
    p->active=1;p->state=p->phase=0;
    p->x=(u16)(camera+32);p->y=owner?136:104;p->facing=1;
    a->owner=owner;a->cursor=a->timer=0;
    /* 9094 installs the idle script, but does not execute it. Code/attr and
     * fraction bytes retain their previous values until normal initialization. */
}
void arcade_players_course_begin(ArcadePlayer p[2],ArcadePlayerAnimation a[2],
    u16 camera,u8 two_players,const u8 out[2])
{
    /* P2 is cleared before either eligibility check; P1 out is left alone. */
    p[1].active=0;
    if(two_players && !out[1])begin(&p[1],&a[1],camera,1);
    if(!out[0])begin(&p[0],&a[0],camera,0);
}

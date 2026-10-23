#ifndef ARCADE_ORB_H
#define ARCADE_ORB_H
#include "arcade_powerup.h"
/* E1AF missed-fruit carrier, not the provisional generic ET_ORB enemy.
 * The owner retains a reference to the ordinary pickup throughout flight. */
typedef struct {
    u16 x;
    u8 subx,y,suby,active,state,phase,variant,direction,timer;
    u8 anim_timer,cursor,code,attr;
} ArcadeOrb;
void arcade_orb_step(ArcadeOrb *a,ArcadePowerup *parent,u16 camera);
#endif

#include "arcade_e_motion.h"
u8 arcade_e_turn(u8 direction,u16 x,u8 y,u16 target_x,u8 target_y)
{
    u8 increment;
    if(!direction)return 1;
    if(direction<4 || direction==16)increment=y>=target_y;
    else if(direction<8)increment=x>=target_x;
    else if(direction<12)increment=y<target_y;
    else increment=x<target_x;
    if(increment)return direction==16?1:(u8)(direction+1);
    return direction==1?16:(u8)(direction-1);
}

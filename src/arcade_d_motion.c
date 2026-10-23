#include "arcade_d_motion.h"
u8 arcade_d_aim(u16 x,u8 y,u16 target_x,u8 target_y)
{
    u8 quadrant=0,dx,dy,quotient,remainder,angle=4;
    u16 distance=(u16)(target_x-x);
    if(target_x<x){distance=(u16)(0-distance);quadrant=2;}
    dx=(u8)distance;dy=(u8)(target_y-y);
    if(target_y<y){dy=(u8)(0-dy);quadrant++;}
    /* Source unsigned divide returns quotient255/remainder numerator for0. */
    if(dx){quotient=(u8)(dy/dx);remainder=(u8)(dy%dx);}
    else {quotient=255;remainder=dy;}
    if(quotient<=5)
    {
        angle=3;
        if(quotient<2){angle=2;if((dy>>1)<remainder)angle=1;}
    }
    switch(quadrant)
    {
        case 0:return (u8)(17-angle);
        case 1:return (u8)(1+angle);
        case 2:return (u8)(9+angle);
        default:return (u8)(9-angle);
    }
}

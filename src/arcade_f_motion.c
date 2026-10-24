#include "arcade_f_motion.h"
u8 arcade_f_wave_y(u16 *phase,u8 *half,u8 origin,u8 amplitude)
{
    u16 next=(u16)(*phase+0x028d);
    u8 offset;
    if(next&0x8000){next&=0x7fff;*half^=1;}
    *phase=next;
    /* E162's unsigned16x16 multiply returns the upper16bits of
     * (amplitude<<8)*sample; the result fits in one byte. */
    offset=(u8)(((u16)amplitude*arcade_f_sine[next>>8])>>8);
    return *half?(u8)(origin-offset):(u8)(origin+offset);
}

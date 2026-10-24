#include "arcade_f_art.h"
u16 arcade_f_tile(u8 code,u8 attr)
{
    switch(((u16)(attr&31)<<9)|code|((u16)(attr&32)<<3))
    {
        case 3584:return 558;
        case 3587:return 566;
        case 3610:return 570;
        case 3611:return 574;
        case 3612:return 578;
        case 3613:return 32;
        case 1585:return 586;
        case 2097:return 590;
        case 7863:return 398;
        case 7864:return 402;
        case 7865:return 406;
    }
    return 65535;
}

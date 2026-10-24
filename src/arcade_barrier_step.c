#include "arcade_barrier_step.h"
u8 arcade_barrier_next_phase(u8 cursor)
{
    /* EB74 reads cursor+4 without following the loop marker. */
    u8 i=cursor%10;
    if(cursor>=40 || i>=8)return 255;
    return arcade_barrier_frames[cursor/10][i+1][0];
}
void arcade_barrier_step(ArcadeWorldObject *o,u8 *cursor,u16 camera,u8 advance,
                        u8 clock,ArcadeFoodDraw *draw,u8 *flash)
{
    const u8 *f;
    draw->write=0;*flash=0;
    if(!o->active || o->kind!=2)return;
    draw->row=o->y>>3;
    switch(o->phase)
    {
        case 0:
            if((u16)(camera+256)<=o->x || o->food>3)return;
            *cursor=(u8)(o->food*10+1);f=arcade_barrier_frames[o->food][0];
            o->active=f[0];o->phase=1;o->food=f[2];o->timer=f[3];
        case 1:
            if(!advance)return;
            draw->write=1;o->phase++;
            break;
        case 2:
            if(!advance)return;
            draw->write=2;o->active=o->phase=3;
            break;
        case 3:
            if((u8)(o->x-camera)<=4)o->active=0;
            draw->write=4;*flash=(u8)(0x4d-(clock&12));
            return;
        case 4:
            if((u8)(o->x-camera)<=4)o->active=0;
            if(o->timer){o->timer--;return;}
            if(*cursor>=40)return;
            if(*cursor%10==9)*cursor-=9;
            f=arcade_barrier_frames[*cursor/10][*cursor%10];(*cursor)++;
            o->active=f[0];o->phase=f[1];o->food=f[2];o->timer=f[3];
            draw->write=3;
            break;
        default:return;
    }
    if(o->food>=8 && o->food<44)draw->pattern=arcade_barrier_patterns[o->food-8];
    else draw->write=0;
}

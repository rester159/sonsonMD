#include "arcade_cell_sat.h"
u16 arcade_cell_sat(const ArcadeLargeCell *cell,u16 tile,
                    const ArcadeSpriteView *view,u16 *sat,u16 first,u16 limit)
{
    s16 x[3],y[3];u16 i,count=0,flags;
    x[0]=(s16)cell->x-view->x;y[0]=(s16)cell->y-view->y;
    /* Interior cells cannot expose either negative wrap copy. Preserve the
     * general path below for borders and displaced viewports. */
    if(x[0]>=0 && x[0]<=240 && y[0]>=0 && y[0]<=240)
    {
        if(first>limit)return 65535;
        if(x[0]>=(s16)view->width || y[0]>=(s16)view->height)return 0;
        if(first==limit)return 65535;
        flags=(u16)(0x2000|tile|((cell->attr&64)?0:0x0800)|((cell->attr&128)?0:0x1000));
        sat[first*4]=(u16)(128+y[0])&1023;
        sat[first*4+1]=(u16)(0x0500|(first+1));
        sat[first*4+2]=flags;
        sat[first*4+3]=(u16)(128+x[0])&511;
        return 1;
    }
    x[1]=x[0]-256;y[1]=y[0];
    x[2]=x[0];y[2]=y[0]-256;
    for(i=0;i<3;i++)
        if(x[i]>-16 && x[i]<(s16)view->width && y[i]>-16 && y[i]<(s16)view->height)count++;
    if(first>limit || count>limit-first)return 65535;
    flags=(u16)(0x2000|tile|((cell->attr&64)?0:0x0800)|((cell->attr&128)?0:0x1000));
    count=0;
    for(i=0;i<3;i++)
    {
        u16 slot;
        if(x[i]<=-16 || x[i]>=(s16)view->width || y[i]<=-16 || y[i]>=(s16)view->height)continue;
        slot=first+count++;
        sat[slot*4]=(u16)(128+y[i])&1023;
        sat[slot*4+1]=(u16)(0x0500|(slot+1));
        sat[slot*4+2]=flags;
        sat[slot*4+3]=(u16)(128+x[i])&511;
    }
    return count;
}

#include "arcade_large_present.h"
#ifndef SGDK_GCC
#include <string.h>
#endif
u16 arcade_large_present_mixed(const ArcadeLargeCell cells[16],u16 visible,
                         const u16 resident_children[4],
                         const ArcadeSpriteView *view,u16 *sat,u16 first,
                         u16 limit,u16 reserve_bytes,u16 reserve_ops)
{
    u16 frames[16],prepared[64*4];
    u16 i,n,total=0;u8 dynamic=0;
    if(first>limit || limit>64)return 65535;
    /* No sprite contribution needs no upload admission or cached-SAT fallback. */
    if(!visible)return 0;
    for(i=0;i<16;i++)
    {
        frames[i]=65535;
        if(!(visible&(1u<<i)))continue;
        u16 tile=(i>=12 && resident_children)?resident_children[i-12]:65535;
        if(tile!=65535 && tile>2044)return 65535;
        n=arcade_cell_sat(&cells[i],tile==65535?ARCADE_LARGE_VRAM_BASE+i*4:tile,
                          view,prepared,first+total,limit);
        if(n==65535)return 65535;
        if(n)
        {
            if(tile==65535)
            {
                frames[i]=arcade_large_art_frame(cells[i].code,cells[i].attr);
                if(frames[i]==65535)return 65535;
                dynamic=1;
            }
            total+=n;
        }
    }
    if(dynamic && !arcade_large_upload(frames,reserve_bytes,reserve_ops))return 65535;
    /* Longword copy; an indexed word loop cost ~30 cycles per SAT word. */
    memcpy(sat+first*4,prepared+first*4,(u16)(total*8));
    return total;
}

u16 arcade_large_present(const ArcadeLargeCell cells[16],u16 visible,
                         const ArcadeSpriteView *view,u16 *sat,u16 first,
                         u16 limit,u16 reserve_bytes,u16 reserve_ops)
{return arcade_large_present_mixed(cells,visible,0,view,sat,first,limit,reserve_bytes,reserve_ops);}

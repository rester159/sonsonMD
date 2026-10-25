#include <genesis.h>
#include "arcade_finish_art.h"
#include "arcade_barrier_upload.h"
#include "vram_words.h"
#if ARCADE_FINISH_TILE_BASE+8 > ARCADE_BARRIER_VRAM_BASE
#error End marker overlaps barrier art
#endif
static u8 pending,pose,column;
void arcade_finish_background_reset(void)
{
    pending=0;
    VDP_loadTileData(arcade_finish_pixels,ARCADE_FINISH_TILE_BASE,8,DMA);
}
void arcade_finish_background_write(u16 camera,u8 phase,u8 pattern)
{
    pose=pattern;pending=1;
    column=(u8)(((camera>>3)+((20-(camera>>3))&31)+phase)&63);
}
void arcade_finish_background_render(void)
{
    u16 k;
    if(!pending || !vram_words_room(4))return;
    for(k=0;k<4;k++)
        vram_words_queue(&arcade_finish_tiles[pose][k],
            VDP_getBGAAddress()+2*((7-(k&1))*64+((column+(k>>1))&63)),1,2);
    pending=0;
}

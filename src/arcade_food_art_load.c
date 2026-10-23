#include <genesis.h>
#include "arcade_food_art.h"
#include "arcade_powerup_upload.h"
#include "arcade_large_upload.h"
#include "ss_anim.h"
#if ARCADE_POWERUP_VRAM_BASE + 12 > 280 || SS_ANIM_TILE_BASE < 288
#error Food low arena overlaps sprite graphics
#endif
#if ARCADE_LARGE_VRAM_BASE + 64 > 1372
#error Food high arena overlaps large enemy graphics
#endif
/* Cold scene load, with display disabled and prior DMA flushed. */
u8 arcade_food_art_load(void)
{
    if(TILE_FONT_INDEX<1440)return 0;
    VDP_loadTileData(arcade_food_pixels,280,8,DMA);
    VDP_loadTileData(arcade_food_pixels+8*8,1372,68,DMA);
    return 1;
}

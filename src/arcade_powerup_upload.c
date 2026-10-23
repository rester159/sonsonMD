#include <genesis.h>
#include "arcade_powerup_upload.h"
#include "arcade_bonus_art.h"
#include "ss_anim.h"
#if ARCADE_POWERUP_VRAM_BASE < ARCADE_BONUS_TILE_BASE + ARCADE_BONUS_TILE_COUNT
#error Special pickup arena overlaps bonus art
#endif
#if ARCADE_POWERUP_VRAM_BASE + 12 > SS_ANIM_TILE_BASE
#error Special pickup arena overlaps animation art
#endif
static u16 resident[3];
void arcade_powerup_upload_reset(void)
{
    u16 i;
    for(i=0;i<3;i++)resident[i]=65535;
}
u8 arcade_powerup_upload(const u16 cells[3],u16 reserve_bytes,u16 reserve_ops)
{
    u16 i,mask=0,words=0,ops=0;
    for(i=0;i<3;i++)
    {
        u16 cell=cells[i];
        if(cell==65535)continue;
        if(cell>=ARCADE_POWERUP_ART_CELLS)return 0;
        if(cell!=resident[i])
        {
            const u32 *source=arcade_powerup_pixels+(u32)cell*32;
            mask|=(u16)(1u<<i);words+=64;ops++;
            if(((u32)source&0x1ffff)>0x1ff80)ops++;
        }
    }
    if((u32)DMA_getQueueSize()+ops+reserve_ops>DMA_getMaxQueueSize())return 0;
    if((u32)words+((u32)reserve_bytes+1)/2>32767)return 0;
    if((u32)DMA_getQueueTransferSize()+(u32)words*2+reserve_bytes>65534)return 0;
    if(!DMA_canQueue(DMA_VRAM,words+(reserve_bytes+1)/2))return 0;
    for(i=0;i<3;i++)if(mask&(1u<<i))
    {
        u16 cell=cells[i];
        if(!DMA_queueDma(DMA_VRAM,(void*)(arcade_powerup_pixels+(u32)cell*32),
                        (ARCADE_POWERUP_VRAM_BASE+i*4)*32,64,2))return 0;
        resident[i]=cell;
    }
    return 1;
}

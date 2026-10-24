#include <genesis.h>
#include "arcade_large_upload.h"
#include "ss_level.h"
#if SS_LEVEL_TILE_TOP > ARCADE_LARGE_VRAM_BASE
#error Large enemy upload arena overlaps the level dictionary
#endif
static u16 resident[16];
void arcade_large_upload_reset(void)
{
    u16 k;
    for(k=0;k<16;k++)resident[k]=65535;
}
u8 arcade_large_upload(const u16 frames[16],u16 reserve_bytes,u16 reserve_ops)
{
    u16 k,words=0,ops=0,mask=0;
    /* SGDK computes the font address from the current VDP table layout. */
    if(ARCADE_LARGE_VRAM_BASE+64>TILE_FONT_INDEX)return 0;
    for(k=0;k<16;k++)
    {
        u16 f=frames[k];
        if(f==65535)continue;
        if(f>=ARCADE_LARGE_ART_FRAMES)return 0;
        if(f!=resident[k])
        {
            const u32 *source=arcade_large_art_pixels+(u32)f*32;
            mask|=(u16)(1u<<k);words+=64;ops++;
            /* Ordinary SDK queue helper splits at a 128 KiB source boundary. */
            if(((u32)source&0x1ffff)>0x1ff80)ops++;
        }
    }
    if((u32)DMA_getQueueSize()+ops+reserve_ops>DMA_getMaxQueueSize())return 0;
    if((u32)words+((u32)reserve_bytes+1)/2>32767)return 0;
    if((u32)DMA_getQueueTransferSize()+(u32)words*2+reserve_bytes>65534)return 0;
    if(!DMA_canQueue(DMA_VRAM,words+(reserve_bytes+1)/2))return 0;
    /* Main-thread-only queue producer; preflight covers all possible splits.
     * No custom DMA/Z80 ownership, copy staging or early flush. */
    for(k=0;k<16;k++)if(mask&(1u<<k))
    {
        u16 f=frames[k];
        if(!DMA_queueDma(DMA_VRAM,(void*)(arcade_large_art_pixels+(u32)f*32),
                         (ARCADE_LARGE_VRAM_BASE+k*4)*32,64,2))return 0;
        resident[k]=f;
    }
    return 1;
}

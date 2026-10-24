#include <genesis.h>
#include "arcade_e_upload.h"
#include "arcade_bonus_art.h"
#include "ss_anim.h"
#include "arcade_d_art.h"
#include "arcade_finish_art.h"
#if ARCADE_E_SLOT0 < ARCADE_D_TILE_BASE + ARCADE_D_TILE_COUNT || ARCADE_E_SLOT0 + 4 > 1952
#error E slot0 overlaps D art or SAT
#endif
#if ARCADE_E_SLOT1 < 1972 || ARCADE_E_SLOT1 + 4 > ARCADE_FINISH_TILE_BASE
#error E slot1 overlaps the 80-entry SAT or finish art
#endif
const u16 arcade_e_upload_tiles[2]={ARCADE_E_SLOT0,ARCADE_E_SLOT1};
static u16 resident[2];
void arcade_e_upload_reset(void)
{
    u16 i;
    for(i=0;i<2;i++)resident[i]=65535;
}
u8 arcade_e_upload(const u16 cells[2],u16 reserve_bytes,u16 reserve_ops)
{
    u16 i,mask=0,words=0,ops=0;
    for(i=0;i<2;i++)
    {
        u16 cell=cells[i];
        if(cell==65535)continue;
        if(cell>=ARCADE_E_ART_CELLS)return 0;
        if(cell!=resident[i])
        {
            const u32 *source=arcade_e_pixels[cell];
            mask|=(u16)(1u<<i);words+=64;ops++;
            if(((u32)source&0x1ffff)>0x1ff80)ops++;
        }
    }
    if((u32)DMA_getQueueSize()+ops+reserve_ops>DMA_getMaxQueueSize())return 0;
    if((u32)words+((u32)reserve_bytes+1)/2>32767)return 0;
    if((u32)DMA_getQueueTransferSize()+(u32)words*2+reserve_bytes>65534)return 0;
    if(!DMA_canQueue(DMA_VRAM,words+(reserve_bytes+1)/2))return 0;
    for(i=0;i<2;i++)if(mask&(1u<<i))
    {
        u16 cell=cells[i];
        if(!DMA_queueDma(DMA_VRAM,(void*)(arcade_e_pixels[cell]),
                        arcade_e_upload_tiles[i]*32,64,2))return 0;
        resident[i]=cell;
    }
    return 1;
}

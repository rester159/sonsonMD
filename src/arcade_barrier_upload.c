#include <genesis.h>
#include "arcade_barrier_upload.h"
static u8 resident[ARCADE_BARRIER_UPLOAD_SLOTS];
void arcade_barrier_upload_reset(void)
{
    u16 i;for(i=0;i<ARCADE_BARRIER_UPLOAD_SLOTS;i++)resident[i]=255;
}
u8 arcade_barrier_upload(u8 slot,u8 pose,u16 reserve_bytes,u16 reserve_ops)
{
    const u32 *source;u16 ops=1;
    if(slot>=ARCADE_BARRIER_UPLOAD_SLOTS || pose>=180)return 0;
    /* Never publish into an incompatible menu/large-plane VRAM layout. */
    if(VDP_getBGAAddress()!=0xe000 || VDP_getBGBAddress()!=0xc000 ||
       VDP_getWindowAddress()!=0xd000 || VDP_getHScrollTableAddress()!=0xf000 ||
       VDP_getSpriteListAddress()!=0xf400 || VDP_getPlaneWidth()!=64 ||
       VDP_getPlaneHeight()!=32)return 0;
    if(resident[slot]==pose)return 1;
    source=arcade_barrier_pose_pixels[pose];
    if(((u32)source&0x1ffff)>0x1ff80)ops++;
    if((u32)DMA_getQueueSize()+ops+reserve_ops>DMA_getMaxQueueSize())return 0;
    if((u32)reserve_bytes+128>65534 ||
       (u32)DMA_getQueueTransferSize()+128+reserve_bytes>65534)return 0;
    if(!DMA_canQueue(DMA_VRAM,64+(reserve_bytes+1)/2))return 0;
    if(!DMA_queueDma(DMA_VRAM,(void*)source,
        (ARCADE_BARRIER_VRAM_BASE+(u16)slot*4)*32,64,2))return 0;
    resident[slot]=pose;return 1;
}

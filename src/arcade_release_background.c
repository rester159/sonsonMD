#include <genesis.h>
#include "arcade_release.h"
#include "arcade_release_art.h"
#include "arcade_barrier_upload.h"
#include "vram_words.h"
#if ARCADE_RELEASE_TILE_BASE < ARCADE_BARRIER_VRAM_BASE + ARCADE_BARRIER_UPLOAD_SLOTS*4
#error Release art overlaps barrier art
#endif
#if ARCADE_RELEASE_TILE_BASE + ARCADE_RELEASE_TILE_COUNT > 2048
#error Release art exceeds VRAM
#endif
static ArcadeRelease release;
static u8 pending,pose,column;
void arcade_release_background_reset(void)
{
    memset(&release,0,sizeof(release));pending=0;
    /* Cold scene initialization, after plane/SAT layout and world art load. */
    VDP_loadTileData(arcade_release_pixels,ARCADE_RELEASE_TILE_BASE,
                     ARCADE_RELEASE_TILE_COUNT,DMA);
}
void arcade_release_background_begin(void){arcade_release_begin(&release);}
void arcade_release_background_step(u16 camera,u8 clock,u8 plane_phase)
{
    u8 frame=arcade_release_step(&release,clock);
    if(frame==255)return;
    pose=frame;pending=1;
    /* Source ring columns30/31. Choose their visible world instance in the
     * 32-column arcade ring, then retain its physical64-column destination.
     * Storing the physical slot also survives a native ribbon rebase. */
    column=(u8)(((camera>>3)+((30-(camera>>3))&31)+plane_phase)&63);
}
void arcade_release_background_render(void)
{
    u16 i,k;
    if(!pending)return;
    /* Admit the entire pose before publication. Immutable ROM map sources
     * remain valid until VBlank, even if simulation advances another pose. */
    if(!vram_words_room(24))return;
    for(i=0;i<6;i++)for(k=0;k<4;k++)
    {
        u16 row=(arcade_release_destinations[i]-0x1000)/32-(k&1);
        u16 col=(column+(k>>1))&63;
        vram_words_queue(&arcade_release_tiles[pose][k],
                         VDP_getBGAAddress()+2*(row*64+col),1,2);
    }
    pending=0;
}

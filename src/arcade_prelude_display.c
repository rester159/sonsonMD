#include <genesis.h>
#include "arcade_prelude_display.h"
#include "ss_title.h"
#include "ss_anim.h"
#include "arcade_prelude_hud.h"
#include "arcade_prelude_hud_art.h"
#define TITLE_BASE 768
#if SS_ANIM_TILE_BASE + SS_ANIM_TILE_COUNT > TITLE_BASE
#error Prelude animation/title VRAM overlap
#endif
#if TITLE_BASE + SS_TITLE_TILE_COUNT > ARCADE_PRELUDE_HUD_BASE
#error Prelude title/HUD VRAM overlap
#endif
#if ARCADE_PRELUDE_HUD_BASE + ARCADE_PRELUDE_HUD_TILES > 1376
#error Prelude HUD/font VRAM overlap
#endif
#if TITLE_BASE + SS_TITLE_TILE_COUNT > 1376
#error Prelude title/font VRAM overlap
#endif
#if ARCADE_BONUS_TILE_BASE + ARCADE_BONUS_TILE_COUNT > SS_ANIM_TILE_BASE
#error Prelude bonus/animation VRAM overlap
#endif
void arcade_prelude_display_begin(void)
{
    u16 row,col,words[32],empty[4]={0};
    VDP_setEnable(FALSE);DMA_flushQueue();DMA_waitCompletion();
    VDP_setScreenWidth256();VDP_setPlaneSize(64,32,TRUE);
    VDP_clearPlane(BG_A,TRUE);VDP_clearPlane(BG_B,TRUE);
    VDP_setHorizontalScroll(BG_A,0);VDP_setVerticalScroll(BG_A,0);
    VDP_setHorizontalScroll(BG_B,0);VDP_setVerticalScroll(BG_B,0);
    PAL_setColors(PAL0*16,ss_pal_char,16,CPU);
    PAL_setColors(PAL1*16,ss_pal_sprite,16,CPU);VDP_setBackgroundColor(0);
    VDP_loadTileData(ss_title_tiles,TITLE_BASE,SS_TITLE_TILE_COUNT,DMA);
    VDP_loadTileData(arcade_prelude_hud_pixels,ARCADE_PRELUDE_HUD_BASE,ARCADE_PRELUDE_HUD_TILES,DMA);
    VDP_loadTileData(ss_anim_tiles,SS_ANIM_TILE_BASE,SS_ANIM_TILE_COUNT,DMA);
    VDP_loadTileData(arcade_bonus_pixels,ARCADE_BONUS_TILE_BASE,ARCADE_BONUS_TILE_COUNT,DMA);
    for(row=0;row<32;row++)
    {
        for(col=0;col<32;col++)
        {
            /* Arcade visible area is240lines; NTSC Genesis is224. Keep the
             * source top at row1, with copyright/credits in the final2rows. */
            u16 source_row=row==26?28:row==27?30:(row+1)&31;
            u16 cell=ss_title_nametable[source_row*32+col];
            words[col]=(cell&0xf800)|((cell&0x7ff)+TITLE_BASE);
        }
        DMA_doDma(DMA_VRAM,words,VDP_getBGAAddress()+row*128,32,2);
    }
    DMA_doDma(DMA_VRAM,empty,VDP_getSpriteListAddress(),4,2);
    VDP_setEnable(TRUE);
}
u8 arcade_prelude_display_publish(const ArcadePrelude *s)
{
    static const ArcadeSpriteView view={0,8,256,224};
    u16 sat[48],count=arcade_prelude_sat(s,&view,sat,0,12);
    if(count==65535)return 0;
    if(count)sat[(count-1)*4+1]&=0xff00;
    else{sat[0]=sat[1]=sat[2]=sat[3]=0;count=1;}
    DMA_doDma(DMA_VRAM,sat,VDP_getSpriteListAddress(),count*4,2);
    return 1;
}

void arcade_prelude_display_hud(u32 high,u16 credits,u8 free_play,u8 press_start)
{
    u16 score[8],credit[32],prompt[32];
    arcade_prelude_hud(high,credits,free_play,press_start,score,credit,prompt);
    DMA_doDma(DMA_VRAM,score,VDP_getBGAAddress()+128+13*2,8,2);
    DMA_doDma(DMA_VRAM,credit,VDP_getBGAAddress()+27*128,32,2);
    DMA_doDma(DMA_VRAM,prompt,VDP_getBGAAddress()+12*128,32,2);
}

#include <genesis.h>
#include "arcade_results_display.h"
#include "arcade_results_map.h"
#include "arcade_results_art.h"
#include "arcade_results_decor.h"
#include "arcade_bonus_art.h"
#include "arcade_cell_sat.h"
#define RESULT_BASE ARCADE_BONUS_TILE_BASE
#define DECOR_BASE (RESULT_BASE+ARCADE_RESULTS_ART_TILES)
#if ARCADE_RESULTS_ART_TILES + 24 > ARCADE_BONUS_TILE_COUNT
#error Results scene exceeds shared bonus art arena
#endif
static u16 map[32*32];
static u32 dirty;
static u8 number,decor_active;
static ArcadeResultsDecor decor[3];
void arcade_results_display_begin(void)
{
    number=decor_active=0;
    arcade_results_map_clear(map);dirty=0x7fffffe0UL;
    VDP_loadTileData(arcade_results_pixels,RESULT_BASE,ARCADE_RESULTS_ART_TILES,DMA);
    VDP_loadTileData(arcade_results_decor_pixels,DECOR_BASE,24,DMA);
}
void arcade_results_display_restore(void)
{
    dirty=0;decor_active=0;
    VDP_loadTileData(arcade_bonus_pixels,ARCADE_BONUS_TILE_BASE,ARCADE_BONUS_TILE_COUNT,DMA);
}
static void dirty_group(u8 group)
{
    const ArcadeResultsGroup *g=&arcade_results_groups[group];u16 i;
    for(i=g->first;i<g->first+g->count;i++)dirty|=1UL<<(arcade_results_cells[i].cell>>5);
}
void arcade_results_display_event(const ArcadeResultsEvent *e)
{
    u16 i;
    if(e->flags&ARCADE_RESULTS_BOARD){number=e->number;dirty_group(0);}
    if(e->flags&ARCADE_RESULTS_CLEAR)dirty|=0x7fffffe0UL;
    if(e->flags&ARCADE_RESULTS_PLAYER)
    {
        if(e->player_mask&1){dirty_group(1);dirty|=1UL<<11;}
        if(e->player_mask&2){dirty_group(3);dirty|=1UL<<19;}
        if(e->marker==1)dirty_group(2);
        if(e->marker==2)dirty_group(4);
    }
    arcade_results_map_event(map,number,e);
    if(e->flags&ARCADE_RESULTS_DECOR)
    {
        for(i=0;i<3;i++)arcade_results_decor_begin(&decor[i],i);
        decor_active=1;
    }
    if((e->flags&ARCADE_RESULTS_ANIMATE) && decor_active)
        for(i=0;i<3;i++)arcade_results_decor_step(&decor[i],i);
}
u8 arcade_results_display_pending(void){return dirty!=0;}
void arcade_results_display_background(void)
{
    u16 row,column,words[32];
    for(row=5;row<31;row++)if(dirty&(1UL<<row))
    {
        if(DMA_getQueueSize()>=DMA_getMaxQueueSize() || !DMA_canQueue(DMA_VRAM,32))return;
        for(column=0;column<32;column++)words[column]=RESULT_BASE+map[row*32+column];
        /* Copy queue owns the words after return, so later map events and
         * reuse of this stack row cannot corrupt deferred transfers. */
        if(!DMA_copyAndQueueDma(DMA_VRAM,words,VDP_getBGAAddress()+row*128,32,2))return;
        dirty&=~(1UL<<row);
    }
}
u16 arcade_results_display_sat(u16 *sat,u16 first,u16 limit)
{
    static const ArcadeSpriteView view={0,1,256,224};u16 i;
    if(!decor_active)return 0;
    if(first>limit || limit-first<3)return 65535;
    for(i=0;i<3;i++)
    {
        const ArcadeResultsPose *p=&arcade_results_poses[i][decor[i].pose];
        ArcadeLargeCell cell;
        cell.x=(u8)arcade_results_positions[i].x;
        cell.y=(u8)(-arcade_results_positions[i].y-16);
        cell.code=p->code;cell.attr=p->attr;
        arcade_cell_sat(&cell,DECOR_BASE+p->tile,&view,sat,first+i,limit);
    }
    return 3;
}

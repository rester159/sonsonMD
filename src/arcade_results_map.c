#include "arcade_results_map.h"
#include "arcade_results_art.h"
void arcade_results_map_clear(u16 *map)
{
    u16 i;
    /* Source011A clears26 rows beginning10A0, leaving HUD and row31. */
    for(i=5*32;i<31*32;i++)map[i]=arcade_results_digits[11];
}
static void group(u16 *map,u8 index)
{
    const ArcadeResultsGroup *g=&arcade_results_groups[index];u16 i;
    for(i=g->first;i<g->first+g->count;i++)
        map[arcade_results_cells[i].cell]=arcade_results_cells[i].tile;
}
static void number_write(u16 *map,u16 cell,u8 number)
{
    /* 9877 stores packed number intoDC/DD and clearsDE/DF. 4EBE writes
     * eight nibbles, then blanks leading zeroes except the final digit. */
    u8 i,started=0;
    for(i=0;i<8;i++)
    {
        u8 digit=i==2?(number>>4):i==3?(number&15):0;
        if(digit)started=1;
        map[cell+i]=arcade_results_digits[(!started && i<7)?10:digit];
    }
}
void arcade_results_map_event(u16 *map,u8 number,const ArcadeResultsEvent *e)
{
    if(e->flags&ARCADE_RESULTS_CLEAR)arcade_results_map_clear(map);
    if(e->flags&ARCADE_RESULTS_BOARD)group(map,0);
    if(e->flags&ARCADE_RESULTS_PLAYER)
    {
        if(e->player_mask&1){group(map,1);number_write(map,0x16d,number);}
        if(e->player_mask&2){group(map,3);number_write(map,0x26d,number);}
        if(e->marker==1)group(map,2);
        if(e->marker==2)group(map,4);
    }
}

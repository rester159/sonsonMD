#include "arcade_name_map.h"
#include "arcade_name_art.h"
void arcade_name_map_begin(u16 *map,u8 players)
{
    u16 i;u8 group;
    /* Source011A preserves HUD rows0..4 and bottom row31. */
    for(i=160;i<992;i++)map[i]=ARCADE_NAME_BLANK;
    for(group=0;group<4;group++)
    {
        const ArcadeNameGroup *g=&arcade_name_groups[group];
        if(group>=2 && !(players&(1<<(group-2))))continue;
        for(i=g->first;i<g->first+g->count;i++)
            map[arcade_name_cells[i].cell]=arcade_name_cells[i].tile;
    }
}
void arcade_name_map_update(u16 *map,u8 player,const ArcadeNameEntry *s,u8 paint)
{
    u8 i;u16 start=player?0x2f2:0x2e7;
    for(i=0;i<4;i++)if(paint&(1<<i))map[start+i*2]=arcade_name_glyphs[s->name[i]];
}
u16 arcade_name_cursor_sat(const ArcadeNameEntry *s,u8 player,u16 base,
                           const ArcadeSpriteView *view,u16 *sat,u16 first,u16 limit)
{
    ArcadeLargeCell cell;u8 row,column,y;
    if(s->raw[2])return 0;
    /* Selector has already received9FF9's hole correction in name_step. */
    row=s->raw[6]/17;column=s->raw[6]%17;
    cell.x=(u8)(0x3c+column*8);if(cell.x>0xbc)cell.x=0x3c;
    y=(u8)(0xc4-row*16);if(y<0x7c)y=0xc4;
    cell.y=(u8)(-y-16);cell.code=player?0x57:0x56;cell.attr=player?0xc0:0xc3;
    return arcade_cell_sat(&cell,base+ARCADE_NAME_CURSOR+player*4,view,sat,first,limit);
}

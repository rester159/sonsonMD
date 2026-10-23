#include "arcade_title_map.h"
#include "arcade_title_art.h"
void arcade_title_map_label(u16 *map,u8 command)
{
    u16 i,j;
    for(i=0;i<ARCADE_TITLE_ART_GROUPS;i++)
    {
        const ArcadeTitleGroup *g=&arcade_title_groups[i];
        if(g->command!=command)continue;
        for(j=g->first;j<g->first+g->count;j++)
            map[arcade_title_cells[j].cell]=arcade_title_cells[j].tile;
        return;
    }
}
void arcade_title_map_ranking(u16 *map,u8 total,const ArcadeTitleRankEntry entries[5])
{
    u8 row;
    total=total!=0;
    arcade_title_map_label(map,total?13:8);
    for(row=0;row<5;row++)
    {
        const ArcadeTitleRankRow *r=&arcade_title_rank_rows[total][row];
        const ArcadeTitleRankEntry *entry=&entries[row];
        const u16 *glyphs=arcade_title_rank_glyphs[row];
        u8 i,started=0;
        /* 4EBE/4EEA expands all eight nibbles, blanking leading zeroes
         * except the last. Preserve even non-decimal nibble values. */
        for(i=0;i<8;i++)
        {
            u8 digit=(entry->score[i>>1]>>((i&1)?0:4))&15;
            if(digit)started=1;
            map[r->score+i]=glyphs[(!started && i<7)?16:digit];
        }
        for(i=0;i<(total?8:4);i++)
            map[r->name+i+(i>=4?1:0)]=glyphs[entry->name[i]];
    }
}

#include "arcade_ranking.h"
void arcade_ranking_init(ArcadeRanking *s)
{
    /* 40C4/40E8 repeat the ROM records41E6 and41EE. */
    static const u8 individual[8]={0,1,0,0,0x18,0x15,0x22,0x1f};
    static const u8 total[12]={0,2,0,0,0x0a,0x10,0x13,0x11,0x20,0x13,0x1f,0x1d};
    u8 row,i;
    for(row=0;row<5;row++)
    {
        for(i=0;i<8;i++)s->individual[row][i]=individual[i];
        for(i=0;i<12;i++)s->total[row][i]=total[i];
    }
}
static u8 qualifies(const u8 *score,const u8 *entry)
{
    u8 i;
    /* 9CBF compares two unsigned words, accepting equality. Bytewise
     * lexical comparison is equivalent and avoids unaligned word loads. */
    for(i=0;i<4;i++)
        if(score[i]!=entry[i])return score[i]>entry[i];
    return 1;
}
static u8 insert(u8 *table,u8 stride,const u8 *score)
{
    u8 row,i;
    u16 pos;
    for(row=0;row<5;row++)if(qualifies(score,table+row*stride))break;
    if(row==5)return 5;
    for(pos=(u16)5*stride;pos>(u16)(row+1)*stride;)
    {
        --pos;table[pos]=table[pos-stride];
    }
    table+=row*stride;
    for(i=0;i<4;i++)table[i]=score[i];
    for(;i<stride;i++)table[i]=16;
    return row;
}
void arcade_ranking_submit(ArcadeRanking *s,const u8 scores[3][4],u8 two,
                           ArcadeRankingResult *result)
{
    u8 i,row;
    for(i=0;i<3;i++){result->qualified[i]=0;result->row[i]=0;}
    row=insert(&s->individual[0][0],8,scores[0]);
    if(row<5){result->qualified[0]=1;result->row[0]=row;}
    if(!two)return; /* 9C63 skips total qualification as well as P2. */
    row=insert(&s->individual[0][0],8,scores[1]);
    if(row<5)
    {
        result->qualified[1]=1;result->row[1]=row;
        /* Source9C6D checks row equality even if P1 did not qualify. */
        if(result->row[0]==row)
        {
            if(row==4)result->qualified[0]=0;
            else ++result->row[0];
        }
    }
    row=insert(&s->total[0][0],12,scores[2]);
    if(row<5){result->qualified[2]=1;result->row[2]=row;}
}
void arcade_ranking_names(ArcadeRanking *s,const ArcadeRankingResult *r,const u8 names[2][4])
{
    u8 player,i;
    for(player=0;player<2;player++)
        if(r->qualified[player])
            for(i=0;i<4;i++)s->individual[r->row[player]][4+i]=names[player][i];
    if(r->qualified[2])
        for(player=0;player<2;player++)
            for(i=0;i<4;i++)s->total[r->row[2]][4+player*4+i]=names[player][i];
}

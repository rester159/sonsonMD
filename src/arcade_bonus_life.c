#include "arcade_bonus_life.h"
/* 4206/4213/4220/422D selected by raw DSW2 bits 3..4. */
static const u32 first[4]={0x000200,0x000300,0x000200,0x000300};
static const u32 increments[4][2]={
    {0,0},{0,0},{0x000600,0x001000},{0x000600,0x001200}
};
void arcade_bonus_life_begin(ArcadeBonusLife *s,u8 dip)
{s->threshold=first[dip&3];s->phase=0;}
static u32 bcd_add24(u32 a,u32 b)
{
    u32 result=0;u8 shift,carry=0;
    for(shift=0;shift<24;shift+=4)
    {
        u8 digit=(u8)(((a>>shift)&15)+((b>>shift)&15)+carry);
        carry=digit>=10;if(carry)digit-=10;
        result|=(u32)digit<<shift;
    }
    return result;
}
u8 arcade_bonus_life_step(ArcadeBonusLife *s,u8 dip,u8 clock,u8 owner,
                         u8 live,u8 out,u32 score)
{
    u32 packed=0,value=score;u8 shift,phase=s->phase;
    dip&=3;
    if(!live || out || (clock&7) || ((clock>>3)&1)!=owner)return 0;
    /* FC63 rejects scores with a nonzero leading packed-BCD byte. */
    if(score>=1000000UL || phase>1 || (phase && dip<2))return 0;
    for(shift=0;shift<24;shift+=4)
    {packed|=(value%10)<<shift;value/=10;}
    /* Preserve both independent CMPD/BHI tests, including their source quirk:
     * the lower word is tested even when the score's upper word is greater. */
    if((s->threshold>>8)>(packed>>16) ||
       ((s->threshold&255)<<8)>(packed&65535))return 0;
    s->threshold=bcd_add24(s->threshold,increments[dip][phase]);
    if(!phase)s->phase=1;
    return 1;
}

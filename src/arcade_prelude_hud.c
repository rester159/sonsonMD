#include "arcade_prelude_hud.h"
#include "arcade_prelude_hud_art.h"
static u16 glyph(u8 bank,u8 code){return ARCADE_PRELUDE_HUD_BASE+arcade_prelude_hud_glyphs[bank][code];}
static void label(u16 *row,u8 col,u8 bank,const char *s)
{
    while(*s){u8 code=*s==' '?16:(u8)(*s-'A'+17);row[col++]=glyph(bank,code);++s;}
}
void arcade_prelude_hud(u32 high,u16 credits,u8 free_play,u8 press_start,
                       u16 score[8],u16 credit[32],u16 prompt[32])
{
    u8 digits[8],i,leading=1;u16 divisor=10000;
    for(i=8;i;i--){digits[i-1]=high%10;high/=10;}
    for(i=0;i<8;i++)
    {
        u8 code=digits[i];if(code || i==7)leading=0;
        score[i]=glyph(0,leading?16:code);
    }
    for(i=0;i<32;i++){credit[i]=glyph(free_play?2:1,16);prompt[i]=glyph(3,16);}
    if(free_play)label(credit,2,2,"FREE PLAY");
    else
    {
        u8 col=9;leading=1;label(credit,2,1,"CREDIT");
        while(divisor)
        {
            u8 d=credits/divisor;credits%=divisor;
            if(d || divisor==1)leading=0;
            if(!leading)credit[col++]=glyph(1,d);
            divisor/=10;
        }
    }
    label(prompt,7,press_start?3:4,press_start?"PRESS START BUTTON":"PLEASE DEPOSIT COIN");
}

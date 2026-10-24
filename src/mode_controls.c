/* Owner-requested console mappings, with the arcade coinage DIP ratios. */
#include "mode_controls.h"
#include "game.h"
#include "frontend.h"
volatile u16 g_arcadeCredits;
volatile u8 g_homeCredits, g_paused;
volatile u8 g_startPlayers = 1;
static u16 previous[2];
static u8 coin_fraction[2], coin_edges;
u8 mode_input_coins(void){return coin_edges;}
/* Read the same pad sample as the coin/edge poll, without polling hardware twice. */
u8 mode_input_starts_held(void)
{
    return (u8)(((previous[0]&BUTTON_START)?1:0) |
                ((previous[1]&BUTTON_START)?2:0));
}
void mode_input_rearm(void)
{
    coin_edges = 0;
    previous[0] = JOY_readJoypad(JOY_1);
    previous[1] = JOY_readJoypad(JOY_2);
}
u8 mode_input_poll(void)
{
    static const u8 coins[16] = {255,4,4,3,3,3,2,2,2,1,1,1,1,1,1,1};
    static const u8 credits[16] = {0,3,1,4,2,1,5,3,1,7,6,5,4,3,2,1};
    u8 i, starts = 0;
    coin_edges = 0;
    for (i = 0; i < 2; i++)
    {
        u16 pad = JOY_readJoypad(i ? JOY_2 : JOY_1);
        u16 edge = (u16)(pad & ~previous[i]);
        previous[i] = pad;
        if (edge & BUTTON_START) starts |= (u8)(1 << i);
        if (g_mode == MODE_ARCADE && (edge & BUTTON_C))
        {
            coin_edges |= (u8)(1u << i);
            /* GCC 16 can widen a masked nonvolatile byte load into an odd
             * long access before g_config. Require the real byte access. */
            u8 setting = *(volatile u8 *)&g_config.dsw1;
            /* 4134 initializes both slots to 1C/1C, then applies the DIP
             * ratio to A (raw bit4 set) or B (clear). 9242 owns a separate
             * fractional count at31/36. Controller C maps to its own slot. */
            setting = (((setting & 16) ? 0 : 1) == i) ? (setting & 15) : 15;
            if (++coin_fraction[i] >= coins[setting])
            {
                u16 before = g_arcadeCredits;
                u16 total = (u16)(before + credits[setting]);
                coin_fraction[i] = 0;
                /* 924F..9255 discards a word-overflow award. */
                if (total >= before) g_arcadeCredits = total;
            }
        }
    }
    return starts;
}
bool arcade_take_credit(void)
{
    if (!(g_config.dsw1 & 15)) return TRUE;
    if (!g_arcadeCredits) return FALSE;
    g_arcadeCredits--;
    return TRUE;
}

/* 501F: decimal digits, no leading zeroes, remainder of six cells blank. */
void arcade_credit_text(u16 credits, char text[7])
{
    static const u16 divisors[5]={10000,1000,100,10,1};
    u8 i,n=0;
    for(i=0;i<5;i++)
    {
        u8 digit=0;
        while(credits>=divisors[i]){credits-=divisors[i];digit++;}
        if(digit || n || i==4)text[n++]=(char)('0'+digit);
    }
    while(n<6)text[n++]=' ';
    text[6]=0;
}

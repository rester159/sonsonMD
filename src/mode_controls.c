/* Owner-requested console mappings, with the arcade coinage DIP ratios. */
#include "mode_controls.h"
#include "game.h"
#include "frontend.h"
volatile u8 g_arcadeCredits, g_homeCredits, g_paused;
volatile u8 g_startPlayers = 1;
static u16 previous[2];
static u8 coin_fraction;
void mode_input_rearm(void)
{
    previous[0] = JOY_readJoypad(JOY_1);
    previous[1] = JOY_readJoypad(JOY_2);
}
u8 mode_input_poll(void)
{
    static const u8 coins[16] = {1,4,4,3,3,3,2,2,2,1,1,1,1,1,1,1};
    static const u8 credits[16] = {0,3,1,4,2,1,5,3,1,7,6,5,4,3,2,1};
    u8 i, starts = 0;
    for (i = 0; i < 2; i++)
    {
        u16 pad = JOY_readJoypad(i ? JOY_2 : JOY_1);
        u16 edge = (u16)(pad & ~previous[i]);
        previous[i] = pad;
        if (edge & BUTTON_START) starts |= (u8)(1 << i);
        if (g_mode == MODE_ARCADE && (edge & BUTTON_C))
        {
            /* GCC 16 can widen a masked nonvolatile byte load into an odd
             * long access before g_config. Require the real byte access. */
            u8 setting = *(volatile u8 *)&g_config.dsw1;
            setting &= 15;
            if (++coin_fraction >= coins[setting])
            {
                u16 total = (u16)g_arcadeCredits + credits[setting];
                coin_fraction = 0;
                g_arcadeCredits = (u8)(total > 99 ? 99 : total);
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

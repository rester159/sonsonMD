#ifndef SONSON_MODE_CONTROLS_H
#define SONSON_MODE_CONTROLS_H
#include <genesis.h>
extern volatile u16 g_arcadeCredits;
extern volatile u8 g_homeCredits, g_startPlayers, g_paused;
void arcade_credit_text(u16 credits, char text[7]);
void mode_input_rearm(void);
u8 mode_input_poll(void);
u8 mode_input_coins(void);
u8 mode_input_starts_held(void);
bool arcade_take_credit(void);
#endif

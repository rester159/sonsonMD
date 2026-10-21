#ifndef SONSON_MODE_CONTROLS_H
#define SONSON_MODE_CONTROLS_H
#include <genesis.h>
extern volatile u8 g_arcadeCredits, g_homeCredits, g_startPlayers, g_paused;
void mode_input_rearm(void);
u8 mode_input_poll(void);
bool arcade_take_credit(void);
#endif

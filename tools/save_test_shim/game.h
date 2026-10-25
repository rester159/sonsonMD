#ifndef SAVE_TEST_GAME_H
#define SAVE_TEST_GAME_H
#include "genesis.h"
#define DSW1_DEFAULT 0xDF
#define DSW2_DEFAULT 0xCB
typedef struct {u8 dsw1,dsw2,lives,difficulty,bonusLife,demoSound,flipScreen,homeCredits;} GameConfig;
extern GameConfig g_config;extern u8 g_mode;
void sync_from_dsw(void);
#endif

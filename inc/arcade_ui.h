#ifndef ARCADE_UI_H
#define ARCADE_UI_H
#include <genesis.h>
void arcade_font_load(void);
void arcade_gameplay_font_load(void);
void arcade_gameplay_labels(u8 two_players,u8 survival);
void arcade_gameplay_lives(u8 owner,u8 reserves);
void arcade_menu_begin(u8 logo);
#endif

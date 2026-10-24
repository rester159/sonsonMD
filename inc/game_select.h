/* game_select.h -- two-game cartridge chooser (SON SON / SON SON II).
 *
 * Runs once per power-on/reset, after the SEGA and CAPCOM splashes, in the
 * front-end's H40 mode. Blocks until a game is chosen and leaves a blank
 * screen. The artwork is a text placeholder until the owner's two selection
 * images are converted.
 */
#ifndef GAME_SELECT_H
#define GAME_SELECT_H

#include <genesis.h>

#define GAME_SONSON   0
#define GAME_SONSON2  1

u8 game_select_run(void);

#endif

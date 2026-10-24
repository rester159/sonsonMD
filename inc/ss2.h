/* ss2.h -- SON SON II (PC Engine, 1989) native Genesis port.
 *
 * Entered from the cartridge game chooser. Owns the VDP (H32, 64x32 planes,
 * HUD in the window plane), CRAM and sprites while it runs; returns to the
 * chooser with a blank screen.
 */
#ifndef SS2_H
#define SS2_H

#include <genesis.h>

void ss2_run(void);

#endif

/* ss2_logic.h -- SON SON II native game logic entry points.
 *
 * The logic modules operate on ss2_ram (ss2_ram.h) and read source tables
 * through ss2_rom_byte(), which reproduces the HuCard's gameplay bank mapping
 * (MPR2-5 = banks 1-4 at $4000-$BFFF, MPR7 = bank 0 at $E000, and the
 * routine's own bank at $C000).
 */
#ifndef SS2_LOGIC_H
#define SS2_LOGIC_H

#include <genesis.h>

/* source ROM banks kept for table reads (generated: src/ss2_rom.c) */
extern const u8 ss2_bank0[0x2000];
extern const u8 ss2_bank1[0x2000];
extern const u8 ss2_bank2[0x2000];
extern const u8 ss2_bank3[0x2000];
extern const u8 ss2_bank4[0x2000];
extern const u8 ss2_bank6[0x2000];
extern const u8 ss2_bank7[0x2000];
extern const u8 ss2_bank9[0x2000];

u8   ss2_rom_byte(u16 addr);                 /* bank 9 at $C000            */
u8   ss2_metatile_byte(u16 addr, u8 index);  /* bank 6|stage bit2 at $C000 */

/* presentation requests raised by the logic */
extern u8 ss2_staff_sprites_hidden;

/* flow events that the source handles by resetting its task stack */
void ss2_player_died(void);                  /* $F884                      */

/* routines whose rebuild is pending: counts and records the source address */
void ss2_unported(u16 source_addr);
extern u16 ss2_unported_last;
extern u16 ss2_unported_count;

void ss2_player_update(void);                /* bank 9 $C000               */
void ss2_sfx(u8 id);                         /* $BBF5                      */
u8   ss2_probe(u8 ox, u8 oy);                /* bank 3 $8307               */
void ss2_center_camera_x(void);              /* $C6A7                      */
void ss2_jump_air(void);                     /* $CA30                      */
void ss2_touch_down(void);                   /* $CBB7                      */
void ss2_grab_vine(void);                    /* $CBFA                      */

#endif

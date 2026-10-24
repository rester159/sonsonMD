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
extern const u8 ss2_bank5[0x2000];
extern const u8 ss2_bank8[0x2000];
extern const u8 ss2_bank17[0x2000];

extern u8 ss2_mpr6;                          /* bank mapped at $C000       */
void ss2_map6(u8 bank);                      /* $BB83 (also sets $8E)      */
u8   ss2_bank_byte(u8 bank, u16 addr);

u8   ss2_rom_byte(u16 addr);                 /* gameplay MPR mapping       */
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

/* objects (src/ss2_objects.c) */
void ss2_objects_update(void);               /* $ED0D loop over 32 slots   */
void ss2_obj_update(u8 x);                   /* $EE01                      */
void ss2_obj_common(u8 x);                   /* $EE67 script/motion/hits   */
void ss2_obj_die(u8 x);                      /* $F59F                      */
void ss2_obj_init(u8 x, u16 rec);            /* bank 3 $8451               */
u8   ss2_obj_alloc(void);                    /* bank 3 $8428 (0xFF = full) */
u8   ss2_obj_alloc_top(void);                /* bank 3 $8436               */
u8   ss2_obj_probe(u8 x);                    /* bank 3 $8327               */
void ss2_obj_snap_y(u8 x);                   /* bank 3 $858B               */
u8   ss2_spawn_visible(void);                /* bank 3 $85B1               */
u8   ss2_probe_point(u16 sx, u16 sy);        /* bank 3 $8379               */
void ss2_spawn_scan(void);                   /* bank 4 $BC10               */
void ss2_random(void);                       /* $FDB4                      */
void ss2_score_award(u8 id);                 /* bank 4 $BB89               */
void ss2_item_pickup(u8 x);                  /* bank 2 $7D65               */
void ss2_obj_death(u8 x);                    /* bank 2 $6E47               */
/* per-bank handlers: return 0 when the address is not a known entry */
u8   ss2_obj_handler(u8 bank, u16 addr, u8 x);
u8   ss2_spawn_handler(u8 bank, u16 addr);

#endif

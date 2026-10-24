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
extern const u8 ss2_bankF[0x2000];
extern const u8 ss2_bank1F[0x2000];
extern const u8 ss2_bank10[0x2000];
extern const u8 ss2_bank11[0x2000];
extern const u8 ss2_bank12[0x2000];
extern const u8 ss2_bank13[0x2000];
extern const u8 ss2_bank14[0x2000];
extern const u8 ss2_bank15[0x2000];
extern const u8 ss2_bank16[0x2000];
extern const u8 ss2_bank18[0x2000];
extern const u8 ss2_bank19[0x2000];
extern const u8 ss2_bank1A[0x2000];
extern const u8 ss2_bank1B[0x2000];
extern const u8 ss2_bank1C[0x2000];
extern const u8 ss2_bank1D[0x2000];
extern const u8 ss2_bank1E[0x2000];

extern u8 ss2_mpr6;                          /* bank mapped at $C000       */
void ss2_map6(u8 bank);                      /* $BB83 (also sets $8E)      */

/* CPU view per 8 KiB page (the MPR model): $2000 work RAM, $4000-$BFFF
 * banks 1-4, $C000 the current bank, $E000 bank 0 */
extern const u8 *ss2_page[8];
extern const u8 *const ss2_banks[0x20];      /* by HuCard bank number      */

static inline u8 ss2_rom_byte(u16 addr) { return ss2_page[addr >> 13][addr & 0x1FFF]; }
static inline u8 ss2_bank_byte(u8 bank, u16 addr) { return ss2_banks[bank & 0x1F][addr & 0x1FFF]; }
/* metatile table: bank 6 | stage bit 2 at $C000 */
static inline u8 ss2_metatile_byte(u16 addr, u8 index)
{
    extern u8 ss2_ram[0x2000];
    return ss2_banks[6 | ((ss2_ram[0x69] >> 2) & 1)][(u16)(addr + index) & 0x1FFF];
}

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

/* shots and magic (src/ss2_shots.c) */
void ss2_shots_update(void);                 /* bank 9 $D03D               */
void ss2_magic_cast(void);                   /* $D1E8 (then $CC35)         */
void ss2_magic_sweep(void);                  /* $D7BC, main body while $74 */
void ss2_magic_damage_all(u8 amount);        /* $D86A                      */
void ss2_shield_start(void);                 /* $D622 (from $FEAE)         */
void ss2_shield_end(void);                   /* $D702 (from $FEAE)         */

/* frame (src/ss2_game.c) */
extern u16 ss2_sat[64 * 4];                  /* PCE SATB layout            */
extern u8 ss2_hud_dirty;                     /* bit0 score, bit1 money     */
extern u8 ss2_hud_life[2], ss2_hud_magic[2]; /* bar shown/max this frame   */
void ss2_game_vblank(u8 pad);                /* pad in PCE $16 layout      */
void ss2_game_start(void);                   /* create the gameplay tasks  */
void ss2_game_frame(void);                   /* run tasks + sprite table   */
void ss2_game_tick(void);                    /* host replay: one pass      */
void ss2_main_restart(void);                 /* JMP $EDB0 from inside slot 0 */

/* dialogue window and HUD icons (src/ss2_msgwin.c): map writes queued
 * for the renderer */
enum { SS2_BAT_PUT, SS2_BAT_RESTORE, SS2_BAT_HUD, SS2_BAT_HUDPUT };
typedef struct { u8 kind, x, y; u16 w; } Ss2BatOp;   /* RESTORE: w = width<<8|rows */
#define SS2_BATQ 512
extern Ss2BatOp ss2_bat_queue[SS2_BATQ];
extern u16 ss2_bat_head, ss2_bat_tail;
void ss2_message(void);                      /* bank 2 $6984              */
void ss2_msg_open(void);                     /* $6997                     */
void ss2_msg_text(void);                     /* $69EE                     */
void ss2_msg_close(void);                    /* $6C1F                     */
void ss2_msg_continue(u16 addr, u8 y);       /* $6C12: JMP ($28) after a choice */
void ss2_hud_text(u16 ptr);                  /* bank 4 $BADE              */
void ss2_hud_put(u16 a, u16 w);              /* one HUD BAT word          */
void ss2_weapon_box(void);                   /* bank 0 $FE5A              */
void ss2_shots_update(void);                 /* bank 9 $D03D (ss2_shots.c) */
u8   ss2_npc_continue(u16 addr, u8 y);       /* src/ss2_npc.c (1 = handled) */

/* doors (src/ss2_door.c) */
extern u8 ss2_redraw_request;                /* $EBE1 playfield redraw     */
void ss2_door(u8 x);                         /* $7DB5 item kind 1          */

/* palette fades (src/ss2_fade.c): RAM shadow $2EA0-$2F9F -> ss2_vce */
extern u16 ss2_vce[512];                     /* PCE colour RAM mirror      */
extern u8 ss2_vce_dirty;
extern u8 ss2_fade_bg, ss2_fade_sp;          /* brightness 0-7 last uploaded */
extern u8 ss2_pal13_variant;                 /* door palette 13 variant, $FF none */
void ss2_vce_reset(void);
void ss2_fade_in(void);                      /* $71BA                      */
void ss2_fade_out(void);                     /* $71D7                      */
void ss2_fade_in_all(void);                  /* $7274                      */
void ss2_fade_out_all(void);                 /* $7297                      */
void ss2_fade_in_alt(void);                  /* $71F4                      */
void ss2_fade_out_alt(void);                 /* $7211                      */

/* sprite pattern sets (src/ss2_sprset.c): which ROM cell each PCE sprite
 * cell holds, plus the RAM animation bases the sets write */
extern u8 ss2_cell_bank[0x200];
extern u16 ss2_cell_src[0x200];
extern u8 ss2_cells_dirty;
void ss2_sprset_stage(void);                 /* $774F                      */
void ss2_sprset_interior(void);              /* $77A5 (list in $28/$29)    */
void ss2_sprset_5548(void);                  /* $77D0                      */
void ss2_staff_art(void);                    /* $79F7                      */

/* objects (src/ss2_objects.c) */
void ss2_objects_update(void);               /* $ED0D loop over 32 slots   */
extern u8 ss2_obj_next;                      /* slot the loop resumes after */
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
u8   ss2_random(void);                       /* $FDB4 (returns carry)      */
void ss2_score_award(u8 id);                 /* bank 4 $BB89               */
u8   ss2_item_pickup(u8 x);                  /* bank 2 $7D65 (1: rerun $EE67) */
void ss2_obj_death(u8 x);                    /* bank 2 $6E47               */
/* per-bank handlers: return 0 when the address is not a known entry */
u8   ss2_obj_handler(u8 bank, u16 addr, u8 x);
u8   ss2_spawn_handler(u8 bank, u16 addr);

#endif

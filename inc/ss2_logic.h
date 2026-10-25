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
extern const u8 ss2_bank1_jp[0x2000], ss2_bank2_jp[0x2000];   /* the Japanese text */
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
extern const u8 ss2_bankC[0x2000];
extern const u8 ss2_bankD[0x2000];
extern const u8 ss2_bankE[0x2000];
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
extern const u8 *ss2_banks[0x20];             /* by HuCard bank number      */

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
extern u8 ss2_sat_hi;                        /* entries >= this are empty  */
extern u8 ss2_hud_dirty;                     /* bit0 score, bit1 money     */
extern u8 ss2_hud_life[2], ss2_hud_magic[2]; /* bar shown/max this frame   */
void ss2_game_vblank(u8 pad);                /* pad in PCE $16 layout      */
void ss2_game_start(void);                   /* create the gameplay tasks  */
void ss2_game_frame(void);                   /* run tasks + sprite table   */
void ss2_game_tick(void);                    /* host replay: one pass      */
void ss2_boss_watch_start(void);             /* $E1E2 -> task 2 = bank 2 $6E1A (ss2_obj1f.c) */
void ss2_main_restart(void);                 /* JMP $EDB0 from inside slot 0 */
void ss2_main_loop(void);                    /* slot 0 $EC3B                */
void ss2_sat_update(void);                   /* $8732 outside the main body */
void ss2_game_listener(void);                /* slot 1 $E2A6                */
void ss2_score_task(void);                   /* slot 3 $FD05                */
void ss2_cr_set(u8 v);                       /* $2201 with $8B = 5          */
void ss2_bg_on(void);                        /* $BA1E                       */
void ss2_bg_off(void);                       /* $BA35                       */
void ss2_spr_on(void);                       /* $B9F0                       */
void ss2_spr_off(void);                      /* $BA07                       */

/* game flow (src/ss2_flow.c) */
void ss2_power_on(void);                     /* $E000/$E2D0: RAM + tasks    */
void ss2_game_start_task(void);              /* slot 0 $F5AA: new game      */
void ss2_death_task(void);                   /* slot 0 $F884: miss/demo end */
void ss2_stage_clear_start(void);            /* task 0 = $F617             */
void ss2_ending_task(void);                  /* slot 0 bank 4 $AD5A (ss2_ending.c) */
extern u8 ss2_ending_part;                   /* 0 none, 1 tally on the stage map, 2 picture, 3 credits */
extern s8 ss2_end_font_dx;                   /* credits font tiles $1C0-$1DF: glyph x offset (-8/8 blank) */
void ss2_end_petal(u8 x);                    /* bank 4 $B3E9 object handler */
void ss2_title_return(void);                 /* slot 0 $FB97: back to the title */

/* settings in cartridge SRAM (src/ss2_options.c, Genesis addition) */
#define SS2_LANG_ENGLISH  0
#define SS2_LANG_JAPANESE 1
extern u8 ss2_language;
void ss2_options_load(void);
void ss2_options_save(void);
void ss2_set_language(u8 lang);             /* text banks 1-2 */
enum { SS2_SCENE_CONTINUE = 2, SS2_SCENE_ENDING = 3 };   /* more ss2_scene values */
extern u8 ss2_scene;                         /* SS2_SCENE_* for the renderer */
extern u8 ss2_pal_intro;                     /* intro sprite palettes 8-15 */
extern u8 ss2_stage_request;                 /* renderer: load stage graphics */
enum { SS2_SCENE_PLAY, SS2_SCENE_TITLE };

/* dialogue window and HUD icons (src/ss2_msgwin.c): map writes queued
 * for the renderer */
enum { SS2_BAT_PUT, SS2_BAT_RESTORE, SS2_BAT_HUD, SS2_BAT_HUDPUT,
       SS2_BAT_CLEAR, SS2_BAT_PICTURE, SS2_BAT_TEXT, SS2_BAT_BOX };
/* RESTORE: w = width<<8|rows; PICTURE ($FBB1): x,y = width bytes, rows,
 * w = BAT address, w2 = source map; TEXT ($BAD9): w = table, w2 = BAT base */
typedef struct { u8 kind, x, y; u16 w, w2; } Ss2BatOp;
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
void ss2_bat_clear(void);                    /* $BA91                     */
void ss2_bat_picture(u16 dst, u16 src, u8 w, u8 h);   /* $FBB1            */
void ss2_bat_text(u16 ptr);                  /* $BAD9 (camera-relative)   */
void ss2_bat_box(u16 dst, u16 src);          /* bank 4 $A4CA: 8 RLE rows  */
void ss2_bat_put(u8 x, u8 y, u16 w);         /* one BAT word at a world tile */
extern u8 ss2_shop_screen, ss2_shop_type;    /* shop scene up; shopkeeper type */
extern u16 ss2_shop_goods;                   /* its goods table ($A666..)   */
void ss2_weapon_box(void);                   /* bank 0 $FE5A              */
void ss2_shots_update(void);                 /* bank 9 $D03D (ss2_shots.c) */
u8   ss2_npc_continue(u16 addr, u8 y);       /* src/ss2_npc.c (1 = handled) */

/* townspeople and shops (src/ss2_npc.c) */
extern u8 ss2_shop_screen;                   /* 1 while the shop interior is up */
void ss2_npc(u8 x);                          /* bank 3 $997F item kind 29  */
u8   ss2_drop_item(u8 p);                    /* bank 3 $9072/$9428 (ZP $35) */
void ss2_money_sub(u16 a);                   /* bank 4 $BA52               */
void ss2_hud_bars(void);                     /* bank 2 $7649 + $7688       */
void ss2_hud_life_bar(void);                 /* $7649                      */
void ss2_hud_magic_bar(void);                /* $7688                      */

/* doors (src/ss2_door.c) */
extern u8 ss2_redraw_request;                /* $EBE1 playfield redraw     */
void ss2_door(u8 x);                         /* $7DB5 item kind 1          */
void ss2_platform_carry(void);               /* bank 2 $7FD8               */

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
void ss2_fade_in_title(void);                /* $722E                      */
extern u8 ss2_fade_late;                     /* next fade's first step crosses a VBlank */
void ss2_fade_out_title(void);               /* $7251                      */
void ss2_fade_out_cont(void);                /* $72BA                      */
void ss2_fade_in_spr_alt(void);              /* $72DD (sprites 1-7, alt 8-15) */

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
void ss2_obj_camera(void);                   /* camera delta for $EE01     */
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

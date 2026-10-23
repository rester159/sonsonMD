#ifndef SONSON_SAVE_H
#define SONSON_SAVE_H
#include <genesis.h>
#include "arcade_ranking.h"
extern volatile u8 g_saveStatus;
void save_init(void);
void save_apply_settings(u8 mode);
void save_capture_settings(u8 mode);
bool save_commit(void);
u16 save_run_key(u8 mode,u8 players,u8 survival);
u32 save_best_score(u16 key);
u32 save_best_ticks(u16 key);
u32 save_mode_high(u8 mode);
void save_record_run(u16 key,u32 score,u32 ticks);
void save_reset_options(void);
void save_clear_scores(void);
void save_get_rankings(ArcadeRanking *out);
/* Stages finalized names/scores; caller uses save_commit at a safe boundary. */
void save_set_rankings(const ArcadeRanking *ranking);
#endif

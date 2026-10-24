#ifndef ARCADE_RESULTS_H
#define ARCADE_RESULTS_H
#include "arcade_types.h"
typedef struct {u8 stage,phase,timer,course;} ArcadeResults;
typedef struct {u8 clock,screen_busy,p1_out,p2_out,two_players,marker;} ArcadeResultsInput;
enum {
    ARCADE_RESULTS_PLAY=1, ARCADE_RESULTS_CLEAR=2,
    ARCADE_RESULTS_BOARD=4, ARCADE_RESULTS_PLAYER=8,
    ARCADE_RESULTS_DECOR=16, ARCADE_RESULTS_ANIMATE=32,
    ARCADE_RESULTS_AWARD=64, ARCADE_RESULTS_RESTART=128
};
typedef struct {u32 points[2];u16 flags;u8 player_mask,marker,number;} ArcadeResultsEvent;
/* 97F9..9A2B: caller starts at stage=phase=0, with completed-course byte7E.
 * CLEAR begins native screen preparation; screen_busy acknowledges completion.
 * Marker is sourceA5 (zero for ordinary A7F0 completion). */
void arcade_results_step(ArcadeResults *s,const ArcadeResultsInput *in,
                         ArcadeResultsEvent *event);
#endif

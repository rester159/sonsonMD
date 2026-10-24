#ifndef ARCADE_RESULTS_DECOR_H
#define ARCADE_RESULTS_DECOR_H
#include "arcade_types.h"
typedef struct {u16 tile;u8 code,attr,timer;} ArcadeResultsPose;
typedef struct {u16 x;u8 y;} ArcadeResultsPosition;
typedef struct {u8 next,timer,pose;} ArcadeResultsDecor;
extern const ArcadeResultsPose arcade_results_poses[3][2];
extern const ArcadeResultsPosition arcade_results_positions[3];
extern const u32 arcade_results_decor_pixels[24*8];
/* 992C initializes all three and immediately visits9617 once. */
void arcade_results_decor_begin(ArcadeResultsDecor *s,u8 index);
/* 99C5 calls9617 once per presentation update. Index must be0..2. */
void arcade_results_decor_step(ArcadeResultsDecor *s,u8 index);
#endif

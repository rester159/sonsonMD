#ifndef ARCADE_RANKING_H
#define ARCADE_RANKING_H
#include "arcade_types.h"
/* Source0300..0363, excluding transient name-entry records. */
typedef struct {u8 individual[5][8],total[5][12];} ArcadeRanking;
typedef struct {u8 qualified[3],row[3];} ArcadeRankingResult;
void arcade_ranking_init(ArcadeRanking *);
/* Scores are source packed bytes, not binary integers. 9BFF inserts P1,
 * 9C32 inserts P2 only in a two-player session, then9CD5 inserts total.
 * Result rows deliberately retain9C6D's equality-only P1 adjustment.
 * Newly inserted names are source blank16; name-entry handling is separate. */
void arcade_ranking_submit(ArcadeRanking *,const u8 scores[3][4],u8 two_players,
                           ArcadeRankingResult *);
/* 9EDC commits only qualifying rows, with both names concatenated for total. */
void arcade_ranking_names(ArcadeRanking *,const ArcadeRankingResult *,const u8 names[2][4]);
#endif

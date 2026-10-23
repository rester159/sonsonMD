#ifndef ARCADE_BARRIER_H
#define ARCADE_BARRIER_H
#include "arcade_world_object_pool.h"
#include "arcade_shots.h"
#include "arcade_player.h"
/* E569/E6FF: retain the entry X/Y for all20 tests but apply pushes to live X.
 * Returns1 for source kind=1 (crushed); caller owns the life-state transition. */
u8 arcade_barrier_player(const ArcadeWorldObjectPool *pool,ArcadePlayer *player,
                         u16 camera);
/* EB11 scans all20 slots even after a hit retires the shot. next_phase is
 * the phase byte four bytes beyond each object's current animation cursor. */
u8 arcade_barrier_shots(ArcadeWorldObjectPool *pool,const u8 next_phase[20],
                       ArcadePlayerShot *shot);
/* Ordered candidate lists may omit only records ineligible for this scan.
 * Flags and geometry are rechecked; input coordinates retain source semantics. */
u8 arcade_barrier_player_indexed(const ArcadeWorldObjectPool *pool,ArcadePlayer *player,
    u16 camera,const u8 *slots,u8 count);
u8 arcade_barrier_shots_indexed(ArcadeWorldObjectPool *pool,const u8 next_phase[20],
    ArcadePlayerShot *shot,const u8 *slots,u8 count);
#endif

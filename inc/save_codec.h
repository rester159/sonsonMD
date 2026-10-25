#ifndef SONSON_SAVE_CODEC_H
#define SONSON_SAVE_CODEC_H
#ifdef SGDK_GCC
#include <genesis.h>
#else
#include <stdint.h>
#endif
#include "arcade_ranking.h"
#define SAVE_SLOT_SIZE 512
#define SAVE_RECORDS 32
#define SAVE_COMMIT 0xA5
/* Version2 adds100 ranking bytes in formerly reserved space. Version1 loads
 * with original default rankings. Explicit bytes, CRC-32/ISO-HDLC. */
typedef struct { uint16_t key; uint32_t score, ticks; } SavedRecord;
typedef struct {
    uint8_t arcade1, arcade2, home1, home2, homeCredits;
    SavedRecord records[SAVE_RECORDS];
    ArcadeRanking ranking;
} SavedData;
uint32_t save_crc32(const uint8_t *p, unsigned n);
void save_encode(uint8_t *out, const SavedData *data, uint32_t generation);
int save_decode(const uint8_t *in, SavedData *data, uint32_t *generation);
int save_newer(uint32_t a, uint32_t b);
#endif

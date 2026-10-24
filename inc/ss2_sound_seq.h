/* Son Son II sound sequencer: native rebuild of the PC Engine driver in
 * HuCard banks 0x0A/0x0B (request $8003, tick $8000).  See docs/ss2/SOUND.md.
 *
 * This file is shared by the cartridge (ss2_sound.c) and the host
 * validation harness (tools/ss2/sound/host).  The sequencer interprets the
 * original song/sfx byte code (ss2_sound_data.c, extracted by
 * tools/ss2/sound/extract.py) and reports every PSG register write it would
 * make through ss2_snd_psg_write(); it never touches Genesis hardware.
 */
#ifndef SS2_SOUND_SEQ_H
#define SS2_SOUND_SEQ_H

#ifdef SS2_SND_HOST
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
#else
#include <genesis.h>
#endif

/* ---- generated data (src/ss2_sound_data.c) ------------------------------ */
extern const u8  ss2_snd_ftab[258];   /* PCE $81C8..$82C9: period table (+2 spill bytes) */
extern const u8  ss2_snd_bank[];      /* PCE $890A..: tables, instruments, songs, sfx */
extern const u16 ss2_snd_bank_len;
#define SS2_SND_BANK_BASE 0x890Au

/* ---- consumer hooks (implemented by ss2_sound.c or the host harness) ----- */
/* One HuC6280 PSG register write, program order: reg 0..9 = $0800..$0809. */
void ss2_snd_psg_write(u8 reg, u8 val);
/* Called just before the 32 waveform writes; src = PCE address of the
 * 32-byte wave in ss2_snd_bank (instrument + 0, or a $890A sfx wave). */
void ss2_snd_psg_wave(u16 src);

/* ---- sequencer ------------------------------------------------------------ */
void ss2_snd_seq_reset(void);        /* zeroed driver RAM + empty ring ($E2F4) */
void ss2_snd_seq_queue(u8 id);       /* bank 4 $BBF5: drop when the ring is full */
void ss2_snd_seq_request(u8 id);     /* $8003 directly (bypasses the ring) */
void ss2_snd_seq_tick(void);         /* $8000 */
void ss2_snd_seq_frame(void);        /* IRQ $E16D..$E191: pop one ring entry, then tick */
void ss2_snd_seq_set_hold(u8 v);     /* RAM $6B: nonzero = music muted and frozen (pause) */

/* Diagnostics: count of byte-code reads outside the extracted data and of
 * undefined opcodes (both are 0 for every request id; see host tests). */
extern u16 ss2_snd_seq_faults;

/* State image, exposed for tests: zero page $C8..$DF, music channels
 * $2DEC..$2E7B (6 x 0x18), sfx slots $2E7C..$2E9F (6 x 6, slot k = channel 5-k). */
typedef struct
{
    u8 zp[0x18];
    u8 ch[0x90];
    u8 sl[0x24];
    u8 hold;
    u8 ring[16];
    u8 ring_rd, ring_wr;
} Ss2SndState;
extern Ss2SndState ss2_snd;

#endif

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
/* PCE $81C8..$82C7 read as words: entry k = bytes 2k,2k+1 (the driver
 * indexes it with an even 8-bit offset, so 128 entries cover every index;
 * entries 96..127 are the code bytes that follow the 96-note table). */
extern const u16 ss2_snd_period[128];
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

/* Driver state.  Field comments give the source RAM location; values are
 * kept with the source's widths and wrap-around (u16 = byte pair). */
typedef struct
{
    const u8 *ptr;      /* +0/1  stream pointer, NULL = channel unused        */
    const u8 *inst;     /* $D2   instrument record of +9 (cached)            */
    u16 per;            /* +E/F  period << 2                                 */
    u16 vib;            /* +12/13 vibrato accumulator                        */
    u8 len, gate;       /* +2 length counter, +3 gate counter                */
    u8 gratio, flags;   /* +4 gate ratio, +5 flags                           */
    u8 voloff;          /* +6 volume offset (v - 31)                         */
    u8 env, envacc;     /* +7 level|phase, +8 rate accumulator               */
    u8 ins;             /* +9                                                */
    u8 oct, trn;        /* +A octave, +B transpose                           */
    u8 det, sld;        /* +C detune, +D slide (signed)                      */
    u8 lfoc, vibd;      /* +10 LFO counter, +11 vibrato delta                */
    u8 tremd, trem;     /* +14 tremolo delta, +15 tremolo level              */
    u8 rep, bal;        /* +16 repeat counter, +17 balance                   */
    /* cached copies of the (constant) instrument record bytes +$20..+$24 */
    u8 ar, dr, sl, rr;  /* attack, decay, sustain level, release rates      */
    u8 lfop;            /* +$24 & $7F: LFO half period                      */
    u8 lfor;            /* +$24 & $80: retrigger LFO on each untied note    */
    u8 lfo_on;          /* vibd|tremd != 0 (else the LFO counter is dead state:
                           only lfo_reset makes the deltas nonzero and it
                           also restarts the counter)                       */
    /* output cache: the PSG values the last envelope tick produced; valid
     * unless a fetch changed the channel (dirty).  Reused only when the
     * envelope level did not move and no LFO/slide is active, i.e. when the
     * source recomputes identical values; the writes are still emitted. */
    u16 o_per;          /* (vib + per) >> 2: regs 2/3, or reg 7 (|$80) in noise */
    u8 o_vol;           /* reg 4 value                                      */
    u8 recalc;          /* b0: fetched since last output (dirty);
                           b1: LFO or slide active (period changes per tick) */
} Ss2SndChan;

typedef struct
{
    u16 per;            /* +0/1 period << 2 */
    u8 slide, vol;      /* +2, +3 volume << 3 */
    u8 vslide, mode;    /* +4, +5 bit7 noise / wave+1 */
} Ss2SndSlot;

typedef struct
{
    Ss2SndChan ch[6];   /* $2DEC + 0x18*c */
    Ss2SndSlot sl[6];   /* $2E7C + 6*k, slot k = channel 5-k */
    const u8 *sp;       /* $DE/$DF sfx stream */
    u16 acc, step;      /* $D4:$D5 tempo accumulator, $D6:$D7 step */
    u8 last;            /* $D8 */
    u8 pri, mask;       /* $D9, $DA */
    u8 parsed, wait;    /* $DB, $DC */
    u8 loop;            /* $DD */
    u8 busy;            /* $C8 */
    u8 hold;            /* $6B */
    u8 ring[16];
    u8 ring_rd, ring_wr;
} Ss2SndState;
extern Ss2SndState ss2_snd;

#endif

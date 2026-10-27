#include "sound_ym_pitch.h"
/* sound.c -- SonSon NATIVE audio engine: record-&-replay arcade music + SFX,
 * mapped to the Genesis PSG + YM2612 and advanced from the V-Blank interrupt so
 * it never slows with the game loop.  See inc/sound.h for the full rationale and
 * the verified command-number table.
 *
 * This is a CLEAN, SELF-CONTAINED native module: it does NOT pull in the sibling
 * port's 6809 transliteration world (runtime_snd.h, snd_sub_E*.c).  The replay
 * engine + the AY->PSG/YM output mapping are adapted from the sibling port's
 * VERIFIED sound.c (the perceived-accuracy path the owner shipped), with the
 * live-6809 (SND_LIVE) branch and its hooks removed.  The only shared asset is
 * the recorded data blob, sound_replay_data.c (read-only).
 *
 * DATA: each sound command's per-frame AY register stream was recorded from the
 * verified driver and encoded (op<0x80 = wait N frames; 0x80|reg,val = write reg;
 * 0xFD mask,val = masked mixer write; 0xFE = end one-shot; 0xFF lo,hi = loop).
 * Two concurrent slots per chip ([0]=music, [1]=sfx, routed by index bit31) let
 * an SFX borrow a channel without disturbing the music.
 */
#include <genesis.h>
#include "sound.h"
#include "game.h"

/* ---- recorded data (sound_replay_data.c, generated) ----------------------- */
extern const uint8_t  snd_replay_blob[];
extern const uint32_t snd_replay_index[64][2];

/* ---- captured AY-3-8910 register state (our own; no 6809 runtime) ----------
 * ay[0] = AY#1 (SFX chip), ay[1] = AY#2 (music chip).  16 regs each.
 * ay[2] = a PRIVATE register file for the layered 1-up jingle (see below): its
 * own AY state so the jingle never collides with the music or SFX registers. */
static u8 ay[3][16];

/* ---- replay channels: [chip][slot], slot 0 = music, 1 = sfx --------------- */
typedef struct {
    const u8 *base;        /* stream start (loop-jump target base)              */
    const u8 *pos;
    u16       wait;
    u8        active;
    u8        oneshot;     /* treat the generator's forced 0xFF loop as the end */
} ReplayCh;
static ReplayCh rp[2][2];

/* ===========================================================================
 *  LAYERED 1-UP / EXTRA-LIFE JINGLE  (cmd 0x04)  -- the one thing that must play
 *  *over* the BGM.  The arcade sound-6809 fired the 1-up jingle on a SPARE AY
 *  channel layered over the running music; the 2-slot-per-chip record-&-replay
 *  cannot (both the BGM and the recorded 0x04 jingle are 3-voice AY#2/PSG music
 *  streams that share one register file + the same 3 PSG voices, so arming 0x04
 *  via the normal index REPLACES the BGM -- why it was gated off in game.c).
 *
 *  FAITHFUL FIX: give the jingle its OWN dedicated replay channel + register
 *  file (ay[2]) rendered to the YM2612's *second* FM bank (channels 3-5), which
 *  the SFX path (ch0-2) and the PSG BGM never touch.  The jingle is three
 *  square-wave AY voices -> three YM square voices: same timbre class as every
 *  other SFX in this engine, and now a TRUE layer over BGM+SFX, exactly the
 *  arcade's spare-channel behaviour.  No existing slot/index/output is touched,
 *  so the BGM and the gunshot/pickup SFX are bit-for-bit unaffected.
 *
 *  DATA: the SAME verified 0x04 AY#2 register stream that sound_replay_data.c's
 *  index routes to the music slot, but re-encoded as a ONE-SHOT (the recorded
 *  jingle self-mutes its mixer at the end; the generator's force-loop -- meant
 *  for endless BGM -- turned that one-shot into an infinite loop, the other half
 *  of why 0x04 could not be layered).  Reproduce with rec/cmd_04.txt chip-B
 *  (bytes 16..31) + encode_replay.py's encode() WITHOUT the is_music force-loop
 *  (end 0xFE instead of 0xFF).  Plays once (~3.2 s @480Hz) then holds silence. */
static const u8 snd_1up_oneshot[] = {
    0x11,0x80,0xB4,0x81,0x00,0x82,0x8F,0x83,0x00,0x84,0x78,0x85,0x00,0x86,0x17,0xFD,
    0x3F,0x38,0x88,0x0B,0x89,0x0B,0x8A,0x0B,0x8B,0x00,0x8C,0x00,0x8D,0x00,0x8E,0x00,
    0x8F,0x00,0x07,0x88,0x0A,0x89,0x0A,0x8A,0x0A,0x12,0x88,0x09,0x89,0x09,0x8A,0x09,
    0x1A,0x88,0x08,0x89,0x08,0x8A,0x08,0x12,0x88,0x07,0x89,0x07,0x8A,0x07,0x19,0x88,
    0x06,0x89,0x06,0x8A,0x06,0x15,0x88,0x05,0x89,0x05,0x8A,0x05,0x19,0x88,0x04,0x89,
    0x04,0x8A,0x04,0x12,0x88,0x0B,0x89,0x0B,0x8A,0x0B,0x07,0x88,0x0A,0x89,0x0A,0x8A,
    0x0A,0x08,0x88,0x09,0x89,0x09,0x8A,0x09,0x08,0x88,0x08,0x89,0x08,0x8A,0x08,0x0A,
    0x88,0x07,0x89,0x07,0x8A,0x07,0x0F,0x88,0x06,0x89,0x06,0x8A,0x06,0x08,0x88,0x05,
    0x89,0x05,0x8A,0x05,0x0C,0x88,0x04,0x89,0x04,0x8A,0x04,0x09,0x88,0x0B,0x89,0x0B,
    0x8A,0x0B,0x07,0x88,0x0A,0x89,0x0A,0x8A,0x0A,0x07,0x88,0x09,0x89,0x09,0x8A,0x09,
    0x0A,0x88,0x08,0x89,0x08,0x8A,0x08,0x08,0x88,0x07,0x89,0x07,0x8A,0x07,0x0F,0x88,
    0x06,0x89,0x06,0x8A,0x06,0x09,0x88,0x05,0x89,0x05,0x8A,0x05,0x09,0x88,0x04,0x89,
    0x04,0x8A,0x04,0x0D,0x88,0x0B,0x89,0x0B,0x8A,0x0B,0x02,0x88,0x0A,0x89,0x0A,0x8A,
    0x0A,0x07,0x88,0x09,0x89,0x09,0x8A,0x09,0x0A,0x88,0x08,0x89,0x08,0x8A,0x08,0x0B,
    0x88,0x07,0x89,0x07,0x8A,0x07,0x0D,0x88,0x06,0x89,0x06,0x8A,0x06,0x0C,0x88,0x05,
    0x89,0x05,0x8A,0x05,0x08,0x88,0x04,0x89,0x04,0x8A,0x04,0x09,0x80,0xA0,0x82,0x87,
    0x84,0x6B,0x88,0x0B,0x89,0x0B,0x8A,0x0B,0x06,0x88,0x0A,0x89,0x0A,0x8A,0x0A,0x13,
    0x88,0x09,0x89,0x09,0x8A,0x09,0x18,0x88,0x08,0x89,0x08,0x8A,0x08,0x13,0x88,0x07,
    0x89,0x07,0x8A,0x07,0x18,0x88,0x06,0x89,0x06,0x8A,0x06,0x16,0x88,0x05,0x89,0x05,
    0x8A,0x05,0x19,0x88,0x04,0x89,0x04,0x8A,0x04,0x12,0x80,0xB4,0x88,0x0B,0x89,0x0B,
    0x8A,0x0B,0x06,0x88,0x0A,0x89,0x0A,0x8A,0x0A,0x13,0x88,0x09,0x89,0x09,0x8A,0x09,
    0x18,0x88,0x08,0x89,0x08,0x8A,0x08,0x16,0x88,0x07,0x89,0x07,0x8A,0x07,0x19,0x88,
    0x06,0x89,0x06,0x8A,0x06,0x13,0x88,0x05,0x89,0x05,0x8A,0x05,0x19,0x88,0x04,0x89,
    0x04,0x8A,0x04,0x12,0x84,0x5F,0x88,0x0B,0x89,0x0B,0x8A,0x0B,0x06,0x88,0x0A,0x89,
    0x0A,0x8A,0x0A,0x13,0x88,0x09,0x89,0x09,0x8A,0x09,0x17,0x88,0x08,0x89,0x08,0x8A,
    0x08,0x17,0x88,0x07,0x89,0x07,0x8A,0x07,0x19,0x88,0x06,0x89,0x06,0x8A,0x06,0x12,
    0x88,0x05,0x89,0x05,0x8A,0x05,0x19,0x88,0x04,0x89,0x04,0x8A,0x04,0x16,0x80,0xA0,
    0x82,0x78,0x84,0x5A,0x88,0x0B,0x89,0x0B,0x8A,0x0B,0x02,0x88,0x0A,0x89,0x0A,0x8A,
    0x0A,0x5E,0x88,0x09,0x89,0x09,0x8A,0x09,0x5C,0x88,0x08,0x89,0x08,0x8A,0x08,0x5E,
    0x88,0x07,0x89,0x07,0x8A,0x07,0x5F,0x88,0x06,0x89,0x06,0x8A,0x06,0x5A,0x88,0x05,
    0x89,0x05,0x8A,0x05,0x5C,0x88,0x04,0x89,0x04,0x8A,0x04,0x5E,0xFD,0x3F,0x3F,0xFE,
};
static ReplayCh rp_1up;        /* dedicated 1-up channel -> ay[2] -> YM ch3-5   */

static u8 snd_ready = 0;   /* set by snd_init; gates the V-Blank tick           */

/* ===========================================================================
 *  ONE-SHOT CUES vs LOOPING BGM  (VERIFIED 2026-10-24 on the real sound 6809)
 *  encode_replay.py force-looped EVERY music-slot stream, but most of them are
 *  short cues, not BGM. Injecting each command into MAME's `sonson` sound CPU
 *  (main-CPU latch $3010 + FIRQ strobe $3019, -wavwrite RMS) shows:
 *    - 0x10/0x11/0x13/0x17: real looping BGM (kept as loops);
 *    - 0x04/0x05/0x06/0x16/0x18: ONE-SHOT, LAYERED over the running BGM, which
 *      keeps playing (latch-level cues; gameplay selectors are translated below);
 *    - 0x14 (player death): ONE-SHOT that STOPS the whole BGM (both chips), then
 *      silence until the game re-sends a BGM command.
 *  Played through the forced loop, repeated latch cues 0x04/0x05/0x06 replaced
 *  the BGM with a ~3 s phrase repeating forever. The layered cues now play on
 *  the dedicated layer channel (rp_1up / ay[2] -> YM ch3-5, see above), so the
 *  BGM slots are untouched; a newer cue simply replaces the older one there.
 * =========================================================================== */
static bool cmd_is_layered_cue(u8 cmd)
{
    return cmd == 0x04 || cmd == 0x05 || cmd == 0x06 || cmd == 0x16 || cmd == 0x18;
}
#define SND_CMD_DEATH_STOPS_BGM 0x14

/* Arm the layer channel on `stream` (its own AY file ay[2], starts muted). */
static void layer_start(const u8 *stream, u8 oneshot)
{
    u8 i;
    for (i = 0; i < 16; i++) ay[2][i] = 0;
    ay[2][7] = 0xFF;                                  /* start muted; stream opens it */
    rp_1up.base = rp_1up.pos = stream;
    rp_1up.wait = 0;
    rp_1up.oneshot = oneshot;
    rp_1up.active = 1;
}

/* ===========================================================================
 *  REPLAY ENGINE  (cmd -> arm streams; per-tick -> apply register writes)
 * =========================================================================== */
static void replay_start(u8 cmd)
{
    if (cmd == SND_CMD_STOP)
    {
        /* stop/reset (the arcade pairs 00 + <song>): halt everything + silence
         * the registers the output mapping reads. */
        u8 c;
        for (c = 0; c < 2; c++)
        {
            rp[c][0].active = rp[c][1].active = 0;
            ay[c][7] = 0xFF;                          /* mixer: all channels off */
            ay[c][8] = ay[c][9] = ay[c][10] = 0;      /* volumes 0               */
        }
        rp_1up.active = 0;
        ay[2][7] = 0xFF;
        ay[2][8] = ay[2][9] = ay[2][10] = 0;
        return;
    }

    {
        const uint32_t *e = snd_replay_index[cmd & 0x3F];
        u8 c;

        if (cmd_is_layered_cue(cmd))
        {
            /* One recorded chip per cue; 0x04 uses its hand-verified one-shot. */
            if (cmd == 0x04) { layer_start(snd_1up_oneshot, 0); return; }
            for (c = 0; c < 2; c++)
                if (e[c] != 0xFFFFFFFFu)
                {
                    layer_start(snd_replay_blob + (e[c] & 0xFFFFFFu), 1);
                    return;
                }
            return;
        }

        if (cmd == SND_CMD_DEATH_STOPS_BGM)
        {
            /* the death tune silences the whole BGM, not just the chip it uses */
            for (c = 0; c < 2; c++)
                if (rp[c][0].active)
                {
                    rp[c][0].active = 0;
                    ay[c][7] = 0xFF;
                    ay[c][8] = ay[c][9] = ay[c][10] = 0;
                }
        }

        for (c = 0; c < 2; c++)
        {
            if (e[c] != 0xFFFFFFFFu)
            {
                u8 slot = (e[c] & 0x80000000u) ? 0 : 1;     /* music : sfx       */
                ReplayCh *r = &rp[c][slot];
                r->base = r->pos = snd_replay_blob + (e[c] & 0xFFFFFFu);
                r->wait = 0;
                r->oneshot = (cmd == SND_CMD_DEATH_STOPS_BGM);
                r->active = 1;
            }
        }
    }
}

/* Advance one replay channel `r` by one tick, applying its register writes into
 * the 16-byte AY register file `regs`.  (Channel + register file are decoupled so
 * the dedicated 1-up channel can decode the same byte language into its own ay[2]
 * file -- see snd_1up_oneshot.) */
static void replay_tick_into(ReplayCh *r, u8 *regs)
{
    if (!r->active) return;
    if (r->wait) { r->wait--; return; }
    for (;;)
    {
        u8 op = *r->pos++;
        if (op < 0x80)                       /* wait `op` frames (this + op-1)   */
        {
            r->wait = (u16)(op - 1);
            return;
        }
        if (op == 0xFE) { r->active = 0; return; }   /* one-shot done (hold)     */
        if (op == 0xFF)                              /* loop-jump                */
        {
            if (r->oneshot)                          /* forced loop: cue is over */
            {
                regs[7] = 0xFF;                      /* mute (recording ends mid-fade) */
                regs[8] = regs[9] = regs[10] = 0;
                r->active = 0;
                return;
            }
            u16 off = (u16)(r->pos[0] | (r->pos[1] << 8));
            r->pos = r->base + off;
            continue;
        }
        if (op == 0xFD)                              /* masked mixer (reg7) write */
        {
            u8 mask = *r->pos++;
            u8 val  = *r->pos++;
            regs[7] = (u8)((regs[7] & (u8)~mask) | val);
            continue;
        }
        regs[op & 0x1F] = *r->pos++;                 /* normal register write    */
    }
}

static void replay_tick(u8 chip, u8 slot)
{
    replay_tick_into(&rp[chip][slot], ay[chip]);
}

/* ===========================================================================
 *  AY#2 (MUSIC) -> Genesis PSG  (3 squares + noise; exact AY timbre class)
 *  period: f_ay = 1.5MHz/(16*p) ; psg_tone = 3.579545MHz/(32*f) = p*1.1932
 *          (integer: (p*1221)>>10).  vol: AY 0..15 -> PSG atten 15-v.
 * =========================================================================== */
static void ay_chip_to_psg(const u8 *r)
{
    u8 mixer = r[7];
    u8 ch;
    for (ch = 0; ch < 3; ch++)
    {
        u16 period   = (u16)(((r[2 * ch + 1] & 0x0F) << 8) | r[2 * ch]);
        u8  vol      = r[8 + ch];
        bool tone_on = ((mixer >> ch) & 1) == 0;        /* AY: 0 = enabled       */
        if (!tone_on || period == 0 || (vol & 0x1F) == 0)
        {
            PSG_setEnvelope(ch, PSG_ENVELOPE_MIN);
            continue;
        }
        {
            u16 tone = (u16)(((u32)period * 1221) >> 10);
            if (tone > 0x3FF) tone = 0x3FF;             /* PSG tone is 10-bit     */
            PSG_setTone(ch, tone);
        }
        {
            u8 v = (vol & 0x10) ? 15 : (vol & 0x0F);    /* env mode -> full       */
            PSG_setEnvelope(ch, (u8)(15 - v));
        }
    }
    /* noise: any AY noise enable (mixer bits 3-5, 0=on) drives PSG noise ch3.   */
    {
        bool noise_on = ((mixer >> 3) & 7) != 7;
        if (noise_on)
        {
            u8 np = r[6] & 0x1F;
            PSG_setNoise(PSG_NOISE_TYPE_WHITE,
                         np < 8  ? PSG_NOISE_FREQ_CLOCK2 :
                         np < 16 ? PSG_NOISE_FREQ_CLOCK4 : PSG_NOISE_FREQ_CLOCK8);
            {
                u8 nv = 0, ch2;
                for (ch2 = 0; ch2 < 3; ch2++)
                    if (((mixer >> (3 + ch2)) & 1) == 0 && (r[8 + ch2] & 0x0F) > nv)
                        nv = r[8 + ch2] & 0x0F;
                PSG_setEnvelope(3, (u8)(15 - nv));
            }
        }
        else PSG_setEnvelope(3, PSG_ENVELOPE_MIN);
    }
}

/* ===========================================================================
 *  AY -> YM2612 single-operator square-ish voice.
 *  Setup (once): alg7, OP1-3 muted (TL=127), OP4 the sole carrier with instant
 *  attack / no decay / fast release -- a steady tone gated by key on/off + TL.
 *  Per tick: F-num from the AY period (fnum*2^block ~= 1845281/p); TL from vol.
 *  TWO FM banks are configured: part 0 (ch0-2) renders AY#1 SFX; part 1 (ch3-5)
 *  renders the layered 1-up jingle (ay[2]) -- the spare FM voices the SFX never
 *  use, so the jingle layers over both the SFX and the PSG music.
 * =========================================================================== */
static u8 ym_keyed[3];      /* part 0 / ch0-2  (SFX)        */
static u8 ym_keyed2[3];     /* part 1 / ch3-5  (1-up layer) */

/* Configure one FM channel (part p, channel index c 0-2 within the part) as the
 * single-OP4 square voice, and key it off.  The within-part register offsets are
 * identical for both parts; key on/off (global reg 0x28) selects the channel by
 * code (0..2 for part 0, 4..6 for part 1). */
static void ym_voice_init(u8 p, u8 c)
{
    static const u8 op_off[4] = { 0x0, 0x4, 0x8, 0xC };
    u8 o;
    for (o = 0; o < 4; o++)
    {
        YM2612_writeReg(p, 0x30 + op_off[o] + c, 0x01);   /* DT=0 MUL=1   */
        YM2612_writeReg(p, 0x40 + op_off[o] + c, 0x7F);   /* TL: muted    */
        YM2612_writeReg(p, 0x50 + op_off[o] + c, 0x1F);   /* AR=31        */
        YM2612_writeReg(p, 0x60 + op_off[o] + c, 0x00);   /* D1R=0        */
        YM2612_writeReg(p, 0x70 + op_off[o] + c, 0x00);   /* D2R=0        */
        YM2612_writeReg(p, 0x80 + op_off[o] + c, 0x0F);   /* SL=0 RR=15   */
        YM2612_writeReg(p, 0x90 + op_off[o] + c, 0x00);
    }
    YM2612_writeReg(p, 0xB0 + c, 0x07);                   /* alg7 FB0     */
    YM2612_writeReg(p, 0xB4 + c, 0xC0);                   /* L+R          */
    YM2612_writeReg(0, 0x28, (u8)((p ? 0x04 : 0x00) | c));/* key off      */
}

static void snd_ym_init(void)
{
    /* The YM2612 lives on the Z80 side of the bus: 68k register writes are
     * SILENTLY DROPPED unless the Z80 bus is held.  We never run a Z80 sound
     * driver (PSG is 68k-side; the SFX replay drives the YM from the 68k), so
     * park the Z80: NULL driver + hold the bus permanently -- every
     * YM2612_writeReg afterwards lands.  (This re-establishes the bus-held state
     * after the CAPCOM splash, which resets the YM + returns the Z80 to halt.) */
    Z80_loadDriver(Z80_DRIVER_NULL, TRUE);
    Z80_requestBus(TRUE);

    {
        u8 c;
        for (c = 0; c < 3; c++)
        {
            ym_voice_init(0, c);  ym_keyed[c]  = 0;       /* SFX bank (ch0-2)    */
            ym_voice_init(1, c);  ym_keyed2[c] = 0;       /* 1-up bank (ch3-5)   */
        }
    }
}

#define SFX_TL_BIAS 12          /* SFX attenuation floor (~9 dB), restoring audibility under
                                 * the music; owner-tunable (+1 ~= -0.75 dB).      */
static void ay1_to_ym(void)
{
    const u8 *r = ay[0];
    u8 mixer = r[7];
    u8 c;
    for (c = 0; c < 3; c++)
    {
        u16 period = (u16)(((r[2 * c + 1] & 0x0F) << 8) | r[2 * c]);
        u8  vol    = r[8 + c];
        u8  v      = (vol & 0x10) ? 15 : (vol & 0x0F);
        bool on = (((mixer >> c) & 1) == 0) && period != 0 && v != 0;
        if (!on)
        {
            if (ym_keyed[c]) { YM2612_writeReg(0, 0x28, 0x00 | c); ym_keyed[c] = 0; }
            continue;
        }
        {
            const u16 pitch = sound_ym_pitch[period];
            YM2612_writeReg(0, 0xA4 + c, (u8)(pitch >> 8));
            YM2612_writeReg(0, 0xA0 + c, (u8)pitch);
        }
        YM2612_writeReg(0, 0x4C + c, (u8)((15 - v) * 2 + SFX_TL_BIAS)); /* OP4 TL */
        if (!ym_keyed[c]) { YM2612_writeReg(0, 0x28, 0xF0 | c); ym_keyed[c] = 1; }
    }
}

#define ONEUP_TL_BIAS 6         /* 1-up jingle sits ABOVE the SFX floor (it is a
                                 * reward cue meant to be heard over the BGM, like
                                 * the arcade's spare-channel layer); still a touch
                                 * under full to blend.  Owner-tunable.            */
/* ay[2] (the 1-up jingle's private AY file) -> YM2612 part 1, ch3-5.  Identical
 * mapping to ay1_to_ym but on the second FM bank with its own key state, so the
 * jingle plays as three square voices LAYERED over the SFX (ch0-2) + PSG music. */
static void ay_1up_to_ym(void)
{
    const u8 *r = ay[2];
    u8 mixer = r[7];
    u8 c;
    for (c = 0; c < 3; c++)
    {
        u16 period = (u16)(((r[2 * c + 1] & 0x0F) << 8) | r[2 * c]);
        u8  vol    = r[8 + c];
        u8  v      = (vol & 0x10) ? 15 : (vol & 0x0F);
        bool on = (((mixer >> c) & 1) == 0) && period != 0 && v != 0;
        if (!on)
        {
            if (ym_keyed2[c]) { YM2612_writeReg(0, 0x28, 0x04 | c); ym_keyed2[c] = 0; }
            continue;
        }
        {
            const u16 pitch = sound_ym_pitch[period];
            YM2612_writeReg(1, 0xA4 + c, (u8)(pitch >> 8));
            YM2612_writeReg(1, 0xA0 + c, (u8)pitch);
        }
        YM2612_writeReg(1, 0x4C + c, (u8)((15 - v) * 2 + ONEUP_TL_BIAS)); /* OP4 TL */
        if (!ym_keyed2[c]) { YM2612_writeReg(0, 0x28, 0xF0 | 0x04 | c); ym_keyed2[c] = 1; }
    }
}

/* ===========================================================================
 *  V-BLANK TICK  (registered via SYS_setVIntCallback in snd_init)
 *  Fires at a constant ~60Hz HW V-Blank -- NEVER dropped under game load.  We
 *  advance the replay a fixed 8 ticks (= 480Hz, the arcade sound-IRQ rate) and
 *  render to PSG/YM once, so tempo + SFX stay rock-steady regardless of how busy
 *  the 68k game loop is.  This is SonSon's separate-sound-CPU behaviour on the
 *  Genesis timebase.  (Runs in V-Int context, exactly like the verified port.)
 * =========================================================================== */
static void snd_vblank(void)
{
    u8 t;
    if (!snd_ready) return;

    for (t = 0; t < 8; t++)
    {
        replay_tick(0, 0); replay_tick(0, 1);        /* chip A: music first, sfx */
        replay_tick(1, 0); replay_tick(1, 1);        /* chip B                   */
        replay_tick_into(&rp_1up, ay[2]);            /* layered 1-up jingle      */
    }

    ay_chip_to_psg(ay[1]);                           /* AY#2 music -> PSG        */

    /* HOLD THE Z80 BUS for the YM writes: SGDK's YM2612_writeReg busy-waits on
     * the YM STATUS port (a read of the bus the Z80 shares).  With the bus held
     * that read is clean; without it, if the Z80 is contending the wait can spin.
     * (We park the Z80 with the bus held in snd_init, but re-assert here so the
     * write window is always clean even across SGDK's per-frame DMA handshake.) */
    {
        u16 bus_taken = Z80_getAndRequestBus(TRUE);
        ay1_to_ym();                                 /* AY#1 SFX -> YM2612 ch0-2 */
        ay_1up_to_ym();                              /* 1-up jingle -> YM ch3-5  */
        if (!bus_taken) Z80_releaseBus();
    }
}

/* ===========================================================================
 *  PUBLIC API
 * =========================================================================== */
void snd_init(void)
{
    u8 c, s;

    /* clear captured register state + replay slots (incl. the 1-up's ay[2]) */
    for (c = 0; c < 3; c++)
    {
        u8 i;
        for (i = 0; i < 16; i++) ay[c][i] = 0;
        ay[c][7] = 0xFF;                             /* mixer: everything off    */
        if (c < 2) for (s = 0; s < 2; s++) rp[c][s].active = 0;
    }
    rp_1up.active = 0;

    snd_ym_init();                                   /* parks Z80 + sets SFX voice */

    /* PSG: start fully silent (the music tick re-enables channels as it plays). */
    for (c = 0; c < 4; c++) PSG_setEnvelope(c, PSG_ENVELOPE_MIN);

    snd_ready = 1;
    SYS_setVIntCallback(snd_vblank);                 /* decoupled 480Hz advance   */
}

void snd_shutdown(void)
{
    snd_ready = 0;
    SYS_setVIntCallback(NULL);                        /* stop advancing            */
    replay_start(SND_CMD_STOP);                       /* halt + silence regs       */
    rp_1up.active = 0;                                 /* stop the 1-up layer       */
    {
        u8 c;
        for (c = 0; c < 4; c++) PSG_setEnvelope(c, PSG_ENVELOPE_MIN);
        for (c = 0; c < 3; c++) YM2612_writeReg(0, 0x28, 0x00 | c);        /* ch0-2 off */
        for (c = 0; c < 3; c++) YM2612_writeReg(0, 0x28, 0x04 | c);        /* ch3-5 off */
    }
}

void snd_play_coin(void)
{
    replay_start(SND_CMD_COIN);
}

/* Main ROM $63AB/$63DD: gameplay queues event selectors, not sound-CPU
 * latch bytes. In particular food 4 -> 2, fire 8 -> 11, kill 12/13 -> 8.
 * The replay recordings are indexed by the latch byte. */
static const u8 event_to_latch[25] = {
    0x00,0x01,0x09,0x04,0x02,0x03,0x09,0x0A,0x0B,0x0C,
    0x06,0x0F,0x08,0x08,0x0E,0x10,0x00,0x11,0x12,0x13,
    0x14,0x15,0x16,0x17,0x18
};

void snd_command(u8 cmd)
{
    /* Original 6347: ordinary commands require demo sound (52) or live play
     * (22). Shutdown uses replay_start directly so cleanup is never gated. */
    if (g_attract && !g_config.demoSound) return;
    if (cmd < sizeof event_to_latch) replay_start(event_to_latch[cmd]);
}

/* Play the extra-life / 1-up jingle (arcade cmd 0x04) LAYERED over the running
 * BGM: arms the dedicated 1-up channel (its own ay[2] file -> YM ch3-5), so the
 * music + SFX are untouched.  One-shot (~3.2 s) then self-silences; re-arming
 * (a second extra life) restarts it cleanly.  See the snd_1up_oneshot block. */
void snd_play_1up(void)
{
    /* 6410 sends this event through the same ordinary-command gate. */
    if (g_attract && !g_config.demoSound) return;
    layer_start(snd_1up_oneshot, 0);
}

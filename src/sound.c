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

/* ---- recorded data (sound_replay_data.c, generated) ----------------------- */
extern const uint8_t  snd_replay_blob[];
extern const uint32_t snd_replay_index[64][2];

/* ---- captured AY-3-8910 register state (our own; no 6809 runtime) ----------
 * ay[0] = AY#1 (SFX chip), ay[1] = AY#2 (music chip).  16 regs each. */
static u8 ay[2][16];

/* ---- replay channels: [chip][slot], slot 0 = music, 1 = sfx --------------- */
typedef struct {
    const u8 *base;        /* stream start (loop-jump target base)              */
    const u8 *pos;
    u16       wait;
    u8        active;
} ReplayCh;
static ReplayCh rp[2][2];

static u8 snd_ready = 0;   /* set by snd_init; gates the V-Blank tick           */

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
        return;
    }

    {
        const uint32_t *e = snd_replay_index[cmd & 0x3F];
        u8 c;
        for (c = 0; c < 2; c++)
        {
            if (e[c] != 0xFFFFFFFFu)
            {
                u8 slot = (e[c] & 0x80000000u) ? 0 : 1;     /* music : sfx       */
                ReplayCh *r = &rp[c][slot];
                r->base = r->pos = snd_replay_blob + (e[c] & 0xFFFFFFu);
                r->wait = 0;
                r->active = 1;
            }
        }
    }
}

static void replay_tick(u8 chip, u8 slot)
{
    ReplayCh *r = &rp[chip][slot];
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
            u16 off = (u16)(r->pos[0] | (r->pos[1] << 8));
            r->pos = r->base + off;
            continue;
        }
        if (op == 0xFD)                              /* masked mixer (reg7) write */
        {
            u8 mask = *r->pos++;
            u8 val  = *r->pos++;
            ay[chip][7] = (u8)((ay[chip][7] & (u8)~mask) | val);
            continue;
        }
        ay[chip][op & 0x1F] = *r->pos++;             /* normal register write    */
    }
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
    }
}

/* ===========================================================================
 *  AY#1 (SFX) -> YM2612 ch0-2, single-operator square-ish voice.
 *  Setup (once): alg7, OP1-3 muted (TL=127), OP4 the sole carrier with instant
 *  attack / no decay / fast release -- a steady tone gated by key on/off + TL.
 *  Per tick: F-num from the AY period (fnum*2^block ~= 1845281/p); TL from vol.
 * =========================================================================== */
static u8 ym_keyed[3];

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
        static const u8 op_off[4] = { 0x0, 0x4, 0x8, 0xC };
        u8 c, o;
        for (c = 0; c < 3; c++)
        {
            for (o = 0; o < 4; o++)
            {
                YM2612_writeReg(0, 0x30 + op_off[o] + c, 0x01);   /* DT=0 MUL=1   */
                YM2612_writeReg(0, 0x40 + op_off[o] + c, 0x7F);   /* TL: muted    */
                YM2612_writeReg(0, 0x50 + op_off[o] + c, 0x1F);   /* AR=31        */
                YM2612_writeReg(0, 0x60 + op_off[o] + c, 0x00);   /* D1R=0        */
                YM2612_writeReg(0, 0x70 + op_off[o] + c, 0x00);   /* D2R=0        */
                YM2612_writeReg(0, 0x80 + op_off[o] + c, 0x0F);   /* SL=0 RR=15   */
                YM2612_writeReg(0, 0x90 + op_off[o] + c, 0x00);
            }
            YM2612_writeReg(0, 0xB0 + c, 0x07);                   /* alg7 FB0     */
            YM2612_writeReg(0, 0xB4 + c, 0xC0);                   /* L+R          */
            YM2612_writeReg(0, 0x28, 0x00 | c);                   /* key off      */
            ym_keyed[c] = 0;
        }
    }
}

#define SFX_TL_BIAS 24          /* SFX attenuation floor (~18 dB) so SFX sit under
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
            u32 tot = 1845281UL / period;             /* fnum * 2^block          */
            u8  block = 0;
            while (tot >= 2048 && block < 7) { tot >>= 1; block++; }
            if (tot >= 2048) tot = 2047;
            YM2612_writeReg(0, 0xA4 + c, (u8)((block << 3) | (tot >> 8)));
            YM2612_writeReg(0, 0xA0 + c, (u8)tot);
        }
        YM2612_writeReg(0, 0x4C + c, (u8)((15 - v) * 2 + SFX_TL_BIAS)); /* OP4 TL */
        if (!ym_keyed[c]) { YM2612_writeReg(0, 0x28, 0xF0 | c); ym_keyed[c] = 1; }
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
    }

    ay_chip_to_psg(ay[1]);                           /* AY#2 music -> PSG        */

    /* HOLD THE Z80 BUS for the YM writes: SGDK's YM2612_writeReg busy-waits on
     * the YM STATUS port (a read of the bus the Z80 shares).  With the bus held
     * that read is clean; without it, if the Z80 is contending the wait can spin.
     * (We park the Z80 with the bus held in snd_init, but re-assert here so the
     * write window is always clean even across SGDK's per-frame DMA handshake.) */
    {
        u16 bus_taken = Z80_getAndRequestBus(TRUE);
        ay1_to_ym();                                 /* AY#1 SFX -> YM2612 voice */
        if (!bus_taken) Z80_releaseBus();
    }
}

/* ===========================================================================
 *  PUBLIC API
 * =========================================================================== */
void snd_init(void)
{
    u8 c, s;

    /* clear captured register state + replay slots */
    for (c = 0; c < 2; c++)
    {
        u8 i;
        for (i = 0; i < 16; i++) ay[c][i] = 0;
        ay[c][7] = 0xFF;                             /* mixer: everything off    */
        for (s = 0; s < 2; s++) rp[c][s].active = 0;
    }

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
    {
        u8 c;
        for (c = 0; c < 4; c++) PSG_setEnvelope(c, PSG_ENVELOPE_MIN);
        for (c = 0; c < 3; c++) YM2612_writeReg(0, 0x28, 0x00 | c);   /* key off  */
    }
}

void snd_command(u8 cmd)
{
    replay_start(cmd);
}

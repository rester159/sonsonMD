/* Son Son II sound sequencer -- native C rebuild of the PC Engine driver in
 * HuCard banks 0x0A ($8000-$9FFF) and 0x0B ($A000-$BFFF).
 *
 * Control flow mirrors the source routines (addresses cited per block);
 * state lives in a byte image of the driver's RAM (Ss2SndState) so the
 * 8-bit carry/wrap behaviour is the source's.  The song/sfx byte code is the
 * original data (ss2_sound_data.c) interpreted natively: this is an
 * interpreter of the driver's data format, not HuC6280 emulation.
 * Output = the exact PSG register write sequence of the original driver,
 * delivered through ss2_snd_psg_write(); verified write-for-write against the
 * research model by tools/ss2/sound/validate.py.  Contract: docs/ss2/SOUND.md.
 */
#include "ss2_sound_seq.h"

Ss2SndState ss2_snd;
u16 ss2_snd_seq_faults;

/* zero-page aliases ($C8..$DF) */
#define ZP(a)   ss2_snd.zp[(a) - 0xC8]
#define C8      ZP(0xC8)   /* busy: request in progress (tick skipped)        */
#define C9      ZP(0xC9)
#define CA      ZP(0xCA)
#define CB      ZP(0xCB)
#define CC      ZP(0xCC)
#define CD      ZP(0xCD)
#define CE      ZP(0xCE)
#define D4      ZP(0xD4)   /* tempo accumulator hi (tick count)              */
#define D5      ZP(0xD5)   /* tempo accumulator lo                           */
#define D6      ZP(0xD6)   /* tempo step hi  ($3000 / tempo, 8.8 fixed)      */
#define D7      ZP(0xD7)   /* tempo step lo                                  */
#define D8      ZP(0xD8)   /* last processed tick                             */
#define D9      ZP(0xD9)   /* active sfx priority                             */
#define DA      ZP(0xDA)   /* sfx channel mask (bit 5-c = channel c)          */
#define DB      ZP(0xDB)   /* sfx: stream parsed this tick                    */
#define DC      ZP(0xDC)   /* sfx: wait counter                               */
#define DD      ZP(0xDD)   /* sfx: loop counter                               */
#define DE      ZP(0xDE)   /* sfx stream pointer lo                           */
#define DF      ZP(0xDF)   /* sfx stream pointer hi                           */

static u16 inst;           /* $D2/$D3: current channel's instrument record    */

#ifdef SS2_SND_HOST
void ss2_snd_host_event(char kind, u8 ch, u8 byte);   /* validation harness only */
#define HOST_EVENT(k, c, b) ss2_snd_host_event((k), (u8)(c), (b))
#else
#define HOST_EVENT(k, c, b) ((void)0)
#endif

/* ---- data access ------------------------------------------------------ */
static u8 rd(u16 a)
{
    u16 o = (u16)(a - SS2_SND_BANK_BASE);
    if (a >= SS2_SND_BANK_BASE && o < ss2_snd_bank_len) return ss2_snd_bank[o];
    ss2_snd_seq_faults++;
    return 0;
}

/* ---- PSG output ($807D / $806B / $806F) -------------------------------- */
#define psg(r, v) ss2_snd_psg_write((r), (v))
static void psg_m(u8 r, u8 v) { if (!(DA & 0x80)) psg(r, v); }   /* $806B */
static void psg_silence(void) { psg(4, 0x00); psg(5, 0xAA); psg(7, 0x00); }

/* ---- arithmetic ($8006 multiply, $8023 divide) ------------------------- */
static void mul_c9_ce(void)                 /* C9:CA = C9 * CE */
{
    u16 r = (u16)C9 * CE;
    C9 = (u8)(r >> 8); CA = (u8)r; CD = 0;
}

static u16 div_tempo(u8 t)                  /* $3000 / t (C9:CA / CB:CC) */
{
    if (!t) { ss2_snd_seq_faults++; return 0; }   /* source loops forever */
    return (u16)(0x3000u / t);
}

/* ---- music channel helpers -------------------------------------------- */
static u8 next(u8 *p)                       /* $858B: fetch byte, ptr++   */
{
    u16 a = (u16)(p[0] | (p[1] << 8));
    u8 v = rd(a);
    a++;
    p[0] = (u8)a; p[1] = (u8)(a >> 8);
    CB = v;
    return v;
}

static void inst_ptr(u8 *p)                 /* $84AF: D2 = $8A0D + 40*inst */
{
    C9 = 0x28; CE = p[9];
    mul_c9_ce();
    inst = (u16)((rd(0x89B5) | (rd(0x89B6) << 8)) + ((C9 << 8) | CA));
}

static void load_wave(u8 *p)                /* $848B */
{
    u8 y;
    inst_ptr(p);
    if (DA & 0x80) return;
    psg(4, 0x40); psg(4, 0x00);
    ss2_snd_psg_wave(inst);
    for (y = 0; y < 32; y++) psg(6, rd((u16)(inst + y)));
    psg(4, 0x80);
}

/* $815B: re-establish music output on channels whose DA bit is clear */
static void restore_music(void)
{
    u8 save = DA, cf;
    u8 *p = ss2_snd.ch;
    DA = (u8)(DA << 2);
    for (cf = 6; cf; cf--)
    {
        psg(0, (u8)(6 - cf));
        if (!(DA & 0x80))
        {
            psg_silence();
            if (p[0] | p[1])
            {
                psg(5, p[0x17]);
                DA &= 0x7F;
                load_wave(p);
            }
        }
        DA = (u8)(DA << 1);
        p += 0x18;
    }
    DA = save;
    psg(1, 0xFF); psg(8, 0x00); psg(9, 0x00);
}

static void stop_music(void)                /* $8146 */
{
    u8 i;
    D4 = D5 = D6 = D7 = 0; D8 = 0xFF;
    for (i = 0; i < 0x90; i++) ss2_snd.ch[i] = 0;
    restore_music();
}

static void stop_sfx(void)                  /* $812C */
{
    u8 i;
    DA ^= 0x3F;
    restore_music();
    D9 = DA = DC = DD = 0;
    for (i = 0; i < 0x24; i++) ss2_snd.sl[i] = 0;
}

/* ---- request ($8081 / $808A) ------------------------------------------- */
void ss2_snd_seq_request(u8 a)
{
    u8 n, c, pri;
    u16 hdr;
    C8 = 0xFF;
    if (a >= 0xF0)
    {                                       /* $811B: F0 all, F1 sfx, F2/F3 music */
        switch (a & 3)
        {
            case 0: stop_music(); stop_sfx(); break;
            case 1: stop_sfx(); break;
            default: stop_music(); break;
        }
        C8 = 0; return;
    }
    n = (u8)(rd(0x89B4) + 1); CB = n;       /* 43 entries; id reduced mod 43 */
    while (a >= n) a = (u8)(a - n);
    hdr = (u16)(rd((u16)(0x89B7 + 2 * a)) | (rd((u16)(0x89B8 + 2 * a)) << 8));
    pri = rd(hdr);
    if (pri == 0)
    {                                       /* music: 6 channel pointers */
        stop_music();
        for (c = 0; c < 6; c++)
        {
            u8 *p = ss2_snd.ch + c * 0x18;
            p[0] = rd((u16)(hdr + 1 + 2 * c));
            p[1] = rd((u16)(hdr + 2 + 2 * c));
            p[0x17] = 0xAA;
        }
    }
    else if (pri >= D9)
    {                                       /* sfx: priority, channel mask, stream */
        u8 mask = rd((u16)(hdr + 1)), y;
        DA = (u8)((mask ^ DA) & DA);
        stop_sfx();
        D9 = pri;
        DA = mask; CB = mask;
        for (y = 6; y; y--)
        {
            u8 cbit = CB & 1;
            CB >>= 1;
            if (cbit) { psg(0, (u8)(y - 1)); psg(5, 0xAA); }
        }
        hdr = (u16)(hdr + 2);
        DE = (u8)hdr; DF = (u8)(hdr >> 8);
    }
    C8 = 0;
}

/* ---- music channel tick ------------------------------------------------ */
static void lfo_reset(u8 *p)                /* $83B0 */
{
    p[0x10] = 0; p[0x12] = 0; p[0x13] = 0; p[0x15] = 0xFF;
    p[0x11] = rd((u16)(inst + 0x25));
    p[0x14] = rd((u16)(inst + 0x26));
}

static void note_length(u8 *p, u8 a)        /* $83D2 */
{
    u8 x = (u8)(a >> 5), cc = p[5], acc;
    CC = cc; CB = 0;
    acc = (cc & 8) ? 2 : 3;
    for (;;)
    {
        x--;
        if (!x) break;
        CB = acc; acc = (u8)(acc << 1);
    }
    if (cc & 1)
    {
        acc = (u8)(acc + CB);
        cc &= (u8)~1; CC = cc; p[5] = cc;
    }
    p[2] = acc; CE = acc;
    C9 = p[4];
    mul_c9_ce();
    p[3] = C9 ? C9 : 1;
}

static void note(u8 *p, u8 a)               /* $8321 */
{
    u8 n, cc;
    HOST_EVENT('m', (p - ss2_snd.ch) / 0x18, a);
    note_length(p, a);
    n = a & 0x1F;
    if (!n) { p[3] = 0; return; }           /* rest */
    cc = p[5]; CC = cc;
    p[5] = (cc & 2) ? (u8)(cc | 4) : (u8)(cc & ~4);   /* tie flag for the next note */
    if (!(cc & 4))
    {                                       /* not tied from previous note */
        u16 v;
        n--;
        if (cc & 0x80) { CA = n; C9 = 0; }  /* noise: pitch value = noise freq */
        else
        {
            u8 o = p[0x0A], idx, lo, hi, s;
            u16 t;
            if (o & 0x80) o = (u8)((o & 0x7F) + 0x18);
            o = (u8)(o + p[0x0B]);
            o = (u8)(o + n);
            CB = o;
            idx = (u8)(o << 1);
            s = (p[0x0C] & 0x80) ? 0xFF : 0x00;
            t = (u16)p[0x0C] + ss2_snd_ftab[idx];
            lo = (u8)t;
            hi = (u8)(s + ss2_snd_ftab[idx + 1] + (t >> 8));
            CA = lo; C9 = hi;
        }
        v = (u16)(((C9 << 8) | CA) << 2);
        CA = (u8)v; C9 = (u8)(v >> 8);
        p[0x0E] = CA; p[0x0F] = C9;
        p[7] = 0; p[8] = 0;
    }
    cc = p[5]; CC = cc;                     /* $8398 */
    if (cc & 0x10) { p[5] = (u8)(cc & ~0x10); lfo_reset(p); }
    else if ((rd((u16)(inst + 0x24)) & 0x80) && !(cc & 4)) lfo_reset(p);
}

/* returns 1 when the channel ended ($855B pops the dispatcher frame) */
static u8 command(u8 *p, u8 a)              /* $841B */
{
    u8 v;
    switch (a)
    {
        case 0x00:                          /* $8446 tempo */
        {
            u16 q = div_tempo(next(p));
            D6 = (u8)(q >> 8); D7 = (u8)q;
            D4 = D5 = D8 = 0;
            break;
        }
        case 0x01: p[4] = next(p); break;   /* $8465 gate ratio /256 */
        case 0x02:                          /* $846D volume: offset = v - 31 */
            v = next(p);
            p[6] = (u8)((((v & 0x1F) ^ 0x1F) ^ 0xFF) + 1);
            break;
        case 0x03:                          /* $847C instrument */
            p[9] = next(p);
            p[5] |= 0x10;
            load_wave(p);
            break;
        case 0x04: p[5] ^= 0x08; break;     /* $84CE triplet lengths */
        case 0x05: p[5] ^= 0x02; break;     /* $84D2 tie (slur) mode */
        case 0x06: p[5] |= 0x01; break;     /* $84DB dot next note */
        case 0x07: v = next(p); p[0x0A] = (u8)((p[0x0A] & 0x80) | v); break; /* octave */
        case 0x08: p[0x0A] |= 0x80; break;  /* $84F2 +2 octaves */
        case 0x09: p[0x0A] &= 0x7F; break;  /* $84FB */
        case 0x0A: p[0x0B] = next(p); break;/* $8504 transpose */
        case 0x0B: p[0x0C] = next(p); break;/* $850C detune (period add) */
        case 0x0C: p[0x0D] = next(p); break;/* $8513 pitch slide */
        case 0x0D:                          /* $851A repeat n times */
            v = next(p);
            if (v)
            {
                if (!p[0x16]) p[0x16] = CB;
                else
                {
                    p[0x16]--;
                    if (!p[0x16]) goto skip2;
                }
            }
            goto jump;
        case 0x0E:                          /* $8532 break out on last pass */
            if (p[0x16] == 1) { p[0x16] = 0; goto jump; }
            goto skip2;
        case 0x0F:                          /* $853D jump */
        jump:
        {
            u8 lo = next(p), hi = next(p);
            p[1] = hi; p[0] = lo;
            break;
        }
        skip2:                              /* $854B */
        {
            u16 t = (u16)((p[0] | (p[1] << 8)) + 2);
            p[0] = (u8)t; p[1] = (u8)(t >> 8);
            break;
        }
        case 0x10:                          /* $855B end of channel */
        {
            u8 i;
            for (i = 0; i < 0x18; i++) p[i] = 0;
            if (!(DA & 0x80)) psg_silence();
            return 1;
        }
        case 0x11:                          /* $856E noise toggle */
            p[5] ^= 0x80;
            if (!(p[5] & 0x80)) psg_m(7, 0x00);
            break;
        case 0x12:                          /* $8580 balance */
            v = next(p);
            p[0x17] = v;
            psg_m(5, v);
            break;
        default:                            /* $13-$1E: undefined in source */
            ss2_snd_seq_faults++;
            for (v = 0; v < 0x18; v++) p[v] = 0;
            return 1;
    }
    return 0;
}

static void env_rate(u8 *p, u8 rate, u8 step)   /* $8606 */
{
    for (;;)
    {
        u8 c9 = p[8], a;
        u16 t;
        if (c9 >= 0x10) { p[8] = (u8)(c9 - 0x10); return; }
        p[8] = (u8)(rate + c9);
        t = (u16)p[7] + step;
        a = (u8)t;
        if (step & 0x80) { if (!(t >> 8)) a &= 3; }
        else if (t >> 8) a |= 0xF8;
        p[7] = a;
    }
}

static u8 sat_add(u8 a, u8 d)               /* signed delta, clamped 0..FF */
{
    u16 t = (u16)a + d;
    if (d & 0x80) return (t >> 8) ? (u8)t : 0;
    return (t >> 8) ? 0xFF : (u8)t;
}

static void output(u8 *p)                   /* $866E */
{
    u16 v;
    u8 a, cd;
    v = (u16)((p[0x12] | (p[0x13] << 8)) + (u16)(s16)(s8)p[0x11]);
    p[0x12] = (u8)v; p[0x13] = (u8)(v >> 8);
    p[0x15] = sat_add(p[0x15], p[0x14]);
    v = (u16)((p[0x0E] | (p[0x0F] << 8)) + (u16)(s16)(s8)p[0x0D]);
    p[0x0E] = (u8)v; p[0x0F] = (u8)(v >> 8);
    v = (u16)((p[0x12] | (p[0x13] << 8)) + v);
    v >>= 2;
    CA = (u8)v; C9 = (u8)(v >> 8);
    if (p[5] & 0x80) { CA |= 0x80; psg_m(7, CA); }
    else { psg_m(2, CA); psg_m(3, C9); }
    cd = (u8)(p[7] >> 3);
    a = (u8)(p[0x15] >> 3);
    if (!a) a = 1;
    if (a >= cd) a = cd;
    a = (u8)(a + p[6]);
    if (a & 0x80) a = 0;
    psg_m(4, (u8)(a | 0x80));
}

static void negate(u8 *x) { *x = (u8)((u8)(*x - 1) ^ 0xFF); }

static void envelope(u8 *p)                 /* $85AB */
{
    u8 r;
    switch (p[7] & 3)
    {
        case 0:                             /* attack to level $F8 */
            if ((p[7] & 0xF8) == 0xF8 || !(r = rd((u16)(inst + 0x20)))) { p[7]++; break; }
            env_rate(p, r, 0x08);
            break;
        case 1:                             /* decay to sustain level */
            r = rd((u16)(inst + 0x21));
            if (!r) { p[7]++; break; }
            env_rate(p, r, 0xF8);
            if ((p[7] & 0xF8) >= rd((u16)(inst + 0x22))) break;
            p[7] = (u8)((p[7] & 3) | rd((u16)(inst + 0x22)));
            p[7]++;
            break;
        case 2: break;                      /* sustain */
        default:                            /* release */
            r = rd((u16)(inst + 0x23));
            if (r) env_rate(p, r, 0xF8);
            break;
    }
    output(p);                              /* $863E */
    {
        u8 cb = rd((u16)(inst + 0x24)) & 0x7F, a;
        a = (u8)(p[0x10] + 1);
        p[0x10] = a;
        if ((a & 0x7F) == cb)
        {
            a = (u8)((a & 0x80) ^ 0x80);
            p[0x10] = a;
            if (a & 0x80) negate(&p[0x11]);
            negate(&p[0x14]);
        }
    }
}

static void channel_tick(u8 *p)             /* $82DF */
{
    u8 a;
    if (ss2_snd.hold) { psg_m(4, 0x80); return; }
    inst_ptr(p);
    a = p[2];
    if (a && --a)
    {
        u8 cc;
        p[2] = a;
        cc = p[5]; CC = cc;
        a = p[3];
        if (a) { a--; p[3] = a; if (a) { envelope(p); return; } }
        if (!(cc & 4)) p[7] |= 3;           /* gate expired: release */
        envelope(p);
        return;
    }
    for (;;)                                /* $8317 fetch */
    {
        a = next(p);
        if (a >= 0x1F) break;
        if (command(p, a)) return;
    }
    note(p, a);
}

static void music_tick(void)                /* $8288 */
{
    u16 acc = (u16)(((D4 << 8) | D5) + ((D6 << 8) | D7));
    D4 = (u8)(acc >> 8); D5 = (u8)acc;
    while (D4 != D8)
    {
        u8 save, cf;
        u8 *p = ss2_snd.ch;
        D8++;
        save = DA;                          /* $82A1 */
        DA = (u8)(DA << 2);
        for (cf = 6; cf; cf--)
        {
            psg(0, (u8)(6 - cf));
            if (p[0] | p[1]) channel_tick(p);
            DA = (u8)(DA << 1);
            p += 0x18;
        }
        DA = save;
    }
}

/* ---- sfx ($8718) -------------------------------------------------------- */
static u8 snext(void)                       /* $88F6 */
{
    u16 a = (u16)(DE | (DF << 8));
    u8 v = rd(a);
    a++;
    DE = (u8)a; DF = (u8)(a >> 8);
    CB = v;
    return v;
}

static void sfx_volume_out(u8 *q) { psg(4, (u8)((q[3] >> 3) | 0x80)); }   /* $88E8 */

/* returns 1 when the effect ended (stream command 2/3) */
static u8 sfx_channel(u8 *q)                /* $8753 */
{
    u8 a;
    if (!DB && DC)
    {                                       /* between events: slides */
        if (q[2])
        {
            u16 v = (u16)((q[0] | (q[1] << 8)) + (u16)(s16)(s8)q[2]);
            q[0] = (u8)v; q[1] = (u8)(v >> 8);
            v >>= 2;
            CA = (u8)v; C9 = (u8)(v >> 8);
            if (q[5] & 0x80) psg(7, (u8)(CA | 0x80));
            else { psg(2, CA); psg(3, C9); }
        }
        if (!q[4]) return 0;
        q[3] = sat_add(q[3], q[4]);
        sfx_volume_out(q);
        return 0;
    }
    for (;;)                                /* $87BC */
    {
        a = snext();
        if (a & 0x80) break;
        switch (a)
        {
            case 0: DC = snext(); DB = 0xFF; break;          /* wait */
            case 1:                                          /* loop */
            {
                u8 x = snext();
                if (x)
                {
                    if (!DD) DD = x;
                    else if (!--DD)
                    {
                        u16 t = (u16)((DE | (DF << 8)) + 2);
                        DE = (u8)t; DF = (u8)(t >> 8);
                        break;
                    }
                }
                {
                    u8 lo = snext(), hi = snext();
                    DF = hi; DE = lo;
                }
                break;
            }
            case 2: case 3: return 1;                        /* end */
            case 4: psg(5, snext()); break;                  /* balance */
            default: ss2_snd_seq_faults++; return 1;
        }
    }
    /* $881E: event for this channel */
    HOST_EVENT('s', 5 - (q - ss2_snd.sl) / 6, a);
    if (a & 1) q[5] |= 0x80;
    else { q[5] &= 0x7F; psg(7, 0x00); }
    {
        u8 b1 = snext(), b2 = snext(), w, b;
        if (b1 != 0xFF)
        {
            u16 v;
            if (q[5] & 0x80) { C9 = 0; CA = b1; psg(7, (u8)(b1 | 0x80)); }
            else
            {
                u8 idx = (u8)(b1 << 1);
                u8 s = (b2 & 0x80) ? 0xFF : 0x00;
                u16 t = (u16)b2 + ss2_snd_ftab[idx];
                CA = (u8)t;
                C9 = (u8)(s + ss2_snd_ftab[idx + 1] + (t >> 8));
                psg(2, CA); psg(3, C9);
            }
            v = (u16)(((C9 << 8) | CA) << 2);
            q[0] = (u8)v; q[1] = (u8)(v >> 8);
        }
        q[2] = snext();                     /* period slide per tick */
        w = snext();                        /* waveform 0..4 */
        b = snext();
        if (b != 0xFF) q[3] = (u8)((b & 0x1F) << 3);
        q[4] = snext();                     /* volume slide per tick */
        CB = w;
        a = q[5];
        if (!(a & 0x80))
        {
            CB = (u8)(CB + 1);
            if (a != CB)
            {
                u16 src;
                u8 y = (u8)((u8)(CB - 1) << 1);
                q[5] = CB;
                src = (u16)(rd((u16)(0x890A + y)) | (rd((u16)(0x890B + y)) << 8));
                psg(4, 0x40); psg(4, 0x00);
                ss2_snd_psg_wave(src);
                for (y = 0; y < 32; y++) psg(6, rd((u16)(src + y)));
            }
        }
        sfx_volume_out(q);
    }
    return 0;
}

static void sfx_tick(void)                  /* $8718 */
{
    u8 save, cf;
    u8 *q = ss2_snd.sl;
    if (!DA) return;
    save = DA;
    for (cf = 6; cf; cf--)
    {
        psg(0, (u8)(cf - 1));
        if (DA & 1)
        {
            if (sfx_channel(q)) { stop_sfx(); return; }   /* $880E: DA still shifted */
        }
        DA >>= 1;
        q += 6;
    }
    DA = save;
    DC--;
    DB = 0;
}

/* ---- public ------------------------------------------------------------- */
void ss2_snd_seq_tick(void)                 /* $8060 */
{
    if (C8) return;
    sfx_tick();
    music_tick();
}

void ss2_snd_seq_reset(void)
{
    u16 i;
    u8 *b = (u8 *)&ss2_snd;
    for (i = 0; i < sizeof(ss2_snd); i++) b[i] = 0;
    for (i = 0; i < 16; i++) ss2_snd.ring[i] = 0xFF;
    ss2_snd_seq_faults = 0;
    inst = 0;
}

void ss2_snd_seq_set_hold(u8 v) { ss2_snd.hold = v; }

void ss2_snd_seq_queue(u8 id)               /* bank 4 $BBF5 */
{
    u8 w = ss2_snd.ring_wr;
    if (ss2_snd.ring[w] != 0xFF) return;    /* full: dropped, as the source */
    ss2_snd.ring[w] = id;
    ss2_snd.ring_wr = (u8)((w + 1) & 0x0F);
}

void ss2_snd_seq_frame(void)                /* bank 0 $E16D..$E191 */
{
    u8 r = ss2_snd.ring_rd, id = ss2_snd.ring[r];
    ss2_snd.ring[r] = 0xFF;
    if (id != 0xFF)
    {
        ss2_snd_seq_request(id);
        ss2_snd.ring_rd = (u8)((r + 1) & 0x0F);
    }
    ss2_snd_seq_tick();
}

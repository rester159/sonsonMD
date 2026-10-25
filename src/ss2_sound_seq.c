/* Son Son II sound sequencer -- native C rebuild of the PC Engine driver in
 * HuCard banks 0x0A ($8000-$9FFF) and 0x0B ($A000-$BFFF).
 *
 * Control flow mirrors the source routines (addresses cited per block).
 * State is typed for the 68000 (Ss2SndState: word periods, pointer-based
 * stream fetch, cached instrument records, per-channel output cache) while
 * keeping the source's 8/16-bit wrap-around; scratch zero page ($C9-$D3) is
 * not materialised because no value in it survives between source calls.
 * The output cache only avoids recomputing values whose inputs did not
 * change; every PSG write of the source is still emitted, in order.
 * The song/sfx byte code is the original data (ss2_sound_data.c) interpreted
 * natively: an interpreter of the driver's data format, not CPU emulation.
 * Output = the exact PSG register write sequence of the original driver,
 * delivered through ss2_snd_psg_write(); verified write-for-write against the
 * research model by tools/ss2/sound/validate.py.  Contract: docs/ss2/SOUND.md.
 */
#include "ss2_sound_seq.h"

Ss2SndState ss2_snd;
u16 ss2_snd_seq_faults;

#define BANK(a)   (ss2_snd_bank + (u16)((a) - SS2_SND_BANK_BASE))
#define INST_BASE 0x8A0Du       /* [$89B5] */

#ifdef SS2_SND_HOST
void ss2_snd_host_event(char kind, u8 ch, u8 byte);   /* validation harness only */
#define HOST_EVENT(k, c, b) ss2_snd_host_event((k), (u8)(c), (b))
#else
#define HOST_EVENT(k, c, b) ((void)0)
#endif

#ifdef SS2_SND_STATS
unsigned long ss2_stat_ticks, ss2_stat_chan, ss2_stat_fetch, ss2_stat_env;
#define STAT(x) (x)++
#else
#define STAT(x)
#endif

/* ---- data access ------------------------------------------------------
 * Host: every byte-code read is bounds-checked (faults counted, 0 read).
 * Target: reads were validated in range for every id (validate.py). */
#ifdef SS2_SND_HOST
static u8 chk(const u8 *p)
{
    if (p >= ss2_snd_bank && p < ss2_snd_bank + ss2_snd_bank_len) return *p;
    ss2_snd_seq_faults++;
    return 0;
}
#define RD(p)     chk(p)
#else
#define RD(p)     (*(p))
#endif
static u8 rd(u16 a) { return RD(BANK(a)); }

static inline u8 next(Ss2SndChan *c) { const u8 *p = c->ptr; c->ptr = p + 1; return RD(p); }
static inline u8 snext(void) { const u8 *p = ss2_snd.sp; ss2_snd.sp = p + 1; return RD(p); }
static inline u8 ins(const Ss2SndChan *c, u8 off) { return RD(c->inst + off); }

static const u8 *addr_ptr(u8 lo, u8 hi) { return BANK((u16)(lo | (hi << 8))); }

/* ---- PSG output ($807D / $806B / $806F) -------------------------------- */
#define psg(r, v) ss2_snd_psg_write((r), (v))
#define psg_m(r, v) do { if (!mute) psg((r), (v)); } while (0)          /* $806B: mute =
                                     bit 7 of the shifted $DA for the channel */
static void psg_silence(void) { psg(4, 0x00); psg(5, 0xAA); psg(7, 0x00); }

/* ---- music channel helpers -------------------------------------------- */
static void set_inst(Ss2SndChan *c)         /* $84AF: $8A0D + 40*inst */
{
    u8 b;
    c->inst = BANK((u16)(INST_BASE + (u16)c->ins * 40u));
    c->ar = ins(c, 0x20); c->dr = ins(c, 0x21); c->sl = ins(c, 0x22); c->rr = ins(c, 0x23);
    b = ins(c, 0x24);
    c->lfop = b & 0x7F; c->lfor = b & 0x80;
}

static void load_wave(Ss2SndChan *c, u8 mute)   /* $848B */
{
    if (mute) return;
    psg(4, 0x40); psg(4, 0x00);
    ss2_snd_psg_wave((u16)(INST_BASE + (u16)c->ins * 40u));
#ifdef SS2_SND_HOST
    {
        u8 y;
        for (y = 0; y < 32; y++) psg(6, ins(c, y));
    }
#endif
    psg(4, 0x80);
}

/* $815B: re-establish music output on channels whose $DA bit is clear */
static void restore_music(void)
{
    u8 m = (u8)(ss2_snd.mask << 2), i;
    Ss2SndChan *c = ss2_snd.ch;
    for (i = 0; i < 6; i++, c++, m = (u8)(m << 1))
    {
        psg(0, i);
        if (!(m & 0x80))
        {
            psg_silence();
            if (c->ptr)
            {
                psg(5, c->bal);
                load_wave(c, 0);                /* RMB7 $DA */
            }
        }
    }
    psg(1, 0xFF); psg(8, 0x00); psg(9, 0x00);
}

static void clear_chan(Ss2SndChan *c)
{
    u8 *b = (u8 *)c, i;
    for (i = 0; i < sizeof(*c); i++) b[i] = 0;
    c->ptr = 0; c->inst = 0;
}

static void stop_music(void)                /* $8146 */
{
    u8 i;
    ss2_snd.acc = 0; ss2_snd.step = 0; ss2_snd.last = 0xFF;
    for (i = 0; i < 6; i++) clear_chan(&ss2_snd.ch[i]);
    restore_music();
}

static void stop_sfx(void)                  /* $812C ($DA as currently shifted) */
{
    u8 i;
    ss2_snd.mask ^= 0x3F;
    restore_music();
    ss2_snd.pri = ss2_snd.mask = ss2_snd.wait = ss2_snd.loop = 0;
    for (i = 0; i < 6; i++)
    {
        Ss2SndSlot *q = &ss2_snd.sl[i];
        q->per = 0; q->slide = q->vol = q->vslide = q->mode = 0;
    }
}

/* ---- request ($8081 / $808A) ------------------------------------------- */
void ss2_snd_seq_request(u8 a)
{
    u8 n, ci, pri;
    u16 hdr;
    ss2_snd.busy = 0xFF;
    if (a >= 0xF0)
    {                                       /* $811B: F0 all, F1 sfx, F2/F3 music */
        switch (a & 3)
        {
            case 0: stop_music(); stop_sfx(); break;
            case 1: stop_sfx(); break;
            default: stop_music(); break;
        }
        ss2_snd.busy = 0; return;
    }
    n = (u8)(rd(0x89B4) + 1);               /* 43 entries; id reduced mod 43 */
    while (a >= n) a = (u8)(a - n);
    hdr = (u16)(rd((u16)(0x89B7 + 2 * a)) | (rd((u16)(0x89B8 + 2 * a)) << 8));
    pri = rd(hdr);
    if (pri == 0)
    {                                       /* music: 6 channel pointers */
        stop_music();
        for (ci = 0; ci < 6; ci++)
        {
            Ss2SndChan *c = &ss2_snd.ch[ci];
            u8 lo = rd((u16)(hdr + 1 + 2 * ci)), hi = rd((u16)(hdr + 2 + 2 * ci));
            c->ptr = (lo | hi) ? addr_ptr(lo, hi) : 0;
            c->ins = 0; set_inst(c);
            c->bal = 0xAA;
        }
    }
    else if (pri >= ss2_snd.pri)
    {                                       /* sfx: priority, channel mask, stream */
        u8 mask = rd((u16)(hdr + 1)), y, cb;
        ss2_snd.mask = (u8)((mask ^ ss2_snd.mask) & ss2_snd.mask);
        stop_sfx();
        ss2_snd.pri = pri;
        ss2_snd.mask = mask; cb = mask;
        for (y = 6; y; y--)
        {
            u8 cbit = cb & 1;
            cb >>= 1;
            if (cbit) { psg(0, (u8)(y - 1)); psg(5, 0xAA); }
        }
        ss2_snd.sp = BANK((u16)(hdr + 2));
    }
    ss2_snd.busy = 0;
}

/* ---- music channel tick ------------------------------------------------ */
static void lfo_reset(Ss2SndChan *c)        /* $83B0 */
{
    c->lfoc = 0; c->vib = 0; c->trem = 0xFF;
    c->vibd = ins(c, 0x25);
    c->tremd = ins(c, 0x26);
    c->lfo_on = c->vibd | c->tremd;
}

/* $83D2 length: base 3 (2 in triplet mode) doubled code-1 times (8-bit;
 * code 0 = 255 doublings = 0), plus the previous value when dotted */
static const u8 len_tab[2][8] = {
    { 0, 3, 6, 12, 24, 48, 96, 192 },
    { 0, 2, 4, 8, 16, 32, 64, 128 },
};

static void note(Ss2SndChan *c, u8 a)       /* $8321 */
{
    u8 cc = c->flags, x = (u8)(a >> 5), acc, n;
    const u8 *t = len_tab[(cc >> 3) & 1];
    HOST_EVENT('m', c - ss2_snd.ch, a);
    acc = t[x];
    if (cc & 1)
    {
        acc = (u8)(acc + (x > 1 ? t[x - 1] : 0));
        cc &= (u8)~1; c->flags = cc;
    }
    c->len = acc;
    {
        u8 g = (u8)(((u16)c->gratio * acc) >> 8);
        c->gate = g ? g : 1;
    }
    n = a & 0x1F;
    if (!n) { c->gate = 0; return; }        /* rest */
    c->flags = (cc & 2) ? (u8)(cc | 4) : (u8)(cc & ~4);   /* tie flag for the next note */
    if (!(cc & 4))
    {                                       /* not tied from previous note */
        n--;
        if (cc & 0x80) c->per = (u16)(n << 2);              /* noise frequency */
        else
        {
            u8 o = c->oct;
            if (o & 0x80) o = (u8)((o & 0x7F) + 0x18);
            o = (u8)(o + c->trn + n);
            c->per = (u16)((u16)(ss2_snd_period[(u8)(o << 1) >> 1] + (u16)(s16)(s8)c->det) << 2);
        }
        c->env = 0; c->envacc = 0;
    }
    cc = c->flags;                          /* $8398 */
    if (cc & 0x10) { c->flags = (u8)(cc & ~0x10); lfo_reset(c); }
    else if (c->lfor && !(cc & 4)) lfo_reset(c);
}

/* returns 1 when the channel ended ($855B pops the dispatcher frame) */
static u8 command(Ss2SndChan *c, u8 a, u8 mute)   /* $841B */
{
    u8 v;
    switch (a)
    {
        case 0x00:                          /* $8446 tempo: $3000 / T */
            v = next(c);
            if (!v) { ss2_snd_seq_faults++; ss2_snd.step = 0; }   /* source loops forever */
            else ss2_snd.step = (u16)(0x3000u / v);
            ss2_snd.acc = 0; ss2_snd.last = 0;
            break;
        case 0x01: c->gratio = next(c); break;               /* $8465 */
        case 0x02:                                            /* $846D */
            v = next(c);
            c->voloff = (u8)((((v & 0x1F) ^ 0x1F) ^ 0xFF) + 1);
            break;
        case 0x03:                                            /* $847C instrument */
            c->ins = next(c);
            set_inst(c);
            c->flags |= 0x10;
            load_wave(c, mute);
            break;
        case 0x04: c->flags ^= 0x08; break;
        case 0x05: c->flags ^= 0x02; break;
        case 0x06: c->flags |= 0x01; break;
        case 0x07: v = next(c); c->oct = (u8)((c->oct & 0x80) | v); break;
        case 0x08: c->oct |= 0x80; break;
        case 0x09: c->oct &= 0x7F; break;
        case 0x0A: c->trn = next(c); break;
        case 0x0B: c->det = next(c); break;
        case 0x0C: c->sld = next(c); break;
        case 0x0D:                                            /* $851A repeat */
            v = next(c);
            if (v)
            {
                if (!c->rep) c->rep = v;
                else if (!--c->rep) { c->ptr += 2; break; }
            }
            goto jump;
        case 0x0E:                                            /* $8532 break on last */
            if (c->rep == 1) { c->rep = 0; goto jump; }
            c->ptr += 2;
            break;
        case 0x0F:                                            /* $853D jump */
        jump:
        {
            u8 lo = next(c), hi = next(c);
            c->ptr = addr_ptr(lo, hi);
            break;
        }
        case 0x10:                                            /* $855B end */
            clear_chan(c);
            if (!mute) psg_silence();
            return 1;
        case 0x11:                                            /* $856E noise toggle */
            c->flags ^= 0x80;
            if (!(c->flags & 0x80)) psg_m(7, 0x00);
            break;
        case 0x12:                                            /* $8580 balance */
            v = next(c);
            c->bal = v;
            psg_m(5, v);
            break;
        default:                                              /* $13-$1E undefined */
            ss2_snd_seq_faults++;
            clear_chan(c);
            return 1;
    }
    return 0;
}

/* $8606 rate step: while the accumulator is below $10 add the rate and move
 * the level one step (+8 saturating at $F8|low bits, or -8 dropping to
 * low bits & 3 on underflow); then take $10 off.  The accumulator walk is
 * done exactly (8-bit); the k level steps are applied in closed form. */
static inline __attribute__((always_inline)) void emit(const Ss2SndChan *c)
{                                           /* $866E register writes */
    u16 v = c->o_per;
    if (c->flags & 0x80) { psg(7, (u8)((u8)v | 0x80)); psg(4, c->o_vol); }
    else { psg(2, (u8)v); psg(3, (u8)(v >> 8)); psg(4, c->o_vol); }
}

static inline u8 env_steps(Ss2SndChan *c, u8 rate)
{
    u8 acc = c->envacc, k;
    if (acc >= 0x10) { c->envacc = (u8)(acc - 0x10); return 0; }
    if (rate >= 0x10)
    {                                       /* one step (rate <= $EF: no wrap) */
        acc = (u8)(acc + rate); k = 1;
        if (acc < 0x10) { do { acc = (u8)(acc + rate); k++; } while (acc < 0x10); }
    }
    else if (rate == 1) { k = (u8)(0x10 - acc); acc = 0x10; }
    else
    {
        k = 0;
        do { acc = (u8)(acc + rate); k++; } while (acc < 0x10);
    }
    c->envacc = (u8)(acc - 0x10);
    return k;
}

static inline void envelope(Ss2SndChan *c, u8 m)   /* $85AB, $863E, $866E */
{
    u8 e = c->env, a, k, dirty;
    const u8 e0 = e;
    STAT(ss2_stat_env);
    a = e & 3;
    if (a == 2) goto out;                   /* sustain */
    if (a == 3)
    {                                       /* release */
        if (!c->rr) goto out;
        k = env_steps(c, c->rr);
        e = (k <= (e >> 3)) ? (u8)(e - (k << 3)) : (u8)(e & 3);
    }
    else if (a == 0)
    {                                       /* attack to level $F8 */
        if ((e & 0xF8) == 0xF8 || !c->ar) e++;
        else
        {
            k = env_steps(c, c->ar);
            a = (u8)((e >> 3) + k);
            if (a > 31 || a < k) a = 31;
            e = (u8)((a << 3) | (e & 7));
        }
    }
    else
    {                                       /* decay to sustain level */
        if (!c->dr) e++;
        else
        {
            k = env_steps(c, c->dr);
            e = (k <= (e >> 3)) ? (u8)(e - (k << 3)) : (u8)(e & 3);
            if ((e & 0xF8) < c->sl) e = (u8)(((e & 3) | c->sl) + 1);
        }
    }
    c->env = e;
out:
    /* $866E output.  Each cached part is recomputed only when one of its
     * inputs can have changed (fetch, LFO, slide, envelope level);
     * otherwise the source would recompute the identical value. */
    dirty = c->recalc;
    if (dirty)
    {
        if (c->vibd) c->vib = (u16)(c->vib + (u16)(s16)(s8)c->vibd);
        if (c->sld) c->per = (u16)(c->per + (u16)(s16)(s8)c->sld);
        c->o_per = (u16)((u16)(c->vib + c->per) >> 2);
        if (c->tremd)
        {
            s16 t = (s16)(c->trem + (s8)c->tremd);
            c->trem = t < 0 ? 0 : t > 255 ? 255 : (u8)t;
            dirty = 1;
        }
        c->recalc = (c->lfo_on | c->sld) ? 2 : 0;
    }
    if ((dirty & 1) || e != e0)
    {
        u8 cd = (u8)(e >> 3);
        a = (u8)(c->trem >> 3);
        if (!a) a = 1;
        if (a >= cd) a = cd;
        a = (u8)(a + c->voloff);
        if (a & 0x80) a = 0;
        c->o_vol = (u8)(a | 0x80);
    }
    if (!m)
    {
        emit(c);
    }
    /* $8640 LFO (dead state while both deltas are zero, see lfo_on) */
    if (c->lfo_on)
    {
        a = (u8)(c->lfoc + 1);
        if ((a & 0x7F) != c->lfop) { c->lfoc = a; return; }
        a = (u8)((a & 0x80) ^ 0x80);
        c->lfoc = a;
        if (a & 0x80) c->vibd = (u8)-c->vibd;
        c->tremd = (u8)-c->tremd;
    }
}

static inline void channel_tick(Ss2SndChan *c, u8 m)   /* $82DF */
{
    u8 a = c->len;
    if (a > 1)
    {
        u8 g = c->gate;
        c->len = (u8)(a - 1);
        if (g) c->gate = --g;
        if (!g && !(c->flags & 4)) c->env |= 3;   /* gate expired: release */
        else if ((c->env & 3) == 2 && !c->recalc)
        {                                   /* sustain, nothing changed: the
                                               source rewrites the same values */
            if (!m) emit(c);
            return;
        }
        envelope(c, m);
        return;
    }
    STAT(ss2_stat_fetch);
    c->recalc = 1;
    for (;;)                                /* $8317 fetch */
    {
        a = next(c);
        if (a >= 0x1F) break;
        if (command(c, a, m)) return;
    }
    note(c, a);
}

static void music_tick(void)                /* $8288 */
{
    ss2_snd.acc = (u16)(ss2_snd.acc + ss2_snd.step);
    while ((u8)(ss2_snd.acc >> 8) != ss2_snd.last)
    {
        u8 m = (u8)(ss2_snd.mask << 2), i;  /* $82A1 */
        Ss2SndChan *c = ss2_snd.ch;
        ss2_snd.last++;
        STAT(ss2_stat_ticks);
        if (ss2_snd.hold)
        {                                   /* $82DF with $6B set */
            for (i = 0; i < 6; i++, c++, m = (u8)(m << 1))
            {
                psg(0, i);
                if (c->ptr && !(m & 0x80)) psg(4, 0x80);
            }
            continue;
        }
        for (i = 0; i < 6; i++, c++, m = (u8)(m << 1))
        {
            psg(0, i);
            if (c->ptr)
            {
                STAT(ss2_stat_chan);
                channel_tick(c, m & 0x80);
            }
        }
    }
}

/* ---- sfx ($8718) -------------------------------------------------------- */
static inline u8 sat_add(u8 a, u8 d)        /* signed delta, clamped 0..FF */
{
    u16 t = (u16)a + d;
    if (d & 0x80) return (t >> 8) ? (u8)t : 0;
    return (t >> 8) ? 0xFF : (u8)t;
}

static void sfx_volume_out(Ss2SndSlot *q) { psg(4, (u8)((q->vol >> 3) | 0x80)); }   /* $88E8 */

static void sfx_period_out(Ss2SndSlot *q, u16 v)
{
    if (q->mode & 0x80) psg(7, (u8)((u8)v | 0x80));
    else { psg(2, (u8)v); psg(3, (u8)(v >> 8)); }
}

/* returns 1 when the effect ended (stream command 2/3) */
static u8 sfx_channel(Ss2SndSlot *q)        /* $8753 */
{
    u8 a;
    if (!ss2_snd.parsed && ss2_snd.wait)
    {                                       /* between events: slides */
        if (q->slide)
        {
            q->per = (u16)(q->per + (u16)(s16)(s8)q->slide);
            sfx_period_out(q, (u16)(q->per >> 2));
        }
        if (!q->vslide) return 0;
        q->vol = sat_add(q->vol, q->vslide);
        sfx_volume_out(q);
        return 0;
    }
    for (;;)                                /* $87BC */
    {
        a = snext();
        if (a & 0x80) break;
        switch (a)
        {
            case 0: ss2_snd.wait = snext(); ss2_snd.parsed = 0xFF; break;
            case 1:
            {
                u8 x = snext();
                if (x)
                {
                    if (!ss2_snd.loop) ss2_snd.loop = x;
                    else if (!--ss2_snd.loop) { ss2_snd.sp += 2; break; }
                }
                {
                    u8 lo = snext(), hi = snext();
                    ss2_snd.sp = addr_ptr(lo, hi);
                }
                break;
            }
            case 2: case 3: return 1;
            case 4: psg(5, snext()); break;
            default: ss2_snd_seq_faults++; return 1;
        }
    }
    HOST_EVENT('s', 5 - (q - ss2_snd.sl), a);   /* $881E */
    if (a & 1) q->mode |= 0x80;
    else { q->mode &= 0x7F; psg(7, 0x00); }
    {
        u8 b1 = snext(), b2 = snext(), w, b;
        if (b1 != 0xFF)
        {
            u16 v;
            if (q->mode & 0x80) { v = b1; psg(7, (u8)(b1 | 0x80)); }
            else
            {
                v = (u16)(ss2_snd_period[b1 & 0x7F] + (u16)(s16)(s8)b2);
                psg(2, (u8)v); psg(3, (u8)(v >> 8));
            }
            q->per = (u16)(v << 2);
        }
        q->slide = snext();
        w = snext();
        b = snext();
        if (b != 0xFF) q->vol = (u8)((b & 0x1F) << 3);
        q->vslide = snext();
        a = q->mode;
        if (!(a & 0x80))
        {
            w = (u8)(w + 1);
            if (a != w)
            {
                u8 y = (u8)((u8)(w - 1) << 1);
                u16 src = (u16)(rd((u16)(0x890A + y)) | (rd((u16)(0x890B + y)) << 8));
                q->mode = w;
                psg(4, 0x40); psg(4, 0x00);
                ss2_snd_psg_wave(src);
#ifdef SS2_SND_HOST
                for (y = 0; y < 32; y++) psg(6, rd((u16)(src + y)));
#endif
            }
        }
        sfx_volume_out(q);
    }
    return 0;
}

static void sfx_tick(void)                  /* $8718 */
{
    u8 save, cf;
    Ss2SndSlot *q = ss2_snd.sl;
    if (!ss2_snd.mask) return;
    save = ss2_snd.mask;
    for (cf = 6; cf; cf--)
    {
        psg(0, (u8)(cf - 1));
        if (ss2_snd.mask & 1)
        {
            if (sfx_channel(q)) { stop_sfx(); return; }   /* $880E: $DA still shifted */
        }
        ss2_snd.mask >>= 1;
        q++;
    }
    ss2_snd.mask = save;
    ss2_snd.wait--;
    ss2_snd.parsed = 0;
}

/* ---- public ------------------------------------------------------------- */
#ifdef SS2_PROF
void ss2_prof_mark(u8 sec);
#define PROF(n) ss2_prof_mark(n)
#else
#define PROF(n)
#endif

void ss2_snd_seq_tick(void)                 /* $8060 */
{
    if (ss2_snd.busy) return;
    sfx_tick();
    PROF(4);
    music_tick();
}

void ss2_snd_seq_reset(void)
{
    u16 i;
    u8 *b = (u8 *)&ss2_snd;
    for (i = 0; i < sizeof(ss2_snd); i++) b[i] = 0;
    for (i = 0; i < 6; i++) clear_chan(&ss2_snd.ch[i]);
    ss2_snd.sp = 0;
    for (i = 0; i < 16; i++) ss2_snd.ring[i] = 0xFF;
    ss2_snd_seq_faults = 0;
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

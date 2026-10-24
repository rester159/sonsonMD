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
            u32 tot = 1845281UL / period;             /* fnum * 2^block          */
            u8  block = 0;
            while (tot >= 2048 && block < 7) { tot >>= 1; block++; }
            if (tot >= 2048) tot = 2047;
            YM2612_writeReg(1, 0xA4 + c, (u8)((block << 3) | (tot >> 8)));
            YM2612_writeReg(1, 0xA0 + c, (u8)tot);
        }
        YM2612_writeReg(1, 0x4C + c, (u8)((15 - v) * 2 + ONEUP_TL_BIAS)); /* OP4 TL */
        if (!ym_keyed2[c]) { YM2612_writeReg(0, 0x28, 0xF0 | 0x04 | c); ym_keyed2[c] = 1; }
    }
}


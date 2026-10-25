/* Host render test: runs src/ss2_sound.c (virtual PSG -> YM2612/SN76489
 * mapping) over the native sequencer with stubbed chip writes, and checks
 * the YM state against the PCE register state every frame.
 *
 * usage: renderhost ID FRAMES  -> summary line:
 *   frames, fm_on_frames, max_cents_error, keyons, psg_noise_frames,
 *   mismatched_on_frames (PCE tone audible but FM keyed off or vice versa),
 *   ym_writes_per_frame_max, bus_unbalanced
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "ss2_sound.h"
#include "ss2_sound_seq.h"

static u8 ymreg[2][256], keyed[6];
static unsigned keyons, writes, wmax, bus_depth, bus_bad, noise_vol = 15;
static u8 pce_sel, pce_ctrl[6], pce_bal[6] = {0}, pce_noise[6], pce_main;
static u16 pce_per[6];

void YM2612_writeReg(const u16 part, const u8 reg, const u8 data)
{
    if (!bus_depth) bus_bad++;
    ymreg[part][reg] = data;
    writes++;
    if (reg == 0x28)
    {
        u8 ch = (u8)((data & 3) + ((data & 4) ? 3 : 0));
        if ((data & 0xF0) && !keyed[ch]) keyons++;
        keyed[ch] = (data & 0xF0) != 0;
    }
}
void PSG_setEnvelope(u8 channel, u8 value) { if (channel == 3) noise_vol = value; }
void PSG_setTone(u8 channel, u16 value) { (void)channel; (void)value; }
void PSG_setNoise(u8 type, u8 frequency) { (void)type; (void)frequency; }
void Z80_loadDriver(const u16 driver, const bool waitReady) { (void)driver; (void)waitReady; }
void Z80_requestBus(bool wait) { (void)wait; bus_depth = 1; }
bool Z80_getAndRequestBus(bool wait) { (void)wait; bool t = bus_depth != 0; bus_depth++; return t; }
void Z80_releaseBus(void) { bus_depth = 0; }
u16 SYS_isPAL(void) { return 0; }

void ss2_snd_psg_write_impl(u8 reg, u8 val);
void ss2_snd_host_event(char kind, u8 ch, u8 byte) { (void)kind; (void)ch; (void)byte; }

/* shadow of the PCE registers, fed by wrapping the sequencer hook */
void ss2_snd_psg_write(u8 reg, u8 val)
{
    ss2_snd_psg_write_impl(reg, val);
    if (reg == 0) pce_sel = val & 7;
    else if (reg == 1) pce_main = val;
    else if (pce_sel < 6)
    {
        if (reg == 2) pce_per[pce_sel] = (u16)((pce_per[pce_sel] & 0xF00) | val);
        if (reg == 3) pce_per[pce_sel] = (u16)((pce_per[pce_sel] & 0xFF) | ((val & 15) << 8));
        if (reg == 4) pce_ctrl[pce_sel] = val;
        if (reg == 5) pce_bal[pce_sel] = val;
        if (reg == 7) pce_noise[pce_sel] = val;
    }
}

int main(int argc, char **argv)
{
    int id = argc > 1 ? (int)strtol(argv[1], 0, 16) : 3;
    int frames = argc > 2 ? atoi(argv[2]) : 600, f;
    unsigned on_frames = 0, mism = 0, noise_frames = 0;
    double maxc = 0;
    ss2_sound_init();
    bus_depth = 1;
    ss2_sound_request((u8)id);
    for (f = 0; f < frames; f++)
    {
        int ch;
        unsigned w0 = writes;
        ss2_sound_tick();
        if (writes - w0 > wmax) wmax = writes - w0;
        for (ch = 0; ch < 6; ch++)
        {
            u8 vol = pce_ctrl[ch] & 31;
            int tone = (pce_ctrl[ch] & 0x80) && !(pce_ctrl[ch] & 0x40) && vol &&
                       !(ch >= 4 && (pce_noise[ch] & 0x80)) && pce_per[ch] &&
                       (pce_bal[ch] & 0xFF) && (pce_main & 0xFF);
            if (tone != keyed[ch]) { mism++; continue; }
            if (!tone) continue;
            on_frames++;
            {
                u8 p = ch >= 3, c = (u8)(ch % 3);
                unsigned hi = ymreg[p][0xA4 + c], lo = ymreg[p][0xA0 + c];
                unsigned blk = (hi >> 3) & 7, fn = ((hi & 7) << 8) | lo;
                double ym = fn * (7670453.0 / 144.0) * pow(2, blk) / 2097152.0;
                double pce = 3579545.0 / (32.0 * pce_per[ch]);
                double cents = fabs(1200.0 * log2(ym / pce));
                if (pce_per[ch] >= 17 && cents > maxc) maxc = cents;
            }
        }
        if (noise_vol < 15) noise_frames++;
    }
    ss2_sound_shutdown();
    printf("id %02X frames %d fm_on %u max_cents %.2f keyons %u noise_frames %u mismatch %u ym_writes_max %u bus_unheld %u faults %u\n",
           id, frames, on_frames, maxc, keyons, noise_frames, mism, wmax, bus_bad, ss2_snd_seq_faults);
    return 0;
}

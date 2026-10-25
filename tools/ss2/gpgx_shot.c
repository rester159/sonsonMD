/* Headless Genesis Plus GX (libretro core) capture harness.
 *
 * usage: gpgx_shot CORE ROM FRAMES OUTDIR SCRIPT SHOTS
 *   SCRIPT  "a-b:KEYS;a-b:KEYS"  keys U D L R S(start) a b c   (pad 1)
 *   SHOTS   "f1,f2,..."          frames written as OUTDIR/frame-N.ppm
 * No window and no frontend: video is only copied for the requested frames.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
typedef unsigned char u8;

struct Game { const char *path; const void *data; size_t size; const char *meta; };
struct Variable { const char *key, *value; };

static unsigned pixel_format, frame;
/* SS2_AUTOPLAY_LOOP: the play pattern indexed by the game's own loop
 * counter (pass about to read the pad), so builds with different timing
 * see the same input on the same logic frame */
static long apl_start = -1;
static unsigned apl_t;
static int apl_on;
static const char *outdir, *script;
static unsigned shots[256], nshots;

static bool environment(unsigned cmd, void *p)
{
    if (cmd == 10) { pixel_format = *(unsigned *)p; return true; }
    if (cmd == 9 || cmd == 31) { *(const char **)p = "/tmp"; return true; }
    if (cmd == 17) { *(bool *)p = false; return true; }
    if (cmd == 15) { ((struct Variable *)p)->value = NULL; return false; }
    if (cmd == 52) { *(unsigned *)p = 0; return true; }
    return cmd == 16 || cmd == 11 || cmd == 35;
}

static void video(const void *d, unsigned w, unsigned h, size_t pitch)
{
    unsigned i, x, y;
    char path[2048];
    FILE *f;
    if (!d) return;
    for (i = 0; i < nshots && shots[i] != frame; i++) {}
    if (i == nshots) return;
    snprintf(path, sizeof path, "%s/frame-%u.ppm", outdir, frame);
    f = fopen(path, "wb");
    if (!f) exit(4);
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
        {
            unsigned v = pixel_format == 1 ? ((const uint32_t *)((const uint8_t *)d + y * pitch))[x]
                                           : ((const uint16_t *)((const uint8_t *)d + y * pitch))[x];
            unsigned char rgb[3];
            if (pixel_format == 1) { rgb[0] = v >> 16; rgb[1] = v >> 8; rgb[2] = v; }
            else if (pixel_format == 2) { rgb[0] = ((v >> 11) & 31) * 255 / 31; rgb[1] = ((v >> 5) & 63) * 255 / 63; rgb[2] = (v & 31) * 255 / 31; }
            else { rgb[0] = ((v >> 10) & 31) * 255 / 31; rgb[1] = ((v >> 5) & 31) * 255 / 31; rgb[2] = (v & 31) * 255 / 31; }
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
}

/* optional raw audio dump (env SS2_PCM = path): interleaved s16 stereo */
static FILE *pcm;
static unsigned pcm_from;
static void audio(int16_t l, int16_t r)
{
    if (pcm && frame >= pcm_from) { int16_t s[2] = { l, r }; fwrite(s, 2, 2, pcm); }
}
static size_t batch(const int16_t *d, size_t n)
{
    if (pcm && frame >= pcm_from) fwrite(d, 4, n, pcm);
    return n;
}
static void poll(void) {}

/* libretro joypad ids: B=0 Y=1 SELECT=2 START=3 UP=4 DOWN=5 LEFT=6 RIGHT=7 A=8 */
static int key_id(char k)
{
    switch (k)
    {
        case 'U': return 4; case 'D': return 5; case 'L': return 6; case 'R': return 7;
        case 'S': return 3; case 'a': return 1; case 'b': return 0; case 'c': return 8;
    }
    return -1;
}

static int16_t input(unsigned port, unsigned device, unsigned index, unsigned id)
{
    const char *p = script;
    static long autoplay = -2;
    (void)device; (void)index;
    if (port || !p) return 0;
    /* env SS2_AUTOPLAY = first frame of a fixed play pattern after the
     * script: walk right, attack (II) every 12 frames, jump (I) every 90 */
    if (autoplay == -2) autoplay = getenv("SS2_AUTOPLAY") ? atol(getenv("SS2_AUTOPLAY")) : -1;
    if (apl_on || (autoplay >= 0 && frame >= (unsigned)autoplay))
    {
        unsigned t = apl_on ? apl_t : frame - (unsigned)autoplay;
        if (id == 7) return 1;
        if (id == 1) return (t % 12) < 3;
        if (id == 0) return (t % 90) >= 30 && (t % 90) < 39;
        return 0;
    }
    while (*p)
    {
        unsigned a = 0, b = 0;
        a = (unsigned)strtoul(p, (char **)&p, 10);
        b = a;
        if (*p == '-') b = (unsigned)strtoul(p + 1, (char **)&p, 10);
        if (*p == ':') p++;
        while (*p && *p != ';')
        {
            if (frame >= a && frame <= b && key_id(*p) == (int)id) return 1;
            p++;
        }
        if (*p == ';') p++;
    }
    return 0;
}

#define FN(ret, n, args) ret(*n) args = (ret(*) args)dlsym(core, #n); if (!n) return 2;

int main(int argc, char **argv)
{
    unsigned frames;
    char *s;
    if (argc != 7) return 2;
    void *core = dlopen(argv[1], RTLD_NOW);
    if (!core) return 2;
    frames = (unsigned)atoi(argv[3]);
    outdir = argv[4];
    script = argv[5];
    for (s = argv[6]; *s && nshots < 256;)
    {
        shots[nshots++] = (unsigned)strtoul(s, &s, 10);
        if (*s == ',') s++;
    }
    FN(void, retro_set_environment, (bool (*)(unsigned, void *)));
    FN(void, retro_set_video_refresh, (void (*)(const void *, unsigned, unsigned, size_t)));
    FN(void, retro_set_audio_sample, (void (*)(int16_t, int16_t)));
    FN(void, retro_set_audio_sample_batch, (size_t (*)(const int16_t *, size_t)));
    FN(void, retro_set_input_poll, (void (*)(void)));
    FN(void, retro_set_input_state, (int16_t (*)(unsigned, unsigned, unsigned, unsigned)));
    FN(void, retro_init, (void));
    FN(bool, retro_load_game, (const struct Game *));
    FN(void, retro_run, (void));
    FN(void, retro_unload_game, (void));
    FN(void *, retro_get_memory_data, (unsigned));
    FN(void, retro_deinit, (void));
    retro_set_environment(environment);
    retro_set_video_refresh(video);
    retro_set_audio_sample(audio);
    retro_set_audio_sample_batch(batch);
    retro_set_input_poll(poll);
    retro_set_input_state(input);
    retro_init();
    if (getenv("SS2_PCM")) { pcm = fopen(getenv("SS2_PCM"), "wb"); pcm_from = getenv("SS2_PCM_FROM") ? (unsigned)atoi(getenv("SS2_PCM_FROM")) : 0; }
    FILE *f = fopen(argv[2], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    void *data = malloc((size_t)n);
    if (!data || fread(data, 1, (size_t)n, f) != (size_t)n) return 2;
    fclose(f);
    struct Game g = { argv[2], data, (size_t)n, NULL };
    if (!retro_load_game(&g)) return 3;
    FN(size_t, retro_get_memory_size, (unsigned));
    if (getenv("SS2_SRAM"))
    {
        /* cartridge SRAM (RETRO_MEMORY_SAVE_RAM): loaded before the first
         * frame, written back at the end, like a frontend's .srm */
        FILE *sf = fopen(getenv("SS2_SRAM"), "rb");
        u8 *sram = (u8 *)retro_get_memory_data(0);
        if (sf && sram) { fread(sram, 1, retro_get_memory_size(0), sf); }
        if (sf) fclose(sf);
    }
    /* optional per-refresh trace (env SS2_TRACE = path): the frame-loop
     * counter (SS2_TRACE_LOOP, u16 address from out/symbol.txt) and ss2_ram
     * (SS2_TRACE_RAM): a refresh where the loop counter did not move is a
     * presentation the game missed */
    FILE *tf = getenv("SS2_TRACE") ? fopen(getenv("SS2_TRACE"), "w") : NULL;
    unsigned loop_a = getenv("SS2_TRACE_LOOP") ? (unsigned)strtoul(getenv("SS2_TRACE_LOOP"), NULL, 16) & 0xFFFF : 0;
    unsigned ram_a = getenv("SS2_TRACE_RAM") ? (unsigned)strtoul(getenv("SS2_TRACE_RAM"), NULL, 16) & 0xFFFF : 0;
    unsigned pres_a = getenv("SS2_TRACE_PRES") ? (unsigned)strtoul(getenv("SS2_TRACE_PRES"), NULL, 16) & 0xFFFF : 0;
    unsigned count_a = getenv("SS2_TRACE_COUNT") ? (unsigned)strtoul(getenv("SS2_TRACE_COUNT"), NULL, 16) & 0xFFFF : 0;
    unsigned prof_a = getenv("SS2_TRACE_PROF") ? (unsigned)strtoul(getenv("SS2_TRACE_PROF"), NULL, 16) & 0xFFFF : 0;
    const u8 *wram = (const u8 *)retro_get_memory_data(2);
    FILE *hf_ram = getenv("SS2_RAMHASH") ? fopen(getenv("SS2_RAMHASH"), "w") : NULL;
    if (tf && pres_a && !prof_a) fputs("frame,loop,zframe,camx,stage,pres\n", tf);
    else if (tf) fputs(prof_a ? "frame,loop,zframe,camx,stage,s0,s1,s2,s3,s4,s5,s6,s7,s8,s9,s10,s11,s12,s13,s14,s15,s16,s17,s18,s19,s20,s21,s22,total,c0,c1,c2,c3\n"
                         : "frame,loop,zframe,camx,stage\n", tf);
#define WB(a) (wram[((a) & 0xFFFF) ^ 1])
    /* PC samples (SS2_TRACE_PCRING = ring,index addresses, SS2_PCOUT = file):
     * "frame pc" per sample drained from the ROM's ring each refresh */
    unsigned ring_a = 0, ridx_a = 0, rlast = 0;
    FILE *pf = NULL;
    if (getenv("SS2_TRACE_PCRING") && getenv("SS2_PCOUT"))
    {
        sscanf(getenv("SS2_TRACE_PCRING"), "%x,%x", &ring_a, &ridx_a);
        ring_a &= 0xFFFF; ridx_a &= 0xFFFF;
        pf = fopen(getenv("SS2_PCOUT"), "w");
    }
    if (getenv("SS2_AUTOPLAY_LOOP")) apl_start = atol(getenv("SS2_AUTOPLAY_LOOP"));
    unsigned hlast = 0xFFFFFFFF;
    for (frame = 0; frame < frames; frame++)
    {
        if (apl_start >= 0 && wram && loop_a)
        {
            /* from refresh apl_start on: t = passes since then */
            static unsigned l0; static int got;
            unsigned l = (WB(loop_a) << 8) | WB(loop_a + 1);
            if (getenv("SS2_AUTOPLAY_ABS"))
            {
                /* absolute: from loop pass N on, t = pass - N (the same
                 * input on the same game frame in every build) */
                static long n0 = -1;
                if (n0 < 0) n0 = atol(getenv("SS2_AUTOPLAY_ABS"));
                if ((long)(l + 1) >= n0 && frame >= 1000) { apl_on = 1; apl_t = (unsigned)((l + 1 - n0) & 0xFFFF); }
                (void)got; (void)l0;
            }
            else if (frame >= (unsigned)apl_start) { if (!got) { l0 = l; got = 1; } apl_on = 1; apl_t = (l + 1 - l0) & 0xFFFF; }
        }
        /* SS2_LIFE (profiling): refill life ($22B8) to its maximum ($22B9) */
        if (getenv("SS2_LIFE") && wram && ram_a)
            ((u8 *)wram)[((ram_a + 0x2B8) & 0xFFFF) ^ 1] = WB(ram_a + 0x2B9);
        retro_run();
        if (hf_ram && wram && loop_a)
        {
            /* SS2_RAMHASH: FNV-1a of ss2_ram (8 KiB) once per loop pass */
            unsigned l = (WB(loop_a) << 8) | WB(loop_a + 1), h = 2166136261u, k;
            if (l != hlast)
            {
                for (k = 0; k < 0x2000; k++) h = (h ^ WB(ram_a + k)) * 16777619u;
                fprintf(hf_ram, "%u %u %08x\n", frame, l, h);
                hlast = l;
            }
        }
        if (pf && wram)
        {
            unsigned ri = ((WB(ridx_a) << 8) | WB(ridx_a + 1)) & 0x1FC;
            while (rlast != ri)
            {
                unsigned q = ring_a + rlast;
                fprintf(pf, "%u %x\n", frame, (WB(q) << 24) | (WB(q + 1) << 16) | (WB(q + 2) << 8) | WB(q + 3));
                rlast = (rlast + 4) & 0x1FC;
            }
        }
        if (tf && wram)
            fprintf(tf, "%u,%u,%u,%u,%u\n", frame, (WB(loop_a) << 8) | WB(loop_a + 1),
                    WB(ram_a + 0x12), ((WB(ram_a + 0x45) & 7) << 8) | WB(ram_a + 0x46), WB(ram_a + 0x69));
        if (tf && wram && pres_a && !prof_a)
        {
            /* presented-tick counter (SS2_TRACE_PRES = ss2_present_n) */
            fseek(tf, -1, SEEK_CUR);
            fprintf(tf, ",%u\n", (WB(pres_a) << 8) | WB(pres_a + 1));
        }
        if (tf && wram && prof_a)
        {
            /* ss2_tp_last[24] (SS2_TPROF builds): the last finished pass */
            unsigned k;
            fseek(tf, -1, SEEK_CUR);
            for (k = 0; k < 24; k++) fprintf(tf, ",%u", (WB(prof_a + 2 * k) << 8) | WB(prof_a + 2 * k + 1));
            for (k = 0; count_a && k < 4; k++) fprintf(tf, ",%u", (WB(count_a + 2 * k) << 8) | WB(count_a + 2 * k + 1));
            fputc('\n', tf);
        }
    }
    if (tf) fclose(tf);
    if (pf) fclose(pf);
    if (hf_ram) fclose(hf_ram);
    if (getenv("SS2_SRAM"))
    {
        FILE *sf = fopen(getenv("SS2_SRAM"), "wb");
        u8 *sram = (u8 *)retro_get_memory_data(0);
        if (sf && sram) fwrite(sram, 1, retro_get_memory_size(0), sf);
        if (sf) fclose(sf);
    }
    if (getenv("SS2_RAMDUMP"))
    {
        /* 68000 work RAM (64 KiB, byte-swapped words in this core) */
        FILE *rf = fopen(getenv("SS2_RAMDUMP"), "wb");
        u8 *ram = (u8 *)retro_get_memory_data(2);
        if (rf && ram) { fwrite(ram, 1, 0x10000, rf); fclose(rf); }
    }
    if (pcm) fclose(pcm);
    retro_unload_game();
    retro_deinit();
    free(data);
    dlclose(core);
    return 0;
}

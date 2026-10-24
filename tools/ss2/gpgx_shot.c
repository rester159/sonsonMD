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
    (void)device; (void)index;
    if (port || !p) return 0;
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
    for (frame = 0; frame < frames; frame++) retro_run();
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

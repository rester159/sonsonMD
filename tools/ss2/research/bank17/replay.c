/* replay.c -- tools/ss2/hosttest/test_routine.c for whole-frame pairs, with
 * a histogram of the unported addresses reached and the first differing
 * frames. usage: replay PAIRS [maxreport] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ss2_ram.h"
#include "ss2_logic.h"

void ss2_frame_tick_then_vblank(void);
unsigned char ss2_expected_pad;
extern u8 ss2_unported_bank;
static unsigned char irq_mask[0x2000];
static int known;
static unsigned ign_lo = 1, ign_hi = 0;   /* SS2_IGNORE=LO-HI (hex, $2000-based) */

static int masked(unsigned a)
{
    if (irq_mask[a]) return 1;
    if (a >= 0x26 && a <= 0x3F) return 1;
    if (a >= 0x100 && a <= 0x1FF) return 1;
    /* SS2_KNOWN=1: $208E (bank last mapped at $C000) is left by the map
     * streamer ($E8D1, bank 6|7) on frames without live objects; the
     * native renderer replaces the streamer, so the byte is not modelled
     * (shared ss2_game.c). Counted separately. */
    if (known && a == 0x8E) return 1;
    if (a >= ign_lo && a <= ign_hi) return 1;
    return 0;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    char magic[4];
    unsigned n, i, bad = 0, unp = 0, maxrep = argc > 2 ? atoi(argv[2]) : 6;
    static unsigned hist[0x20][0x10000 / 1];
    static u8 pre[0x2000], post[0x2000];
    const char *mp = getenv("SS2_IRQ_MASK");
    FILE *m = fopen(mp, "rb");
    unsigned known_only = 0;
    known = getenv("SS2_KNOWN") != 0;
    if (getenv("SS2_IGNORE") && sscanf(getenv("SS2_IGNORE"), "%x-%x", &ign_lo, &ign_hi) == 2)
    {
        ign_lo -= 0x2000; ign_hi -= 0x2000;
        printf("  (ignoring $%04X-$%04X)\n", ign_lo + 0x2000, ign_hi + 0x2000);
    }
    if (!f || fread(magic, 1, 4, f) != 4 || fread(&n, 4, 1, f) != 1) return 2;
    if (!m || fread(irq_mask, 1, 0x2000, m) != 0x2000) return 2;
    for (i = 0; i < n; i++)
    {
        unsigned short fr;
        unsigned a, diffs = 0;
        if (fread(&fr, 2, 1, f) != 1 || fread(pre, 1, 0x2000, f) != 0x2000 || fread(post, 1, 0x2000, f) != 0x2000) return 3;
        memcpy(ss2_ram, pre, 0x2000);
        ss2_expected_pad = post[0x16];
        ss2_unported_count = 0;
        ss2_frame_tick_then_vblank();
        if (ss2_unported_count)
        {
            unp++;
            hist[ss2_unported_bank & 0x1F][ss2_unported_last]++;
            continue;
        }
        for (a = 0; a < 0x2000; a++)
            if (!masked(a) && ss2_ram[a] != post[a])
            {
                if (diffs < 16 && bad < maxrep)
                    printf("frame %u (#%u): $%04X pre %02X want %02X got %02X\n", fr, i, a + 0x2000, pre[a], post[a], ss2_ram[a]);
                diffs++;
            }
        if (!diffs && known && ss2_ram[0x8E] != post[0x8E]) known_only++;
        if (diffs) bad++;
    }
    for (unsigned a = 0; a < 0x20; a++)
    {
        unsigned b;
        for (b = 0; b < 0x10000; b++)
            if (hist[a][b]) printf("  unported $%04X (bank $%02X at $C000): %u frames\n", b, a, hist[a][b]);
    }
    if (known) printf("  ($208E only: %u frames)\n", known_only);
    printf("%u pairs: %u exact, %u differ, %u reached unported code\n", n, n - bad - unp, bad, unp);
    return bad ? 1 : 0;
}

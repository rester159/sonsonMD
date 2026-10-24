/* Replay research-trace RAM pairs through a native SS2 routine and diff.
 * usage: test_routine PAIRS [maxreport]  (routine chosen at compile time) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ss2_ram.h"
#include "ss2_logic.h"

#ifndef ROUTINE
#define ROUTINE ss2_player_update
#endif
void ROUTINE(void);
unsigned char ss2_expected_pad;

/* bytes written from interrupt context (tools/ss2/irq_mask.py): an IRQ can
 * land inside any bracketed routine */
static unsigned char irq_mask[0x2000];

/* zero-page scratch ($26-$3F) is not persistent state */
static int masked(unsigned a)
{
    if (irq_mask[a]) return 1;
    /* coroutine mode: the task table's resume data and the current-slot
     * byte are scheduler internals with a native representation */
    if (getenv("SS2_CONTINUOUS") && (a < 0x10 || a == 0x15)) return 1;
    if (a >= 0x26 && a <= 0x3F) return 1;
    if (a >= 0x100 && a <= 0x1FF) return 1;   /* hardware stack page */
    return 0;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    char magic[4];
    unsigned n, i, bad_frames = 0, unported_frames = 0, maxrep = argc > 2 ? atoi(argv[2]) : 8;
    static u8 pre[0x2000], post[0x2000];
    if (!f || fread(magic, 1, 4, f) != 4 || fread(&n, 4, 1, f) != 1) return 2;
    {
        const char *mp = getenv("SS2_IRQ_MASK");
        FILE *m = fopen(mp ? mp : "../../out/ss2/irq_mask.bin", "rb");
        if (!m || fread(irq_mask, 1, 0x2000, m) != 0x2000) { fprintf(stderr, "no IRQ mask (run tools/ss2/irq_mask.py)\n"); return 2; }
        fclose(m);
    }
    for (i = 0; i < n; i++)
    {
        unsigned short fr;
        unsigned a, diffs = 0;
        if (fread(&fr, 2, 1, f) != 1 || fread(pre, 1, 0x2000, f) != 0x2000 || fread(post, 1, 0x2000, f) != 0x2000) return 3;
        /* SS2_CONTINUOUS: load only the first frame, then carry native
         * state; interrupt-owned bytes (except the task table) are synced
         * from the trace after each comparison */
        if (!getenv("SS2_CONTINUOUS") || i == 0) memcpy(ss2_ram, pre, 0x2000);
        ss2_expected_pad = post[0x16];
        ss2_unported_count = 0;
        ROUTINE();
        if (ss2_unported_count)
        {
            extern u8 ss2_unported_bank;
            unported_frames++;
            if (unported_frames <= 4) printf("frame %u: unported $%04X (bank $%02X at $C000)\n", fr, ss2_unported_last, ss2_unported_bank);
            continue;
        }
        for (a = 0; a < 0x2000; a++)
            if (!masked(a) && ss2_ram[a] != post[a])
            {
                if (diffs < 12 && bad_frames < maxrep)
                    printf("frame %u: $%04X pre %02X want %02X got %02X\n", fr, a + 0x2000, pre[a], post[a], ss2_ram[a]);
                diffs++;
            }
        if (diffs) bad_frames++;
        if (getenv("SS2_CONTINUOUS"))
            for (a = 0x10; a < 0x2000; a++)
                if (irq_mask[a] || (a >= 0x100 && a <= 0x1FF)) ss2_ram[a] = post[a];
    }
    printf("%u pairs: %u exact, %u differ, %u reached unported code\n", n, n - bad_frames - unported_frames, bad_frames, unported_frames);
    return bad_frames ? 1 : 0;
}

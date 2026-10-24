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

/* zero-page scratch ($26-$3F) is not persistent state */
static int masked(unsigned a)
{
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
    for (i = 0; i < n; i++)
    {
        unsigned short fr;
        unsigned a, diffs = 0;
        if (fread(&fr, 2, 1, f) != 1 || fread(pre, 1, 0x2000, f) != 0x2000 || fread(post, 1, 0x2000, f) != 0x2000) return 3;
        memcpy(ss2_ram, pre, 0x2000);
        ss2_unported_count = 0;
        ROUTINE();
        if (ss2_unported_count) { unported_frames++; if (unported_frames <= 4) printf("frame %u: unported $%04X\n", fr, ss2_unported_last); continue; }
        for (a = 0; a < 0x2000; a++)
            if (!masked(a) && ss2_ram[a] != post[a])
            {
                if (diffs < 12 && bad_frames < maxrep)
                    printf("frame %u: $%04X pre %02X want %02X got %02X\n", fr, a + 0x2000, pre[a], post[a], ss2_ram[a]);
                diffs++;
            }
        if (diffs) bad_frames++;
    }
    printf("%u pairs: %u exact, %u differ, %u reached unported code\n", n, n - bad_frames - unported_frames, bad_frames, unported_frames);
    return bad_frames ? 1 : 0;
}

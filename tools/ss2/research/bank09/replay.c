/* replay.c PAIRS [maxreport] -- tools/ss2/hosttest/test_routine.c in the
 * per-frame mode (ss2_frame_tick_then_vblank) with coverage for banks
 * $09/$0F/$1F: frames where one of their objects is live (before or after),
 * per-handler counts, and the unported addresses (last per frame) by bank.
 * Build/run: tools/ss2/research/bank09/replay.sh PAIRS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ss2_ram.h"
#include "ss2_logic.h"

void ss2_frame_tick_then_vblank(void);
unsigned char ss2_expected_pad;
extern u8 ss2_unported_bank;
static unsigned char irq_mask[0x2000];

static int masked(unsigned a)
{
    if (irq_mask[a]) return 1;
    if (a >= 0x26 && a <= 0x3F) return 1;
    if (a >= 0x100 && a <= 0x1FF) return 1;
    return 0;
}

static int mine_bank(u8 b) { return b == 0x09 || b == 0x0F || b == 0x1F; }

struct hist { u8 bank; u16 addr; unsigned n; };
static struct hist uh[64], hh[64];
static unsigned nuh, nhh;
static unsigned dmine[0x2000], dother[0x2000];

static void add(struct hist *h, unsigned *n, u8 bank, u16 addr)
{
    unsigned i;
    for (i = 0; i < *n; i++) if (h[i].bank == bank && h[i].addr == addr) { h[i].n++; return; }
    if (*n < 64) { h[*n].bank = bank; h[*n].addr = addr; h[*n].n = 1; (*n)++; }
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    char magic[4];
    unsigned n, i, maxrep = argc > 2 ? atoi(argv[2]) : 8;
    unsigned bad = 0, unp = 0, mine = 0, mine_ok = 0, mine_bad = 0, mine_unp = 0, mine_unp_own = 0;
    static u8 pre[0x2000], post[0x2000];
    if (!f || fread(magic, 1, 4, f) != 4 || fread(&n, 4, 1, f) != 1) return 2;
    {
        FILE *m = fopen(getenv("SS2_IRQ_MASK"), "rb");
        if (!m || fread(irq_mask, 1, 0x2000, m) != 0x2000) { fprintf(stderr, "no IRQ mask\n"); return 2; }
        fclose(m);
    }
    {
        /* PAIRS.irq from capture_at.py: interrupt writes seen in this run */
        static unsigned char extra[0x2000];
        char path[1024];
        FILE *m;
        snprintf(path, sizeof path, "%s.irq", argv[1]);
        m = fopen(path, "rb");
        if (m && fread(extra, 1, 0x2000, m) == 0x2000)
            for (i = 0; i < 0x2000; i++) irq_mask[i] |= extra[i];
        if (m) fclose(m);
    }
    for (i = 0; i < n; i++)
    {
        unsigned short fr;
        unsigned a, diffs = 0, x, is_mine = 0;
        if (fread(&fr, 2, 1, f) != 1 || fread(pre, 1, 0x2000, f) != 0x2000 || fread(post, 1, 0x2000, f) != 0x2000) return 3;
        for (x = 0; x < 0x20; x++)
        {
            if (pre[0x45A + x] && mine_bank(pre[0x75A + x]))
            {
                is_mine = 1;
                add(hh, &nhh, pre[0x75A + x], (u16)(pre[0x45A + x] == 0x80 ? (pre[0x8BA + x] << 8) | pre[0x8DA + x]
                                                                             : (pre[0x8FA + x] << 8) | pre[0x91A + x]));
            }
            if (post[0x45A + x] && mine_bank(post[0x75A + x])) is_mine = 1;
        }
        memcpy(ss2_ram, pre, 0x2000);
        ss2_expected_pad = post[0x16];
        ss2_unported_count = 0;
        ss2_frame_tick_then_vblank();
        mine += is_mine;
        if (ss2_unported_count)
        {
            unp++;
            add(uh, &nuh, ss2_unported_bank, ss2_unported_last);
            if (is_mine)
            {
                mine_unp++;
                if (mine_bank(ss2_unported_bank) && ss2_unported_last >= 0xC000) mine_unp_own++;
            }
            continue;
        }
        for (a = 0; a < 0x2000; a++)
            if (!masked(a) && ss2_ram[a] != post[a])
            {
                if (diffs < 12 && bad < maxrep)
                    printf("frame %u: $%04X pre %02X want %02X got %02X\n", fr, a + 0x2000, pre[a], post[a], ss2_ram[a]);
                diffs++;
                if (is_mine) dmine[a]++; else dother[a]++;
            }
        if (diffs) { bad++; if (is_mine) mine_bad++; }
        else if (is_mine) mine_ok++;
    }
    printf("%u pairs: %u exact, %u differ, %u reached unported code\n", n, n - bad - unp, bad, unp);
    printf("frames with a bank 9/F/1F object: %u: %u exact, %u differ, %u unported (%u in these banks' code)\n",
           mine, mine_ok, mine_bad, mine_unp, mine_unp_own);
    for (i = 0; i < nhh; i++) printf("  handler %02X:%04X live in %u frames\n", hh[i].bank, hh[i].addr, hh[i].n);
    for (i = 0; i < nuh; i++) printf("  unported $%04X (bank $%02X at $C000): %u frames\n", uh[i].addr, uh[i].bank, uh[i].n);
    for (i = 0; i < 0x2000; i++)
        if (dmine[i] || dother[i])
            printf("  diff $%04X: %u frames with, %u without a bank 9/F/1F object\n", i + 0x2000, dmine[i], dother[i]);
    return bad ? 1 : 0;
}

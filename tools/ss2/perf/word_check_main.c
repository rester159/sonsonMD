/* Target oracle for split-byte reads: all 65,536 words, register and memory
 * operands, both unsigned and signed promotion, and untouched guard bytes. */
#include <genesis.h>
#include "ss2_ram.h"
volatile u32 word_checks, word_failures;
volatile u16 word_done;
static u8 bytes[8];
static __attribute__((noinline)) u16 memory_word(void)
{
    return ss2_word(bytes[1], bytes[6]);
}
int main(bool hardReset)
{
    u32 n;
    (void)hardReset;
    memset(bytes, 0xA5, sizeof bytes);
    for (n = 0; n < 65536; n++)
    {
        u8 hi = (u8)(n >> 8), lo = (u8)n;
        u16 a, b;
        bytes[1] = hi; bytes[6] = lo;
        a = ss2_word(hi, lo); b = memory_word();
        if ((u32)a != n || (u32)b != n || (s32)(s16)a != (n < 32768 ? (s32)n : (s32)n - 65536)) word_failures++;
        if (bytes[0] != 0xA5 || bytes[2] != 0xA5 || bytes[3] != 0xA5 || bytes[4] != 0xA5 || bytes[5] != 0xA5 || bytes[7] != 0xA5) word_failures++;
        word_checks++;
        if (!(n & 255)) SYS_doVBlankProcess();
    }
    word_done = 0x600D;
    while (TRUE) SYS_doVBlankProcess();
}

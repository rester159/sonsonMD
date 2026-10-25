/* Son Son II settings kept in cartridge SRAM (Genesis addition).
 *
 * SonSon's save (src/save.c) owns SRAM bytes $000-$3FF (two 512-byte
 * slots); Son Son II keeps its block at $400: "S2", version, the settings,
 * and a checksum byte (the sum of the bytes before it, inverted). A block
 * that does not check out gives the defaults. Written only when the OPTIONS
 * screen is left with a changed setting. */
#include <genesis.h>
#include "ss2_logic.h"

#define BLOCK   0x400
#define VERSION 1
#define NBYTES  5                       /* 'S' '2' version language check */

u8 ss2_language;                        /* SS2_LANG_ENGLISH / SS2_LANG_JAPANESE */

static u8 check(const u8 *b)
{
    u8 i, s = 0;
    for (i = 0; i < NBYTES - 1; i++) s = (u8)(s + b[i]);
    return (u8)~s;
}

/* the HuCard's text banks (1-2, pages 2-3) for a language */
void ss2_set_language(u8 lang)
{
    ss2_language = lang;
    ss2_banks[1] = lang == SS2_LANG_JAPANESE ? ss2_bank1_jp : ss2_bank1;
    ss2_banks[2] = lang == SS2_LANG_JAPANESE ? ss2_bank2_jp : ss2_bank2;
    ss2_page[2] = ss2_banks[1];
    ss2_page[3] = ss2_banks[2];
}

void ss2_options_load(void)
{
    u8 b[NBYTES], i;
    SRAM_enableRO();
    for (i = 0; i < NBYTES; i++) b[i] = SRAM_readByte(BLOCK + i);
    SRAM_disable();
    ss2_set_language(b[0] == 'S' && b[1] == '2' && b[2] == VERSION && b[4] == check(b) &&
                     b[3] <= SS2_LANG_JAPANESE ? b[3] : SS2_LANG_ENGLISH);
}

void ss2_options_save(void)
{
    u8 b[NBYTES], i;
    b[0] = 'S'; b[1] = '2'; b[2] = VERSION; b[3] = ss2_language;
    b[4] = check(b);
    SRAM_enable();
    for (i = 0; i < NBYTES; i++) SRAM_writeByte(BLOCK + i, b[i]);
    SRAM_disable();
}

#include "genesis.h"
#include "release_version.h"

__attribute__((externally_visible))
const ROMHeader rom_header = {
    /* This port uses a flat <=4 MiB cartridge. The shared SDK may enable its
     * SSF mapper for other projects; that must not select this ROM's hardware. */
    "SEGA MEGA DRIVE ",
    "(C)SGDK 2024    ",
    SONSON_ROM_TITLE,
    SONSON_ROM_TITLE,
    "GM 00000000-00",
    0x000,
    "JD              ",
    0x00000000,
    0x001FFFFF,
    0xE0FF0000,
    0xE0FFFFFF,
    "RA",
    0xF820,
    0x00200001,
    0x0020FFFF,
    "            ",
    "NATIVE PORT                            ",
    "JUE             "
};

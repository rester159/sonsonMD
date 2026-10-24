"""spawns.py [BANK...] -- list stage spawn records whose code bank is $09,
$0F or $1F (or the banks given in hex).
Stage list table: bank 4 $BD08 (2 bytes per stage) -> bank 8 list of 64
screen pointers (8x8 screens) -> 6-byte records x, y, spawner, bank, lo, hi."""
import os, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
rom = open(os.path.join(ROOT, 'arcade', 'Son_Son_II_J_TEng.pce'), 'rb').read()


def b(bank, a):
    return rom[bank * 0x2000 + (a & 0x1FFF)]


want = set(int(a, 16) for a in sys.argv[1:]) or {9, 0xF, 0x1F}
for s in range(7):
    lst = b(4, 0xBD08 + 2 * s) | b(4, 0xBD09 + 2 * s) << 8
    for scr in range(64):
        r = b(8, lst + 2 * scr) | b(8, lst + 2 * scr + 1) << 8
        while b(8, r) != 0xFF:
            x, y, sp, bk, lo, hi = (b(8, r + i) for i in range(6))
            if bk in want:
                wx = (scr & 7) * 256 + x
                wy = (scr >> 3) * 256 + y
                print('stage %d (k=%d) screen %d,%d world (%4d,%4d) spawner %02X bank %02X handler %04X'
                      % (s + 1, s, scr & 7, scr >> 3, wx, wy, sp, bk, lo | hi << 8))
            r += 6

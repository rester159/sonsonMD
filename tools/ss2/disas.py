"""dis.py BANK:ADDR [count] [mpr-overrides like 4=04]  -- disassemble with an MPR context."""
import sys, huc6280
rom = open(__file__.rsplit('/', 3)[0] + '/arcade/Son_Son_II_J_TEng.pce', 'rb').read()
def make_read(mpr):
    def rd(a):
        a &= 0xFFFF; b = mpr[a >> 13]
        if b >= 0x80: return 0
        return rom[(b % 32) * 0x2000 + (a & 0x1FFF)]
    return rd
if __name__ == '__main__':
    bank, addr = sys.argv[1].split(':'); bank = int(bank, 16); addr = int(addr, 16)
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 40
    mpr = [0xFF, 0xF8, 1, 2, 3, 4, 5, 0]
    mpr[addr >> 13] = bank
    for o in sys.argv[3:]:
        i, v = o.split('='); mpr[int(i)] = int(v, 16)
    rd = make_read(mpr); pc = addr
    for _ in range(n):
        if pc > 0xFFFF: break
        t, k, _ = huc6280.disasm_one(rd, pc)
        print('%02X:%04X  %-12s %s' % (mpr[pc >> 13], pc, ' '.join('%02X' % rd(pc + i) for i in range(k)), t))
        pc += k

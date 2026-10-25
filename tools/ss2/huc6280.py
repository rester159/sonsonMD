"""HuC6280 opcode table, disassembler and a small interpreter.

Offline research tool for the Son Son II (PC Engine) port: it runs the
original HuCard's own loaders so VRAM/CRAM/SATB contents, map decoders and
sound-register writes can be observed without an external emulator. It is
never part of the Genesis cartridge.
"""

# ---- opcode table -------------------------------------------------------
# modes: imp acc imm zp zpx zpy abs absx absy ind indx indy zpind absindx
#        rel zprel blk immzp immzpx immabs immabsx
_T = """
00 BRK imp|01 ORA indx|02 SXY imp|03 ST0 imm|04 TSB zp|05 ORA zp|06 ASL zp|08 PHP imp|09 ORA imm|0A ASL acc|0C TSB abs|0D ORA abs|0E ASL abs
10 BPL rel|11 ORA indy|12 ORA zpind|13 ST1 imm|14 TRB zp|15 ORA zpx|16 ASL zpx|18 CLC imp|19 ORA absy|1A INC acc|1C TRB abs|1D ORA absx|1E ASL absx
20 JSR abs|21 AND indx|22 SAX imp|23 ST2 imm|24 BIT zp|25 AND zp|26 ROL zp|28 PLP imp|29 AND imm|2A ROL acc|2C BIT abs|2D AND abs|2E ROL abs
30 BMI rel|31 AND indy|32 AND zpind|34 BIT zpx|35 AND zpx|36 ROL zpx|38 SEC imp|39 AND absy|3A DEC acc|3C BIT absx|3D AND absx|3E ROL absx
40 RTI imp|41 EOR indx|42 SAY imp|43 TMA imm|44 BSR rel|45 EOR zp|46 LSR zp|48 PHA imp|49 EOR imm|4A LSR acc|4C JMP abs|4D EOR abs|4E LSR abs
50 BVC rel|51 EOR indy|52 EOR zpind|53 TAM imm|54 CSL imp|55 EOR zpx|56 LSR zpx|58 CLI imp|59 EOR absy|5A PHY imp|5D EOR absx|5E LSR absx
60 RTS imp|61 ADC indx|62 CLA imp|64 STZ zp|65 ADC zp|66 ROR zp|68 PLA imp|69 ADC imm|6A ROR acc|6C JMP ind|6D ADC abs|6E ROR abs
70 BVS rel|71 ADC indy|72 ADC zpind|73 TII blk|74 STZ zpx|75 ADC zpx|76 ROR zpx|78 SEI imp|79 ADC absy|7A PLY imp|7C JMP absindx|7D ADC absx|7E ROR absx
80 BRA rel|81 STA indx|82 CLX imp|83 TST immzp|84 STY zp|85 STA zp|86 STX zp|88 DEY imp|89 BIT imm|8A TXA imp|8C STY abs|8D STA abs|8E STX abs
90 BCC rel|91 STA indy|92 STA zpind|93 TST immabs|94 STY zpx|95 STA zpx|96 STX zpy|98 TYA imp|99 STA absy|9A TXS imp|9C STZ abs|9D STA absx|9E STZ absx
A0 LDY imm|A1 LDA indx|A2 LDX imm|A3 TST immzpx|A4 LDY zp|A5 LDA zp|A6 LDX zp|A8 TAY imp|A9 LDA imm|AA TAX imp|AC LDY abs|AD LDA abs|AE LDX abs
B0 BCS rel|B1 LDA indy|B2 LDA zpind|B3 TST immabsx|B4 LDY zpx|B5 LDA zpx|B6 LDX zpy|B8 CLV imp|B9 LDA absy|BA TSX imp|BC LDY absx|BD LDA absx|BE LDX absy
C0 CPY imm|C1 CMP indx|C2 CLY imp|C3 TDD blk|C4 CPY zp|C5 CMP zp|C6 DEC zp|C8 INY imp|C9 CMP imm|CA DEX imp|CC CPY abs|CD CMP abs|CE DEC abs
D0 BNE rel|D1 CMP indy|D2 CMP zpind|D3 TIN blk|D4 CSH imp|D5 CMP zpx|D6 DEC zpx|D8 CLD imp|D9 CMP absy|DA PHX imp|DD CMP absx|DE DEC absx
E0 CPX imm|E1 SBC indx|E3 TIA blk|E4 CPX zp|E5 SBC zp|E6 INC zp|E8 INX imp|E9 SBC imm|EA NOP imp|EC CPX abs|ED SBC abs|EE INC abs
F0 BEQ rel|F1 SBC indy|F2 SBC zpind|F3 TAI blk|F4 SET imp|F5 SBC zpx|F6 INC zpx|F8 SED imp|F9 SBC absy|FA PLX imp|FD SBC absx|FE INC absx
"""
OPS = {}
for _line in _T.strip().splitlines():
    for _e in _line.split('|'):
        _o, _m, _md = _e.split()
        OPS[int(_o, 16)] = (_m, _md)
for _b in range(8):
    OPS[0x07 + _b * 16] = ('RMB%d' % _b, 'zp')
    OPS[0x87 + _b * 16] = ('SMB%d' % _b, 'zp')
    OPS[0x0F + _b * 16] = ('BBR%d' % _b, 'zprel')
    OPS[0x8F + _b * 16] = ('BBS%d' % _b, 'zprel')

SIZE = {'imp': 1, 'acc': 1, 'imm': 2, 'zp': 2, 'zpx': 2, 'zpy': 2, 'abs': 3, 'absx': 3,
        'absy': 3, 'ind': 3, 'indx': 2, 'indy': 2, 'zpind': 2, 'absindx': 3, 'rel': 2,
        'zprel': 3, 'blk': 7, 'immzp': 3, 'immzpx': 3, 'immabs': 4, 'immabsx': 4}


def disasm_one(read, pc):
    """read(addr)->byte over logical space. Returns (text, size, targets)."""
    op = read(pc)
    if op not in OPS:
        return ('.db $%02X' % op, 1, [])
    m, md = OPS[op]
    n = SIZE[md]
    b = [read((pc + i) & 0xFFFF) for i in range(n)]
    w = lambda i: b[i] | (b[i + 1] << 8)
    tg = []
    if md in ('imp',): s = m
    elif md == 'acc': s = m + ' A'
    elif md == 'imm': s = '%s #$%02X' % (m, b[1])
    elif md == 'zp': s = '%s <$%02X' % (m, b[1])
    elif md == 'zpx': s = '%s <$%02X,X' % (m, b[1])
    elif md == 'zpy': s = '%s <$%02X,Y' % (m, b[1])
    elif md == 'abs':
        s = '%s $%04X' % (m, w(1))
        if m in ('JSR', 'JMP'): tg.append(w(1))
    elif md == 'absx': s = '%s $%04X,X' % (m, w(1))
    elif md == 'absy': s = '%s $%04X,Y' % (m, w(1))
    elif md == 'ind': s = '%s ($%04X)' % (m, w(1))
    elif md == 'absindx': s = '%s ($%04X,X)' % (m, w(1))
    elif md == 'indx': s = '%s (<$%02X,X)' % (m, b[1])
    elif md == 'indy': s = '%s (<$%02X),Y' % (m, b[1])
    elif md == 'zpind': s = '%s (<$%02X)' % (m, b[1])
    elif md == 'rel':
        t = (pc + 2 + (b[1] - 256 if b[1] & 0x80 else b[1])) & 0xFFFF
        s = '%s $%04X' % (m, t); tg.append(t)
    elif md == 'zprel':
        t = (pc + 3 + (b[2] - 256 if b[2] & 0x80 else b[2])) & 0xFFFF
        s = '%s <$%02X,$%04X' % (m, b[1], t); tg.append(t)
    elif md == 'blk': s = '%s $%04X,$%04X,$%04X' % (m, w(1), w(3), w(5))
    elif md == 'immzp': s = '%s #$%02X,<$%02X' % (m, b[1], b[2])
    elif md == 'immzpx': s = '%s #$%02X,<$%02X,X' % (m, b[1], b[2])
    elif md == 'immabs': s = '%s #$%02X,$%04X' % (m, b[1], w(2))
    elif md == 'immabsx': s = '%s #$%02X,$%04X,X' % (m, b[1], w(2))
    return (s, n, tg)


# ---- interpreter --------------------------------------------------------
FN, FV, FT, FB, FD, FI, FZ, FC = 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01

_CYC = {'imp': 2, 'acc': 2, 'imm': 2, 'zp': 4, 'zpx': 4, 'zpy': 4, 'abs': 5, 'absx': 5,
        'absy': 5, 'ind': 7, 'indx': 7, 'indy': 7, 'zpind': 7, 'absindx': 7, 'rel': 2,
        'zprel': 6, 'blk': 17, 'immzp': 7, 'immzpx': 7, 'immabs': 8, 'immabsx': 8}


class CPU:
    def __init__(self, bus):
        self.bus = bus
        self.a = self.x = self.y = 0
        self.s = 0xFF
        self.p = FI
        self.pc = 0
        self.mpr = [0xFF, 0xF8, 0, 0, 0, 0, 0, 0]
        self.cycles = 0
        self.irq_disable = 0
        self.speed_hi = False
        self.trace = None
        self.halted = False
        self.writes = 0
        self.idle = False
        self._seen = set()
        self._seen_w = -1

    def _backjump(self, frm):
        # identical register state at a backward-branch target with no writes
        # since the last visit: only an interrupt can change what happens next
        if self.pc >= frm:
            return
        if self.writes != self._seen_w:
            self._seen.clear(); self._seen_w = self.writes
        k = (self.pc, self.a, self.x, self.y, self.p, self.s, self.mpr[self.pc >> 13])
        if k in self._seen:
            self.idle = True
        else:
            self._seen.add(k)

    # logical memory
    def phys(self, a):
        return (self.mpr[a >> 13] << 13) | (a & 0x1FFF)

    def rd(self, a):
        return self.bus.read((self.mpr[a >> 13] << 13) | (a & 0x1FFF))

    def wr(self, a, v):
        p = (self.mpr[a >> 13] << 13) | (a & 0x1FFF)
        v &= 0xFF
        # a RAM store of the value already present cannot change the future
        if not (0xF8 <= p >> 13 <= 0xFB and self.bus.ram[p & 0x1FFF] == v):
            self.writes += 1
        self.bus.write(p, v)

    def rdzp(self, z):
        return self.rd(0x2000 | (z & 0xFF))

    def wrzp(self, z, v):
        self.wr(0x2000 | (z & 0xFF), v)

    def zpw(self, z):
        return self.rdzp(z) | (self.rdzp(z + 1) << 8)

    def push(self, v):
        self.wr(0x2100 | self.s, v); self.s = (self.s - 1) & 0xFF

    def pull(self):
        self.s = (self.s + 1) & 0xFF; return self.rd(0x2100 | self.s)

    def reset(self):
        self.mpr[7] = 0
        self.pc = self.rd(0xFFFE) | (self.rd(0xFFFF) << 8)
        self.p = FI

    def irq(self, vector):
        self.push(self.pc >> 8); self.push(self.pc & 0xFF)
        self.push((self.p & ~FB) & 0xFF)
        self.p = (self.p | FI) & ~(FD | FT)
        self.pc = self.rd(vector) | (self.rd(vector + 1) << 8)
        self.cycles += 8

    def nz(self, v):
        self.p = (self.p & ~(FN | FZ)) | (v & 0x80) | (0 if v else FZ)
        return v

    def adc(self, a, v):
        c = self.p & FC
        if self.p & FD:
            lo = (a & 0x0F) + (v & 0x0F) + c
            hi = (a >> 4) + (v >> 4)
            if lo > 9: lo -= 10; hi += 1
            if hi > 9: hi -= 10; self.p |= FC
            else: self.p &= ~FC
            r = ((hi << 4) | lo) & 0xFF
            self.cycles += 1
        else:
            t = a + v + c
            r = t & 0xFF
            self.p = (self.p & ~(FC | FV)) | (FC if t > 0xFF else 0) | \
                (FV if (~(a ^ v) & (a ^ r) & 0x80) else 0)
        return self.nz(r)

    def sbc(self, a, v):
        c = self.p & FC
        if self.p & FD:
            lo = (a & 0x0F) - (v & 0x0F) - (1 - c)
            hi = (a >> 4) - (v >> 4)
            if lo < 0: lo += 10; hi -= 1
            if hi < 0: hi += 10; self.p &= ~FC
            else: self.p |= FC
            r = ((hi << 4) | lo) & 0xFF
            self.cycles += 1
        else:
            t = a - v - (1 - c)
            r = t & 0xFF
            self.p = (self.p & ~(FC | FV)) | (FC if t >= 0 else 0) | \
                (FV if ((a ^ v) & (a ^ r) & 0x80) else 0)
        return self.nz(r)

    def cmp(self, r, v):
        t = r - v
        self.p = (self.p & ~FC) | (FC if t >= 0 else 0)
        self.nz(t & 0xFF)

    def step(self):
        pc = self.pc
        op = self.rd(pc)
        m, md = OPS.get(op, ('???', 'imp'))
        if self.trace is not None:
            self.trace(self, pc, op)
            if self.pc != pc:           # a research hook redirected execution
                pc = self.pc
                op = self.rd(pc)
                m, md = OPS.get(op, ('???', 'imp'))
        n = SIZE[md]
        self.pc = (pc + n) & 0xFFFF
        self.cycles += _CYC[md]
        tflag = self.p & FT
        self.p &= ~FT
        rd = self.rd
        # effective address
        ea = None
        if md == 'imm': imm = rd(pc + 1)
        elif md == 'zp': ea = 0x2000 | rd(pc + 1)
        elif md == 'zpx': ea = 0x2000 | ((rd(pc + 1) + self.x) & 0xFF)
        elif md == 'zpy': ea = 0x2000 | ((rd(pc + 1) + self.y) & 0xFF)
        elif md == 'abs': ea = rd(pc + 1) | (rd(pc + 2) << 8)
        elif md == 'absx': ea = ((rd(pc + 1) | (rd(pc + 2) << 8)) + self.x) & 0xFFFF
        elif md == 'absy': ea = ((rd(pc + 1) | (rd(pc + 2) << 8)) + self.y) & 0xFFFF
        elif md == 'indx': ea = self.zpw(rd(pc + 1) + self.x)
        elif md == 'indy': ea = (self.zpw(rd(pc + 1)) + self.y) & 0xFFFF
        elif md == 'zpind': ea = self.zpw(rd(pc + 1))
        elif md == 'ind':
            t = rd(pc + 1) | (rd(pc + 2) << 8); ea = rd(t) | (rd((t + 1) & 0xFFFF) << 8)
        elif md == 'absindx':
            t = ((rd(pc + 1) | (rd(pc + 2) << 8)) + self.x) & 0xFFFF
            ea = rd(t) | (rd((t + 1) & 0xFFFF) << 8)

        def src():
            return imm if md == 'imm' else rd(ea)

        # ALU ops with T flag operate on (X) in zero page
        if m in ('LDA',): self.a = self.nz(src())
        elif m == 'LDX': self.x = self.nz(src())
        elif m == 'LDY': self.y = self.nz(src())
        elif m == 'STA': self.wr(ea, self.a)
        elif m == 'STX': self.wr(ea, self.x)
        elif m == 'STY': self.wr(ea, self.y)
        elif m == 'STZ': self.wr(ea, 0)
        elif m in ('ORA', 'AND', 'EOR', 'ADC', 'SBC'):
            v = src()
            acc = self.rdzp(self.x) if tflag else self.a
            if m == 'ORA': r = self.nz(acc | v)
            elif m == 'AND': r = self.nz(acc & v)
            elif m == 'EOR': r = self.nz(acc ^ v)
            elif m == 'ADC': r = self.adc(acc, v)
            else: r = self.sbc(acc, v)
            if tflag: self.wrzp(self.x, r); self.cycles += 3
            else: self.a = r
        elif m == 'CMP': self.cmp(self.a, src())
        elif m == 'CPX': self.cmp(self.x, src())
        elif m == 'CPY': self.cmp(self.y, src())
        elif m == 'BIT':
            v = src()
            self.p = (self.p & ~(FN | FV | FZ)) | (v & 0xC0) | (0 if (v & self.a) else FZ)
        elif m in ('ASL', 'LSR', 'ROL', 'ROR', 'INC', 'DEC'):
            v = self.a if md == 'acc' else rd(ea)
            c = self.p & FC
            if m == 'ASL': nc = v >> 7; v = (v << 1) & 0xFF
            elif m == 'LSR': nc = v & 1; v >>= 1
            elif m == 'ROL': nc = v >> 7; v = ((v << 1) | c) & 0xFF
            elif m == 'ROR': nc = v & 1; v = (v >> 1) | (c << 7)
            elif m == 'INC': nc = c; v = (v + 1) & 0xFF
            else: nc = c; v = (v - 1) & 0xFF
            self.p = (self.p & ~FC) | nc
            self.nz(v)
            if md == 'acc': self.a = v
            else: self.wr(ea, v); self.cycles += 2
        elif m == 'TSB' or m == 'TRB':
            v = rd(ea)
            self.p = (self.p & ~(FN | FV | FZ)) | (v & 0xC0) | (0 if (v & self.a) else FZ)
            self.wr(ea, (v | self.a) if m == 'TSB' else (v & ~self.a))
        elif m == 'TST':
            iv = rd(pc + 1)
            if md == 'immzp': ea = 0x2000 | rd(pc + 2)
            elif md == 'immzpx': ea = 0x2000 | ((rd(pc + 2) + self.x) & 0xFF)
            elif md == 'immabs': ea = rd(pc + 2) | (rd(pc + 3) << 8)
            else: ea = ((rd(pc + 2) | (rd(pc + 3) << 8)) + self.x) & 0xFFFF
            v = rd(ea)
            self.p = (self.p & ~(FN | FV | FZ)) | (v & 0xC0) | (0 if (v & iv) else FZ)
        elif m[:3] in ('RMB', 'SMB'):
            b = int(m[3]); v = rd(ea)
            self.wr(ea, (v & ~(1 << b)) if m[0] == 'R' else (v | (1 << b)))
        elif m[:3] in ('BBR', 'BBS'):
            b = int(m[3]); v = self.rdzp(rd(pc + 1))
            if bool(v & (1 << b)) == (m[2] == 'S'):
                o = rd(pc + 2); self.pc = (self.pc + (o - 256 if o & 0x80 else o)) & 0xFFFF
                self.cycles += 2
                self._backjump(pc)
        elif md == 'rel':
            o = rd(pc + 1)
            f = self.p
            take = {'BPL': not f & FN, 'BMI': f & FN, 'BVC': not f & FV, 'BVS': f & FV,
                    'BCC': not f & FC, 'BCS': f & FC, 'BNE': not f & FZ, 'BEQ': f & FZ,
                    'BRA': True, 'BSR': True}[m]
            if m == 'BSR':
                r = (self.pc - 1) & 0xFFFF
                self.push(r >> 8); self.push(r & 0xFF)
            if take:
                self.pc = (self.pc + (o - 256 if o & 0x80 else o)) & 0xFFFF
                self.cycles += 2
                self._backjump(pc)
        elif m == 'JMP': self.pc = ea; self._backjump(pc)
        elif m == 'JSR':
            r = (self.pc - 1) & 0xFFFF
            self.push(r >> 8); self.push(r & 0xFF); self.pc = ea
        elif m == 'RTS':
            lo = self.pull(); self.pc = ((lo | (self.pull() << 8)) + 1) & 0xFFFF
        elif m == 'RTI':
            self.p = self.pull(); lo = self.pull(); self.pc = lo | (self.pull() << 8)
        elif m == 'BRK':
            self.pc = (pc + 2) & 0xFFFF
            self.push(self.pc >> 8); self.push(self.pc & 0xFF); self.push(self.p | FB)
            self.p = (self.p | FI) & ~(FD | FT)
            self.pc = rd(0xFFF6) | (rd(0xFFF7) << 8)
        elif m == 'PHA': self.push(self.a)
        elif m == 'PHX': self.push(self.x)
        elif m == 'PHY': self.push(self.y)
        elif m == 'PHP': self.push(self.p | FB)
        elif m == 'PLA': self.a = self.nz(self.pull())
        elif m == 'PLX': self.x = self.nz(self.pull())
        elif m == 'PLY': self.y = self.nz(self.pull())
        elif m == 'PLP': self.p = self.pull()
        elif m == 'TAX': self.x = self.nz(self.a)
        elif m == 'TAY': self.y = self.nz(self.a)
        elif m == 'TXA': self.a = self.nz(self.x)
        elif m == 'TYA': self.a = self.nz(self.y)
        elif m == 'TSX': self.x = self.nz(self.s)
        elif m == 'TXS': self.s = self.x
        elif m == 'INX': self.x = self.nz((self.x + 1) & 0xFF)
        elif m == 'INY': self.y = self.nz((self.y + 1) & 0xFF)
        elif m == 'DEX': self.x = self.nz((self.x - 1) & 0xFF)
        elif m == 'DEY': self.y = self.nz((self.y - 1) & 0xFF)
        elif m == 'CLC': self.p &= ~FC
        elif m == 'SEC': self.p |= FC
        elif m == 'CLI': self.p &= ~FI
        elif m == 'SEI': self.p |= FI
        elif m == 'CLD': self.p &= ~FD
        elif m == 'SED': self.p |= FD
        elif m == 'CLV': self.p &= ~FV
        elif m == 'CLA': self.a = 0
        elif m == 'CLX': self.x = 0
        elif m == 'CLY': self.y = 0
        elif m == 'SAX': self.a, self.x = self.x, self.a
        elif m == 'SAY': self.a, self.y = self.y, self.a
        elif m == 'SXY': self.x, self.y = self.y, self.x
        elif m == 'SET': self.p |= FT
        elif m == 'CSL': self.speed_hi = False
        elif m == 'CSH': self.speed_hi = True
        elif m == 'NOP': pass
        elif m in ('ST0', 'ST1', 'ST2'):
            self.writes += 1
            self.bus.write(0x1FE000 + {'ST0': 0, 'ST1': 2, 'ST2': 3}[m], rd(pc + 1))
        elif m == 'TAM':
            v = rd(pc + 1)
            for i in range(8):
                if v & (1 << i): self.mpr[i] = self.a
        elif m == 'TMA':
            v = rd(pc + 1)
            for i in range(8):
                if v & (1 << i): self.a = self.mpr[i]
        elif md == 'blk':
            s_ = rd(pc + 1) | (rd(pc + 2) << 8)
            d_ = rd(pc + 3) | (rd(pc + 4) << 8)
            ln = rd(pc + 5) | (rd(pc + 6) << 8) or 0x10000
            alt = 0
            for _ in range(ln):
                if m == 'TAI': v = rd((s_ + alt) & 0xFFFF)
                else: v = rd(s_)
                if m == 'TIA': self.wr((d_ + alt) & 0xFFFF, v)
                else: self.wr(d_, v)
                if m in ('TII', 'TIN', 'TIA'): s_ = (s_ + 1) & 0xFFFF
                elif m == 'TDD': s_ = (s_ - 1) & 0xFFFF
                if m in ('TII', 'TAI'): d_ = (d_ + 1) & 0xFFFF
                elif m == 'TDD': d_ = (d_ - 1) & 0xFFFF
                alt ^= 1
            self.cycles += 6 * ln
        else:
            raise RuntimeError('unimplemented %02X %s at %04X' % (op, m, pc))

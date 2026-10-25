"""Minimal PC Engine machine model for offline Son Son II research.

Models only what the game's loaders and frame loop observe: HuCard ROM
banking, 8 KiB work RAM, HuC6270 VDC registers/VRAM/DMA/SATB, HuC6260 VCE
colour table, the joypad multiplexer, the timer and IRQ control. Rendering
lives in render.py. Raster timing is approximate (scanline granularity).
"""
from huc6280 import CPU

LINES = 263
CYC_LINE = 455          # master/CPU cycles per line at 7.16 MHz
VBL_LINE = 64 + 240     # VDC active area normally starts at line ~64? see vblank()

PAD_UP, PAD_RIGHT, PAD_DOWN, PAD_LEFT = 1, 2, 4, 8
PAD_I, PAD_II, PAD_SEL, PAD_RUN = 0x10, 0x20, 0x40, 0x80


class PCE:
    def __init__(self, rom):
        self.rom = rom
        self.nbanks = len(rom) // 0x2000
        self.ram = bytearray(0x2000)
        self.vram = [0] * 0x8000
        self.vreg = [0] * 32
        self.vaddr = 0
        self.vstatus = 0
        self.vread = 0
        self.satb = [0] * 256
        self.satb_pending = False
        self.cram = [0] * 512
        self.caddr = 0
        self.vce_ctrl = 0
        self.pad = 0            # buttons held (PAD_* bits)
        self.pad_sel = 0
        self.timer_reload = 0
        self.timer_count = 0
        self.timer_on = 0
        self.timer_cyc = 0
        self.irq_mask = 0
        self.irq_timer = 0
        self.psg_log = []       # (frame, reg, value)
        self.psg_sel = 0
        self.vram_log = None    # optional list of (frame, line, pc, mpr, addr, word)
        self.frame = 0
        self.line = 0
        self.bylatch = 0
        self.line_regs = []     # per-line (BXR, BYR, CR) snapshot of the last frame
        self.cpu = CPU(self)
        self.cpu.reset()

    # ---- bus ------------------------------------------------------------
    def read(self, a):
        bank = a >> 13
        off = a & 0x1FFF
        if bank < 0x80:
            return self.rom[(bank % self.nbanks) * 0x2000 + off]
        if 0xF8 <= bank <= 0xFB:
            return self.ram[off]
        if bank == 0xFF:
            return self.io_read(off)
        return 0xFF

    def write(self, a, v):
        bank = a >> 13
        off = a & 0x1FFF
        if 0xF8 <= bank <= 0xFB:
            self.ram[off] = v
        elif bank == 0xFF:
            self.io_write(off, v)

    def io_read(self, off):
        if off < 0x400:
            r = off & 3
            if r == 0:
                s = self.vstatus
                self.vstatus &= 0x40
                return s
            if r == 2: return self.vread & 0xFF
            if r == 3:
                v = self.vread >> 8
                self.vreg[1] = (self.vreg[1] + self.inc()) & 0xFFFF
                self.vread = self.vram[self.vreg[1] & 0x7FFF]
                return v
            return 0
        if off < 0x800:
            r = off & 7
            if r == 4: return self.cram[self.caddr] & 0xFF
            if r == 5:
                v = 0xFE | (self.cram[self.caddr] >> 8)
                self.caddr = (self.caddr + 1) & 0x1FF
                return v
            return 0xFF
        if off < 0xC00: return 0
        if off < 0x1000: return self.timer_count & 0x7F
        if off < 0x1400:
            # bits 4,5 set; bit6 = 1 (PC Engine, Japanese); bit7 = 1 (no CD)
            if self.pad_sel:
                n = self.pad & 0x0F
            else:
                n = (self.pad >> 4) & 0x0F
            return 0xF0 | (~n & 0x0F) if True else 0
        if off < 0x1800:
            r = off & 3
            if r == 2: return self.irq_mask
            if r == 3:
                return (4 if self.irq_timer else 0) | (2 if self.vstatus & 0x3F else 0)
        return 0xFF

    def inc(self):
        return (1, 32, 64, 128)[(self.vreg[5] >> 11) & 3]

    def io_write(self, off, v):
        if off < 0x400:
            r = off & 3
            if r == 0:
                self.vaddr = v & 0x1F
            elif r == 2:
                self.vreg[self.vaddr] = (self.vreg[self.vaddr] & 0xFF00) | v
                if self.vaddr == 8:
                    self.bylatch = self.vreg[8] + 1
            elif r == 3:
                reg = self.vaddr
                self.vreg[reg] = (self.vreg[reg] & 0x00FF) | (v << 8)
                if reg == 2:
                    a = self.vreg[0] & 0xFFFF
                    w = self.vreg[2]
                    if a < 0x8000:
                        self.vram[a] = w
                        if self.vram_log is not None:
                            c = self.cpu
                            self.vram_log.append((self.frame, self.line, c.pc, tuple(c.mpr), a, w))
                    self.vreg[0] = (a + self.inc()) & 0xFFFF
                elif reg == 1:
                    self.vread = self.vram[self.vreg[1] & 0x7FFF]
                elif reg == 0x12:
                    self.vram_dma()
                elif reg == 0x13:
                    self.satb_pending = True
                if reg == 8:
                    self.bylatch = self.vreg[8] + 1
            return
        if off < 0x800:
            r = off & 7
            if r == 0: self.vce_ctrl = v
            elif r == 2: self.caddr = (self.caddr & 0x100) | v
            elif r == 3: self.caddr = (self.caddr & 0xFF) | ((v & 1) << 8)
            elif r == 4: self.cram[self.caddr] = (self.cram[self.caddr] & 0x100) | v
            elif r == 5:
                self.cram[self.caddr] = (self.cram[self.caddr] & 0xFF) | ((v & 1) << 8)
                self.caddr = (self.caddr + 1) & 0x1FF
            return
        if off < 0xC00:
            r = off & 0xF
            if r == 0: self.psg_sel = v & 7
            self.psg_log.append((self.frame, self.line, r, self.psg_sel, v))
            return
        if off < 0x1000:
            if off & 1:
                if (v & 1) and not self.timer_on:
                    self.timer_count = self.timer_reload
                    self.timer_cyc = 0
                self.timer_on = v & 1
            else:
                self.timer_reload = v & 0x7F
            return
        if off < 0x1400:
            self.pad_sel = v & 1
            return
        if off < 0x1800:
            r = off & 3
            if r == 2: self.irq_mask = v & 7
            elif r == 3: self.irq_timer = 0

    def vram_dma(self):
        s, d, n = self.vreg[0x10], self.vreg[0x11], self.vreg[0x12]
        dcr = self.vreg[0x0F]
        for _ in range(n + 1):
            self.vram[d & 0x7FFF] = self.vram[s & 0x7FFF]
            s = (s - 1 if dcr & 4 else s + 1) & 0xFFFF
            d = (d - 1 if dcr & 8 else d + 1) & 0xFFFF
        self.vreg[0x10], self.vreg[0x11] = s, d
        self.vstatus |= 0x10

    # ---- timing ---------------------------------------------------------
    def run_cycles(self, n):
        c = self.cpu
        end = c.cycles + n
        while c.cycles < end:
            if not (c.p & 0x04):
                if self.irq_timer and not (self.irq_mask & 4):
                    c.irq(0xFFFA); c._seen.clear()
                elif (self.vstatus & 0x2F) and not (self.irq_mask & 2):
                    c.irq(0xFFF8); c._seen.clear()
            before = c.cycles
            c.step()
            if c.idle:
                c.idle = False
                if c.cycles < end:
                    c.cycles = end
            if self.timer_on:
                self.timer_cyc += (c.cycles - before) * (1 if c.speed_hi else 4)
                while self.timer_cyc >= 1024:
                    self.timer_cyc -= 1024
                    if self.timer_count == 0:
                        self.timer_count = self.timer_reload
                        self.irq_timer = 1
                    else:
                        self.timer_count -= 1

    def run_frame(self):
        """One video frame: active display lines then vertical blank."""
        vdw = (self.vreg[0x0D] & 0x1FF) + 1
        vds = ((self.vreg[0x0C] >> 8) & 0xFF) + 2
        vsw = (self.vreg[0x0C] & 0x1F) + 1
        top = vsw + vds                      # first active line (approx.)
        self.line_regs = []
        cpl = CYC_LINE if self.cpu.speed_hi else CYC_LINE // 4
        self.bylatch = self.vreg[8]
        for line in range(LINES):
            self.line = line
            act = line - top
            if 0 <= act < vdw:
                if act == 0:
                    self.bylatch = self.vreg[8]
                self.line_regs.append((self.vreg[7], self.bylatch, self.vreg[5]))
                self.bylatch += 1
                # raster compare: RCR counts from 64 at the first active line
                if (self.vreg[5] & 4) and (self.vreg[6] & 0x3FF) == act + 64:
                    self.vstatus |= 0x04
            if act == vdw:
                self.vblank()
            self.run_cycles(cpl)
        self.frame += 1

    def vblank(self):
        if self.satb_pending or (self.vreg[0x0F] & 0x10):
            base = self.vreg[0x13] & 0x7FFF
            self.satb = self.vram[base:base + 256]
            self.satb_pending = False
            if self.vreg[0x0F] & 1:
                self.vstatus |= 0x08
        if self.vreg[5] & 8:
            self.vstatus |= 0x20

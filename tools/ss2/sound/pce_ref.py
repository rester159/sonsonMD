"""Research-model reference for the Son Son II sound driver (banks 0x0A/0x0B).

Runs the ORIGINAL HuC6280 driver code inside the project's offline model
(tools/ss2/huc6280.py + pce.py; never MAME) and records every PSG register
write. Two modes:

  isolated(schedule, frames)
      Fresh machine, zeroed work RAM, MPR 4/5 = banks 0A/0B exactly as the
      VDC IRQ handler (bank 0 $E16D..$E191) maps them. Each frame: if the
      schedule has a request for this frame, JSR $8003 with A=id, then
      JSR $8000 (driver tick). Mirrors the IRQ handler's one-request-per-frame
      ring pop.

  ingame(frames, script)
      Boot the whole game in the model with a scripted pad; record, per frame,
      the request ids the IRQ handler passes to $8003 and whether $8000 ran.
      The resulting schedule is what a native driver must be fed to reproduce
      the game's sound.

PSG writes are (frame, reg, selected_channel, value) tuples in program order.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SS2 = os.path.dirname(HERE)
if SS2 not in sys.path:
    sys.path.insert(0, SS2)
sys.setrecursionlimit(10000)
from pce import PCE  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(SS2))
ROM_PATH = os.path.join(ROOT, 'arcade', 'Son_Son_II_J_TEng.pce')
SENTINEL = 0xFFF0          # bank-0 address never executed while we call in


def rom():
    return open(ROM_PATH, 'rb').read()


def new_machine(romdata=None):
    m = PCE(romdata or rom())
    c = m.cpu
    c.mpr = [0xFF, 0xF8, 0x01, 0x02, 0x0A, 0x0B, 0x09, 0x00]
    c.s = 0xFF
    c.p = 0x04            # I set: no interrupts while we call routines
    return m


def call(m, addr, a=0, limit=2_000_000):
    """JSR addr with A=a; run until it returns. Returns executed steps."""
    c = m.cpu
    ret = SENTINEL - 1
    c.push(ret >> 8)
    c.push(ret & 0xFF)
    c.a = a & 0xFF
    c.pc = addr
    n = 0
    while c.pc != SENTINEL:
        c.step()
        n += 1
        if n > limit:
            raise RuntimeError('driver call $%04X did not return' % addr)
    return n


def event_tracer(m, out, periods=None):
    """Driver note events straight from the original code: $8321 (music note
    byte in A, channel 6-CF) and $881E (sfx event byte in A, channel CF-1).
    periods (optional): at $8398 (after a pitched note set its frequency)
    record (frame, channel, channel record $E/$F >> 2) = the note's base
    period before LFO/slide."""
    def tr(c, pc, op):
        if c.mpr[4] != 0x0A:
            return
        if pc == 0x8398 and periods is not None:
            r = (m.ram[0xD0] | (m.ram[0xD1] << 8)) & 0x1FFF
            periods.append((m.frame, 6 - m.ram[0xCF],
                            ((m.ram[r + 0xE] | (m.ram[r + 0xF] << 8)) >> 2)))
        elif pc == 0x8321:
            out.append((m.frame, 'm', 6 - m.ram[0xCF], c.a))
        elif pc == 0x881E:
            out.append((m.frame, 's', m.ram[0xCF] - 1, c.a))
    return tr


def isolated(schedule, frames, hold=None, romdata=None, events=None, periods=None):
    """schedule: {frame: [ids...]} (one id is popped per frame like the ring;
    extra ids stay queued). hold: optional {frame: value} for RAM $6B.
    Returns list of (frame, reg, sel, value)."""
    m = new_machine(romdata)
    m.psg_log = []
    if events is not None:
        m.cpu.trace = event_tracer(m, events, periods)
    queue = []
    for f in range(frames):
        m.frame = f
        queue.extend(schedule.get(f, []))
        if hold and f in hold:
            m.ram[0x6B] = hold[f]
        if queue:
            call(m, 0x8003, queue.pop(0))
        call(m, 0x8000)
    return [(f, r, s, v) for (f, _l, r, s, v) in m.psg_log], m


def ingame(frames, script_items=(), snapshot_every=None):
    """Boot the game. Returns dict(requests={frame:[ids]}, ticks=set(frames),
    psg=[(frame, reg, sel, value)], hold={frame: $6B when it changes})."""
    from run import parse_script
    ev = parse_script(list(script_items))
    m = PCE(rom())
    req, ticks, hold, events, calls = {}, set(), {}, [], []
    evtr = event_tracer(m, events)
    last6b = [m.ram[0x6B]]

    def tr(c, pc, op):
        if c.mpr[4] != 0x0A:
            return
        evtr(c, pc, op)
        if pc == 0x8003:
            req.setdefault(m.frame, []).append(c.a)
            calls.append((m.frame, 'R', c.a, m.ram[0x6B]))
            v = m.ram[0x6B]
            if v != last6b[0]:
                hold[m.frame] = v
                last6b[0] = v
        elif pc == 0x8000:
            ticks.add(m.frame)
            calls.append((m.frame, 'T', 0, m.ram[0x6B]))
            v = m.ram[0x6B]
            if v != last6b[0]:
                hold[m.frame] = v
                last6b[0] = v
    m.cpu.trace = tr
    for f in range(frames):
        m.pad = sum(b for a, z, b in ev if a <= f <= z)
        m.run_frame()
    psg = [(f, r, s, v) for (f, _l, r, s, v) in m.psg_log]
    # calls: every driver entry in execution order (the IRQ handler can
    # straddle the model's frame boundary, so per-frame sets are not exact)
    return dict(requests=req, ticks=ticks, psg=psg, hold=hold, events=events, calls=calls)


if __name__ == '__main__':
    sid = int(sys.argv[1], 16) if len(sys.argv) > 1 else 3
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 120
    log, _ = isolated({0: [sid]}, n)
    for e in log[:200]:
        print('f%4d r%X ch%d %02X' % e)
    print(len(log), 'writes')

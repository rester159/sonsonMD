"""Drive the research model: boot, apply a scripted input timeline, save frames.

usage: run.py ROM OUTDIR FRAMES [a-b:buttons ...]   buttons: U D L R 1 2 S N(run)
"""
import sys, os, time, pickle
from pce import PCE
import render
BTN = {'U': 1, 'R': 2, 'D': 4, 'L': 8, '1': 0x10, '2': 0x20, 'S': 0x40, 'N': 0x80}

def parse_script(items):
    ev = []
    for it in items:
        f, b = it.split(':')
        a, _, z = f.partition('-')
        ev.append((int(a), int(z or a), sum(BTN[c] for c in b)))
    return ev

if __name__ == '__main__':
    rom = open(sys.argv[1], 'rb').read()
    out = sys.argv[2]; os.makedirs(out, exist_ok=True)
    frames = int(sys.argv[3]); every = int(os.environ.get('EVERY', 60))
    ev = parse_script(sys.argv[4:])
    m = PCE(rom)
    t = time.time()
    for f in range(frames):
        m.pad = sum(b for a, z, b in ev if a <= f <= z)
        m.run_frame()
        if (f + 1) % every == 0:
            render.render(m).save('%s/f%05d.png' % (out, f + 1))
    print('frames', frames, 'sec %.1f' % (time.time() - t), 'pc %04X' % m.cpu.pc)
    pickle.dump({'vram': m.vram, 'cram': m.cram, 'vreg': m.vreg, 'satb': m.satb,
                 'ram': bytes(m.ram), 'line_regs': m.line_regs, 'mpr': m.cpu.mpr},
                open(out + '/state.pkl', 'wb'))

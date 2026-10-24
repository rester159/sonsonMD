"""Deterministic research-model snapshots used by the converter.

stage1_start: boot, RUN through the attract/intro presses, stop at the first
gameplay frame of stage 1 (frame 1560). Cached in out/ss2/.
"""
import os, pickle, sys
sys.setrecursionlimit(10000)
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
ROM = os.path.join(ROOT, 'arcade', 'Son_Son_II_J_TEng.pce')
OUT = os.path.join(ROOT, 'out', 'ss2')

RUN_PRESSES = [(350, 356), (500, 506), (650, 656), (800, 806)]


def stage1_start():
    path = os.path.join(OUT, 'stage1_start.pkl')
    if os.path.exists(path):
        return pickle.load(open(path, 'rb'))
    from pce import PCE
    m = PCE(open(ROM, 'rb').read())
    for f in range(1560):
        m.pad = 0x80 if any(a <= f <= z for a, z in RUN_PRESSES) else 0
        m.run_frame()
    m.cpu.trace = None
    os.makedirs(OUT, exist_ok=True)
    pickle.dump(m, open(path, 'wb'))
    return m


def stage_start(k, frames=900):
    """Stage index k (0-6) start state: from the stage-1 snapshot, enter the
    stage-clear path ($F861: INC $69 -> stage init $F9F0) with $69 = k-1 at
    a frame start, then press RUN periodically through the stage intro."""
    path = os.path.join(OUT, 'stage_idx%d.pkl' % k)
    if os.path.exists(path):
        return pickle.load(open(path, 'rb'))
    if k == 0:
        return stage1_start()
    m = stage1_start()
    done = [0]
    def tr(c, pc, op):
        if not done[0] and pc == 0xEC42 and c.mpr[7] == 0:
            m.ram[0x69] = k - 1
            c.pc = 0xF861
            done[0] = 1
    m.cpu.trace = tr
    for f in range(frames):
        m.pad = 0x80 if (f % 150) in range(60, 64) else 0
        m.run_frame()
    m.cpu.trace = None
    # RUN also toggles pause ($6B = 8): leave the stage running
    for _ in range(4):
        if not m.ram[0x6B]:
            break
        for f in range(40):
            m.pad = 0x80 if f < 4 else 0
            m.run_frame()
    assert not m.ram[0x6B], 'stage %d snapshot is paused' % (k + 1)
    pickle.dump(m, open(path, 'wb'))
    return m


if __name__ == '__main__':
    m = stage1_start()
    print('frame', m.frame)

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


if __name__ == '__main__':
    m = stage1_start()
    print('frame', m.frame)

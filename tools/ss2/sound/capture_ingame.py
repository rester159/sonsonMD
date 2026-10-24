"""capture_ingame.py OUT.pkl FRAMES [script...]

Boot Son Son II in the research model with a scripted pad and save the
sound-driver request/tick schedule plus the PSG write log (pce_ref.ingame).
Default script: RUN through attract/intro (as snapshot.py), then stage-1
play: walk right, periodic jumps (I) and staff attacks (II), one RUN
pause/unpause.
"""
import pickle, sys
import pce_ref

DEFAULT = ['350-356:N', '500-506:N', '650-656:N', '800-806:N',
           '1600-2600:R', '2700-3300:L', '2000-2004:N', '2150-2154:N']
DEFAULT += ['%d-%d:1' % (f, f + 8) for f in range(1620, 3300, 90)]
DEFAULT += ['%d-%d:2' % (f, f + 4) for f in range(1665, 3300, 90)]

if __name__ == '__main__':
    out, frames = sys.argv[1], int(sys.argv[2])
    script = sys.argv[3:] or DEFAULT
    r = pce_ref.ingame(frames, script)
    r['script'] = script
    r['frames'] = frames
    pickle.dump(r, open(out, 'wb'))
    print('requests', sorted((f, [hex(x) for x in v]) for f, v in r['requests'].items()))
    print('ticks', len(r['ticks']), 'psg writes', len(r['psg']), 'hold', r['hold'])

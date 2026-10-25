"""objsum.py PAIRS... -- object handlers (banks $17/$07) live per capture,
from play.py's .obj files: bank:handler/hit=frames."""
import pickle, sys
for p in sys.argv[1:]:
    o = pickle.load(open(p + '.obj', 'rb'))
    print(p.rsplit('/', 1)[-1], ' '.join('%02X:%04X/%04X=%d' % (b, h, hh, c)
                                     for (b, h, hh), c in sorted(o.items()) if b in (0x17, 0x07)))

"""peek.py PAIRS FRAME [ADDR[:N] ...] -- print pre/post bytes of one pair
(hex addresses; default: the shot slot arrays and live objects)."""
import struct, sys

f = open(sys.argv[1], 'rb')
magic, n = struct.unpack('<4sI', f.read(8))
want = int(sys.argv[2])
for i in range(n):
    fr, = struct.unpack('<H', f.read(2))
    pre, post = f.read(0x2000), f.read(0x2000)
    if fr == want:
        break
else:
    sys.exit('frame not found')


def show(name, a, k):
    o = a - 0x2000 if a >= 0x2000 else a
    print('%-6s $%04X pre  %s' % (name, a, ' '.join('%02X' % pre[o + j] for j in range(k))))
    print('%-6s       post %s' % ('', ' '.join('%02X' % post[o + j] for j in range(k))))


if len(sys.argv) > 3:
    for it in sys.argv[3:]:
        a, _, k = it.partition(':')
        show('', int(a, 16), int(k or '1'))
else:
    for nm, a in (('state', 0x22E5), ('xh', 0x22EC), ('xl', 0x22F3), ('xf', 0x22FA), ('yh', 0x2301),
                  ('yl', 0x2308), ('yf', 0x230F), ('vxh', 0x2332), ('vxl', 0x2339), ('vyh', 0x2340),
                  ('vyl', 0x2347), ('timer', 0x2363), ('hanh', 0x237F), ('hanl', 0x2386),
                  ('scrh', 0x239B), ('scrl', 0x23A2), ('v0', 0x23A9), ('v1', 0x23B0)):
        show(nm, a, 7)
    show('obj', 0x245A, 32)
    show('bank', 0x275A, 32)
    show('zp', 0x2060, 0x30)

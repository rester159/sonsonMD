"""slots.py PAIRS INDEX [INDEX2] -- live object slots (bank, handlers,
state, position, script) before and after pair INDEX (to INDEX2)."""
import struct, sys
f = open(sys.argv[1], 'rb')
magic, n = struct.unpack('<4sI', f.read(8))
i0 = int(sys.argv[2])
i1 = int(sys.argv[3]) if len(sys.argv) > 3 else i0


def show(tag, r):
    R = lambda a: r[a - 0x2000]
    print(tag, 'pad %02X frame %02X 8D %02X' % (R(0x2016), R(0x2012), R(0x208D)))
    for x in range(32):
        if R(0x245A + x):
            print('  %2d st %02X bank %02X h %02X%02X hit %02X%02X x %02X%02X.%02X y %02X%02X.%02X v %02X%02X,%02X%02X scr %02X%02X t %02X 69A %02X 7A %02X 9A %02X BA %02X DA %02X FA %02X' % (
                x, R(0x245A + x), R(0x275A + x), R(0x28BA + x), R(0x28DA + x), R(0x28FA + x), R(0x291A + x),
                R(0x247A + x), R(0x249A + x), R(0x24BA + x), R(0x24DA + x), R(0x24FA + x), R(0x251A + x),
                R(0x25BA + x), R(0x25DA + x), R(0x25FA + x), R(0x261A + x), R(0x293A + x), R(0x295A + x),
                R(0x267A + x), R(0x269A + x), R(0x297A + x), R(0x299A + x), R(0x29BA + x), R(0x29DA + x), R(0x29FA + x)))


for i in range(n):
    fr, = struct.unpack('<H', f.read(2))
    pre = f.read(0x2000)
    post = f.read(0x2000)
    if i0 <= i <= i1:
        print('== pair', i, 'frame', fr)
        show('pre ', pre)
        if i == i1:
            show('post', post)
    if i > i1:
        break

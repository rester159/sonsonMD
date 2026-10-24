"""cmpbase.py PAIRS -- run out/ss2/hosttest/replay_b5 (port) and replay_base
(stubs) on PAIRS with full reporting and show differing frames per build,
with the addresses involved, so new differences stand out."""
import os, re, subprocess, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../../..'))
env = dict(os.environ, SS2_IRQ_MASK=os.path.join(ROOT, 'out/ss2/irq_mask.bin'))


def run(b, p):
    out = subprocess.run([os.path.join(ROOT, 'out/ss2/hosttest', b), p, '100000'], capture_output=True,
                         text=True, env=env).stdout
    d, unp = {}, {}
    for line in out.splitlines():
        m = re.match(r'frame (\d+): \$([0-9A-F]{4}) pre (\w+) want (\w+) got (\w+)', line)
        if m:
            d.setdefault(int(m.group(1)), []).append('%s:%s/%s' % (m.group(2), m.group(4), m.group(5)))
        m = re.match(r'frame (\d+): unported \$([0-9A-F]{4}) \(bank \$(\w+)', line)
        if m:
            unp[int(m.group(1))] = m.group(2) + '@' + m.group(3)
    return d, unp, out.strip().splitlines()[-1]


for p in sys.argv[1:]:
    a, ua, sa = run('replay_b5', p)
    b, ub, sb = run('replay_base', p) if os.path.exists(os.path.join(ROOT, 'out/ss2/hosttest/replay_base')) else ({}, {}, '')
    print('==', p, '\n  port:', sa, '\n  base:', sb)
    for f in sorted(a):
        tag = 'same-as-base' if f in b else ('base-unported' if f in ub else 'NEW')
        print('  %5d %-13s %s' % (f, tag, ' '.join(a[f][:8])))
    from collections import Counter
    print('  port unported:', Counter(ua.values()).most_common(8))

"""difsum.py REP... -- summarise a hosttest replay report (run with a large
MAXREPORT): frames that differ outside the sound driver's RAM ($2DF0-$2E7B,
$20D6/$20D7, which the host build does not run; music changes show there),
and the addresses involved."""
import re, sys
from collections import Counter

for fn in sys.argv[1:]:
    frames, addrs = {}, Counter()
    total = ''
    for line in open(fn):
        m = re.match(r'frame (\d+): \$([0-9A-F]{4}) pre (..) want (..) got (..)', line)
        if not m:
            if 'pairs' in line:
                total = line.strip()
            continue
        a = int(m.group(2), 16)
        if 0x2DF0 <= a <= 0x2E7B or a in (0x20D6, 0x20D7):
            continue
        frames.setdefault(int(m.group(1)), []).append('%04X' % a)
        addrs['%04X' % a] += 1
    fl = sorted(frames)
    print('%s: %s | %d frames differ outside sound RAM%s' % (
        fn, total, len(fl), (': ' + ' '.join(map(str, fl[:12])) + (' ...' if len(fl) > 12 else '')) if fl else ''))
    if fl:
        print('   ', ' '.join('$%s x%d' % kv for kv in addrs.most_common(12)))

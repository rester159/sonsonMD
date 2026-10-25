"""Validate the Son Son II sound conversion against the original driver.

usage: python3.14 validate.py [--quick] [--ingame out/ss2/sound/ingame.pkl] [-j N]

For every case the ORIGINAL driver code runs in the research model
(pce_ref.py, no MAME) and produces the reference PSG register writes and
note events. Compared against it:

  A. native sequencer (src/ss2_sound_seq.c compiled for the host with
     ASan/UBSan, tools/ss2/sound/host) -- every PSG write (frame, register,
     selected channel, value) in program order, plus note events;
  B. the extractor's independent semantic decoder (extract.timeline) --
     note events (frame, channel, byte) and predicted note periods.

Cases: every request id 0..42 alone, the control ids F0/F1/F2, a mixed
script (music + overlapping sfx of different priority + pause hold + stops),
and optionally the in-game schedule captured by capture_ingame.py.
"""
import argparse, json, os, pickle, subprocess, sys
from multiprocessing import Pool

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pce_ref, extract  # noqa: E402

ROOT = extract.ROOT
HOST = os.path.join(ROOT, 'out', 'ss2', 'sound', 'seqhost')


def frame_lines(schedule, frames, hold=None, ticks=None, direct=False):
    """Host stdin. direct=False emulates the ring (one id popped per frame);
    direct=True passes the schedule's ids as the $8003 calls of that frame."""
    q, out, h = [], [], 0
    for f in range(frames):
        if hold and f in hold:
            h = hold[f]
        if direct:
            ids = list(schedule.get(f, []))
        else:
            q.extend(schedule.get(f, []))
            ids = [q.pop(0)] if q else []
        t = 1 if ticks is None or f in ticks else 0
        out.append('%d %d %d %s' % (t, h, len(ids), ' '.join(str(i) for i in ids)))
    return '\n'.join(out) + '\n'


def run_host(lines, ring=False):
    env = dict(os.environ, SS2_RING='1') if ring else None
    r = subprocess.run([HOST], input=lines, capture_output=True, text=True, check=True, env=env)
    w, e, faults = [], [], None
    for ln in r.stdout.splitlines():
        p = ln.split()
        if p[0] == 'W':
            w.append(tuple(int(x) for x in p[1:]))
        elif p[0] == 'E':
            e.append((int(p[1]), p[2], int(p[3]), int(p[4])))
        elif p[0] == 'F':
            faults = int(p[1])
    return w, e, faults


def cmp_seq(ref, got):
    """(matching prefix length, total ref, total got, first mismatch)."""
    n = min(len(ref), len(got))
    i = 0
    while i < n and tuple(ref[i]) == tuple(got[i]):
        i += 1
    first = None
    if i < max(len(ref), len(got)):
        first = (ref[i] if i < len(ref) else None, got[i] if i < len(got) else None)
    return i, len(ref), len(got), first


def frames_equal(ref, got, frames):
    """Fraction of frames whose complete write list is identical."""
    a, b = {}, {}
    for x in ref:
        a.setdefault(x[0], []).append(x)
    for x in got:
        b.setdefault(x[0], []).append(x)
    ok = sum(1 for f in range(frames) if a.get(f, []) == b.get(f, []))
    return ok


def timeline_check(model_events, schedule, frames, hold=None):
    rom = extract.Rom()
    pred = extract.timeline(rom, schedule, frames, hold)
    m = [(f, k, c, b) for f, k, c, b in model_events]
    p = [(f, k, c, b) for f, c, k, b, _ in pred]
    i, nr, ng, first = cmp_seq(m, p)
    return dict(match=i, model=nr, decoder=ng, first_mismatch=first,
                notes_with_period=sum(1 for x in pred if x[4] is not None))


def period_check(model_periods, schedule, frames, hold=None):
    """Decoder-predicted base period of every pitched, non-tied note vs the
    period the original driver stored in its channel record ($E/$F >> 2,
    read at $8398).  Tied notes keep the previous period in both."""
    rom = extract.Rom()
    pred = [x for x in extract.timeline(rom, schedule, frames, hold) if x[2] == 'm' and x[3] & 0x1F]
    ok = tot = 0
    first = None
    for (f, c, _k, _b, period), (mf, mc, mp) in zip(pred, model_periods):
        if (f, c) != (mf, mc):
            first = first or ('misaligned', (f, c), (mf, mc))
            break
        if period is None:
            continue
        tot += 1
        if (period & 0x3FFF) == mp:      # driver stores period << 2 in 16 bits
            ok += 1
        elif first is None:
            first = (f, c, period, mp)
    if len(pred) != len(model_periods) and first is None:
        first = ('count', len(pred), len(model_periods))
    return ok, tot, first


def case(args):
    name, schedule, frames, hold = args
    ev, per = [], []
    ref, _m = pce_ref.isolated(schedule, frames, hold=hold, events=ev, periods=per)
    ring = name.endswith('(ring)')
    w, e, faults = run_host(frame_lines(schedule, frames, hold, direct=ring), ring=ring)
    wi, wr, wg, wfirst = cmp_seq(ref, w)
    ei, er, eg, efirst = cmp_seq(ev, e)
    tl = timeline_check(ev, schedule, frames, hold)
    pk, pt, pfirst = period_check(per, schedule, frames, hold)
    return dict(name=name, frames=frames, writes_ref=wr, writes_native=wg, writes_match=wi,
                frames_identical=frames_equal(ref, w, frames), write_first_mismatch=wfirst,
                events_ref=er, events_native=eg, events_match=ei, event_first_mismatch=efirst,
                decoder_events=tl, decoder_periods=(pk, pt), period_first_mismatch=pfirst, faults=faults)


def ingame_case(path):
    r = pickle.load(open(path, 'rb'))
    frames = r['frames']
    # one host line per driver entry, in the order the game made them
    lines = ''.join('%d %d %d %s\n' % ((1, h, 0, '') if k == 'T' else (0, h, 1, a))
                    for _f, k, a, h in r['calls'])
    w, e, faults = run_host(lines)
    # The IRQ handler can straddle the model's frame boundary (it starts at
    # the end of VBlank line ~262), so in-game writes are compared in program
    # order without frame tags; frame-tagged agreement is reported separately.
    ref = r['psg']
    wi, wr, wg, wfirst = cmp_seq([x[1:] for x in ref], [x[1:] for x in w])
    ei, er, eg, efirst = cmp_seq([x[1:] for x in r.get('events', [])], [x[1:] for x in e])
    return dict(name='ingame', frames=frames, writes_ref=wr, writes_native=wg, writes_match=wi,
                frames_identical=None, write_first_mismatch=wfirst,
                events_ref=er, events_native=eg, events_match=ei, event_first_mismatch=efirst,
                faults=faults, requests=sum(len(v) for v in r['requests'].values()),
                calls=len(r['calls']))


def cases(quick):
    rom = extract.Rom()
    n = rom.r(extract.SONG_COUNT, False) + 1
    out = []
    for i in range(n):
        hdr = rom.w(extract.SONG_TABLE + 2 * i, False)
        music = rom.r(hdr, False) == 0
        if music and not quick:
            # run until every reachable byte-code address has executed
            # (decoder coverage), then 1200 more frames (>= one loop pass)
            v = {}
            extract.timeline(rom, {0: [i]}, 30000, visit=v)
            frames = max(2400, max(v.values()) + 1200)
        else:
            frames = 600 if music else 400
        out.append(('id %02X %s' % (i, 'music' if music else 'sfx'), {0: [i]}, frames, None))
    out.append(('F0 stop all', {0: [3], 5: [0x1A], 60: [0xF0], 90: [0x15]}, 200, None))
    out.append(('F1 stop sfx', {0: [3], 5: [0x20], 60: [0xF1]}, 200, None))
    out.append(('F2 stop music', {0: [3], 5: [0x20], 60: [0xF2], 80: [4]}, 300, None))
    mixed = {0: [3], 100: [0x16], 110: [0x19], 112: [0x15], 113: [0x16], 160: [0x1F],
             200: [0x28, 0x17, 0x15], 260: [0x1A], 400: [0x0F], 700: [5], 760: [0x2A, 0x25],
             900: [0xF1], 950: [0x20], 1000: [0x16]}
    out.append(('mixed music+sfx+pause', mixed, 1200, {250: 1, 420: 0}))
    out.append(('mixed (ring)', mixed, 1200, {250: 1, 420: 0}))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--quick', action='store_true')
    ap.add_argument('--ingame', default=os.path.join(ROOT, 'out', 'ss2', 'sound', 'ingame.pkl'))
    ap.add_argument('-j', type=int, default=os.cpu_count() or 4)
    ap.add_argument('--only', default=None)
    a = ap.parse_args()
    subprocess.run(['bash', os.path.join(HERE, 'host', 'build.sh')], check=True)
    cs = cases(a.quick)
    if a.only:
        cs = [c for c in cs if a.only in c[0]]
    with Pool(a.j) as pool:
        res = pool.map(case, cs)
    if os.path.exists(a.ingame) and not a.only:
        res.append(ingame_case(a.ingame))
    tw = sum(r['writes_ref'] for r in res)
    tm = sum(r['writes_match'] for r in res)
    print('%-26s %7s %9s %9s %7s %13s %11s %s' % ('case', 'frames', 'ref wr', 'native=', 'events', 'decoder ev', 'periods', 'faults'))
    for r in res:
        d = r.get('decoder_events')
        dp = r.get('decoder_periods')
        print('%-26s %7d %9d %9d %3d/%-3d %13s %11s %s' % (
            r['name'], r['frames'], r['writes_ref'], r['writes_match'], r['events_match'], r['events_ref'],
            '%d/%d' % (d['match'], d['model']) if d else '-',
            '%d/%d' % dp if dp else '-', r['faults']))
        if r['write_first_mismatch']:
            print('   first write mismatch (ref, native):', r['write_first_mismatch'])
        if r['event_first_mismatch']:
            print('   first event mismatch (ref, native):', r['event_first_mismatch'])
        if r.get('period_first_mismatch'):
            print('   first period mismatch:', r['period_first_mismatch'])
        if d and d['first_mismatch']:
            print('   first decoder mismatch (model, decoder):', d['first_mismatch'])
    print('TOTAL PSG writes identical: %d / %d (%.4f%%)' % (tm, tw, 100.0 * tm / max(1, tw)))
    rep = os.path.join(ROOT, 'out', 'ss2', 'sound', 'validate.json')
    json.dump(res, open(rep, 'w'), indent=1, default=str)
    print('report ->', rep)


if __name__ == '__main__':
    main()

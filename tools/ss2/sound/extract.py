"""Son Son II sound extractor (HuCard banks 0x0A/0x0B).

usage: python3.14 extract.py [--json OUT.json] [--c SRC.c] [--quiet]

Decodes every request id of the driver's song table ($89B7, 43 entries)
into a versioned portable intermediate (JSON): music tracks (six channel
command streams with addresses, loops and note semantics), sound effects
(priority, channel mask, event stream), instruments (32-sample wave +
envelope/LFO parameters) and the period table.  It also

  * walks every reachable byte-code path (both edges of conditional loops)
    and fails on undefined opcodes or out-of-range pointers,
  * emits src/ss2_sound_data.c: the period table, the bounded original data
    region the native sequencer interprets, and one YM2612 additive patch
    per waveform (4 sine partials fitted to the wave's spectrum),
  * provides `timeline(id, frames)`: an independent semantic decoder that
    predicts the driver's note events (frame, channel, note byte, period)
    without executing driver code; validate.py compares it with the events
    the original driver produces in the research model.

Driver contract and command semantics: docs/ss2/SOUND.md.
"""
import argparse, hashlib, json, math, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
ROM_PATH = os.path.join(ROOT, 'arcade', 'Son_Son_II_J_TEng.pce')
SCHEMA = 'ss2-sound/1'

FTAB = 0x81C8            # 96 x u16 periods (C1..B8), read with an 8-bit index
SONG_COUNT = 0x89B4      # entries - 1
INST_BASE = 0x89B5       # u16 -> $8A0D, 40-byte instrument records
SONG_TABLE = 0x89B7      # u16 header pointers
SFX_WAVES = 0x890A       # 5 x u16 -> 32-byte waves for sfx channels
BANK_BASE = 0x890A       # first data byte (code ends at $8909)
NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']

MUSIC_OPS = {   # opcode: (name, arg bytes)
    0x00: ('tempo', 1), 0x01: ('gate', 1), 0x02: ('volume', 1), 0x03: ('instrument', 1),
    0x04: ('triplet_toggle', 0), 0x05: ('tie_toggle', 0), 0x06: ('dot_next', 0),
    0x07: ('octave', 1), 0x08: ('octave_plus2_on', 0), 0x09: ('octave_plus2_off', 0),
    0x0A: ('transpose', 1), 0x0B: ('detune', 1), 0x0C: ('slide', 1),
    0x0D: ('repeat', 3), 0x0E: ('break_last', 2), 0x0F: ('jump', 2), 0x10: ('end', 0),
    0x11: ('noise_toggle', 0), 0x12: ('balance', 1),
}
SFX_OPS = {0x00: ('wait', 1), 0x01: ('loop', 3), 0x02: ('end', 0), 0x03: ('end', 0),
           0x04: ('balance', 1)}


class DecodeError(Exception):
    pass


class Rom:
    def __init__(self, path=ROM_PATH):
        self.data = open(path, 'rb').read()
        self.sha1 = hashlib.sha1(self.data).hexdigest()
        self.bank = self.data[0xA * 0x2000:0xC * 0x2000]
        self.used = set()

    def r(self, a, mark=True):
        if not 0x8000 <= a <= 0xBFFF:
            raise DecodeError('read outside banks 0A/0B: $%04X' % a)
        if mark:
            self.used.add(a)
        return self.bank[a - 0x8000]

    def w(self, a, mark=True):
        return self.r(a, mark) | (self.r(a + 1, mark) << 8)

    def ftab(self, idx):
        idx &= 0xFF
        return self.r(FTAB + idx, False) | (self.r(FTAB + idx + 1, False) << 8)


def s8(v):
    return v - 256 if v & 0x80 else v


def note_name(semi):
    return '%s%d' % (NOTE_NAMES[semi % 12], semi // 12 + 1)


# ---- static decoding -------------------------------------------------------
def decode_music_channel(rom, start):
    """Reachable command list of one music channel (both loop edges)."""
    seen, out, todo = {}, [], [start]
    while todo:
        pc = todo.pop()
        while pc not in seen:
            op = rom.r(pc)
            if op >= 0x1F:
                ev = dict(addr=pc, op='note', byte=op, length_code=op >> 5,
                          pitch=op & 0x1F, text='rest' if not op & 0x1F else 'note %d' % (op & 0x1F))
                seen[pc] = ev
                pc += 1
                continue
            if op not in MUSIC_OPS:
                raise DecodeError('undefined music opcode $%02X at $%04X' % (op, pc))
            name, n = MUSIC_OPS[op]
            args = [rom.r(pc + 1 + i) for i in range(n)]
            ev = dict(addr=pc, op=name, args=args)
            seen[pc] = ev
            nxt = pc + 1 + n
            if name in ('repeat', 'break_last', 'jump'):
                tgt = args[-2] | (args[-1] << 8)
                ev['target'] = tgt
                todo.append(tgt)
                if name == 'jump' or (name == 'repeat' and args[0] == 0):
                    break           # repeat with count 0 is an unconditional jump
            if name == 'end':
                break
            pc = nxt
    for a in sorted(seen):
        out.append(seen[a])
    return out


def decode_sfx(rom, start):
    seen, todo = {}, [start]
    while todo:
        pc = todo.pop()
        while pc not in seen:
            op = rom.r(pc)
            if op & 0x80:
                b = [rom.r(pc + 1 + i) for i in range(6)]
                ev = dict(addr=pc, op='event', noise=op & 1, freq_index=b[0], period_add=s8(b[1]),
                          slide=s8(b[2]), wave=b[3], volume=None if b[4] == 0xFF else b[4] & 0x1F,
                          volume_slide=s8(b[5]), raw=[op] + b)
                if b[3] >= 5 and not op & 1:
                    raise DecodeError('sfx wave %d out of range at $%04X' % (b[3], pc))
                seen[pc] = ev
                pc += 7
                continue
            if op not in SFX_OPS:
                raise DecodeError('undefined sfx opcode $%02X at $%04X' % (op, pc))
            name, n = SFX_OPS[op]
            args = [rom.r(pc + 1 + i) for i in range(n)]
            ev = dict(addr=pc, op=name, args=args)
            seen[pc] = ev
            if name == 'loop':
                tgt = args[1] | (args[2] << 8)
                ev['target'] = tgt
                todo.append(tgt)
                if args[0] == 0:
                    break
            if name == 'end':
                break
            pc += 1 + n
    return [seen[a] for a in sorted(seen)]


def instrument(rom, i):
    base = rom.w(INST_BASE) + 40 * i
    wave = [rom.r(base + k) for k in range(32)]
    p = [rom.r(base + 32 + k) for k in range(8)]
    return dict(index=i, addr=base, wave=wave, attack=p[0], decay=p[1], sustain=p[2],
                release=p[3], lfo_period=p[4] & 0x7F, lfo_retrigger=p[4] >> 7,
                vibrato_delta=s8(p[5]), tremolo_delta=s8(p[6]), unused=p[7])


def decode_all(rom):
    n = rom.r(SONG_COUNT) + 1
    tracks, insts, sfx_waves = [], set(), set()
    for i in range(n):
        hdr = rom.w(SONG_TABLE + 2 * i)
        pri = rom.r(hdr)
        if pri == 0:
            chans = []
            for c in range(6):
                st = rom.w(hdr + 1 + 2 * c)
                if st == 0:
                    chans.append(None)
                    continue
                evs = decode_music_channel(rom, st)
                for e in evs:
                    if e['op'] == 'instrument':
                        insts.add(e['args'][0])
                chans.append(dict(start=st, events=evs))
            tracks.append(dict(id=i, kind='music', header=hdr, channels=chans))
        else:
            mask = rom.r(hdr + 1)
            evs = decode_sfx(rom, hdr + 2)
            for e in evs:
                if e['op'] == 'event' and not e['noise']:
                    sfx_waves.add(e['wave'])
            tracks.append(dict(id=i, kind='sfx', header=hdr, priority=pri, mask=mask,
                               channels=[c for c in range(6) if mask & (0x20 >> c)],
                               events=evs))
    insts = sorted(insts)
    inst_recs = [instrument(rom, i) for i in insts]
    waves = []
    for k in range(5):
        a = rom.w(SFX_WAVES + 2 * k)
        waves.append(dict(index=k, addr=a, wave=[rom.r(a + j) for j in range(32)],
                          used=k in sfx_waves))
    for k in range(n):
        rom.w(SONG_TABLE + 2 * k)
    rom.r(SONG_COUNT); rom.w(INST_BASE)
    return tracks, inst_recs, waves


# ---- independent semantic timeline (no driver code executed) ---------------
class _Chan:
    pass


def timeline(rom, schedule, frames, hold=None, loops=None, visit=None):
    """Predict note events for a request schedule {frame: [ids]} (one popped
    per frame, like the IRQ ring).  Returns list of (frame, channel, kind,
    byte, period) where kind is 'm' (music note/rest byte) or 's' (sfx event).
    Models tempo, lengths, gate is irrelevant to events, loops, tie, octave,
    transpose, detune, sfx waits/loops/priority, and the $6B hold flag.  Envelopes, LFO and register
    output are deliberately NOT modelled (they are checked at register level
    by the native-sequencer comparison)."""
    n = rom.r(SONG_COUNT, False) + 1
    ev = []
    st = dict(acc=0, step=0, last=0xFF, pri=0, mask=0, wait=0, parsed=0, loop=0, sp=0)
    ch = [None] * 6
    queue = []
    fr = [0]

    def music_start(hdr):
        st.update(acc=0, step=0, last=0xFF)
        for c in range(6):
            p = rom.w(hdr + 1 + 2 * c, False)
            if p:
                k = _Chan()
                k.ptr, k.dur, k.flags, k.octave, k.tr, k.det, k.loop = p, 0, 0, 0, 0, 0, 0
                ch[c] = k
            else:
                ch[c] = None

    def stop_sfx():
        st.update(pri=0, mask=0, wait=0, loop=0)

    def request(a):
        if a >= 0xF0:
            k = a & 3
            if k in (0, 2, 3):
                st.update(acc=0, step=0, last=0xFF)
                for c in range(6):
                    ch[c] = None
            if k in (0, 1):
                stop_sfx()
            return
        a %= n
        hdr = rom.w(SONG_TABLE + 2 * a, False)
        pri = rom.r(hdr, False)
        if pri == 0:
            music_start(hdr)
        elif pri >= st['pri']:
            stop_sfx()
            st['pri'] = pri
            st['mask'] = rom.r(hdr + 1, False)
            st['sp'] = hdr + 2

    def nb(k):
        v = rom.r(k.ptr, False)
        k.ptr = (k.ptr + 1) & 0xFFFF
        return v

    def chan_tick(c, k):
        if st.get('hold'):
            return
        if k.dur and (k.dur - 1) & 0xFF:
            k.dur -= 1
            return
        while True:
            if visit is not None:
                visit.setdefault(k.ptr, fr[0])
            a = nb(k)
            if a >= 0x1F:
                break
            if a == 0x00:
                t = nb(k)
                st['step'] = 0x3000 // t
                st['acc'] = 0
                st['last'] = 0
            elif a in (0x01, 0x02, 0x03, 0x0A, 0x0B, 0x0C, 0x07, 0x12):
                v = nb(k)
                if a == 0x07: k.octave = (k.octave & 0x80) | v
                elif a == 0x0A: k.tr = v
                elif a == 0x0B: k.det = v
            elif a == 0x04: k.flags ^= 0x08
            elif a == 0x05: k.flags ^= 0x02
            elif a == 0x06: k.flags |= 0x01
            elif a == 0x08: k.octave |= 0x80
            elif a == 0x09: k.octave &= 0x7F
            elif a == 0x11: k.flags ^= 0x80
            elif a in (0x0D, 0x0E, 0x0F):
                if a == 0x0D:
                    cnt = nb(k)
                    take = True
                    if cnt:
                        if not k.loop:
                            k.loop = cnt
                        else:
                            k.loop -= 1
                            take = k.loop != 0
                elif a == 0x0E:
                    take = k.loop == 1
                    if take:
                        k.loop = 0
                else:
                    take = True
                if take:
                    lo = nb(k); hi = nb(k)
                    if a == 0x0F and loops is not None:
                        loops.append((fr[0], c, lo | (hi << 8)))
                    k.ptr = lo | (hi << 8)
                else:
                    k.ptr += 2
            elif a == 0x10:
                ch[c] = None
                return
            else:
                raise DecodeError('opcode $%02X' % a)
        # note / rest: length in ticks
        code = a >> 5
        base = 2 if k.flags & 8 else 3
        prev, x = 0, code
        while True:
            x = (x - 1) & 0xFF
            if not x:
                break
            prev, base = base, (base << 1) & 0xFF
        if k.flags & 1:
            base = (base + prev) & 0xFF
            k.flags &= ~1
        k.dur = base
        p = a & 0x1F
        period = None
        if p:
            tied = k.flags & 4
            k.flags = (k.flags | 4) if k.flags & 2 else (k.flags & ~4)
            if not tied:
                if k.flags & 0x80:
                    period = p - 1
                else:
                    o = k.octave
                    if o & 0x80:
                        o = ((o & 0x7F) + 0x18) & 0xFF
                    semi = (o + k.tr + p - 1) & 0xFF
                    period = (rom.ftab(semi << 1) + s8(k.det)) & 0xFFFF
                    if s8(k.det) < 0 and False:
                        pass
                ev.append((fr[0], c, 'm', a, period))
            else:
                ev.append((fr[0], c, 'm', a, None))
        else:
            ev.append((fr[0], c, 'm', a, None))

    def snb():
        v = rom.r(st['sp'], False)
        st['sp'] = (st['sp'] + 1) & 0xFFFF
        return v

    def sfx_tick():
        if not st['mask']:
            return
        mask = st['mask']
        for slot in range(6):
            c = 5 - slot
            if not mask & (1 << slot):
                continue
            if not st['parsed'] and st['wait']:
                continue
            while True:
                a = snb()
                if a & 0x80:
                    break
                if a == 0:
                    st['wait'] = snb(); st['parsed'] = 1
                elif a == 1:
                    x = snb()
                    take = True
                    if x:
                        if not st['loop']:
                            st['loop'] = x
                        else:
                            st['loop'] = (st['loop'] - 1) & 0xFF
                            take = st['loop'] != 0
                    if take:
                        lo = snb(); hi = snb(); st['sp'] = lo | (hi << 8)
                    else:
                        st['sp'] += 2
                elif a in (2, 3):
                    stop_sfx()
                    return
                elif a == 4:
                    snb()
                else:
                    raise DecodeError('sfx opcode $%02X' % a)
            b = [snb() for _ in range(6)]
            ev.append((fr[0], c, 's', a, b[0]))
        st['wait'] = (st['wait'] - 1) & 0xFF
        st['parsed'] = 0

    for f in range(frames):
        fr[0] = f
        if hold and f in hold:
            st['hold'] = hold[f]
        queue.extend(schedule.get(f, []))
        if queue:
            request(queue.pop(0))
        sfx_tick()
        st['acc'] = (st['acc'] + st['step']) & 0xFFFF
        while (st['acc'] >> 8) != st['last']:
            st['last'] = (st['last'] + 1) & 0xFF
            for c in range(6):
                if ch[c] is not None:
                    chan_tick(c, ch[c])
    return ev


# ---- FM patch derivation ---------------------------------------------------
def fit_patch(wave):
    """4-operator additive (algorithm 7) approximation of a 32-sample wave.
    Returns dict(mul=[4], tl=[4], retained=fraction of AC power, rms)."""
    x = [v - sum(wave) / 32.0 for v in wave]
    power = sum(v * v for v in x) / 32.0
    amps = []
    for h in range(1, 16):          # YM MUL 1..15
        re = sum(x[i] * math.cos(2 * math.pi * h * i / 32) for i in range(32))
        im = sum(x[i] * math.sin(2 * math.pi * h * i / 32) for i in range(32))
        amps.append((2.0 * math.hypot(re, im) / 32.0, h))
    amps.sort(reverse=True)
    top = [a for a in amps[:4] if a[0] > 0]
    if not top or power == 0:
        return dict(mul=[1, 1, 1, 1], tl=[127] * 4, retained=0.0, rms=0.0)
    kept = sum(a * a / 2 for a, _ in top)
    # level reference: a full-scale PCE square (+-15.5) has fundamental
    # amplitude 4/pi*15.5; that maps to TL 0 on one carrier, with 6 dB of
    # headroom (TL +8) for the 4-carrier sum.
    ref = 4 / math.pi * 15.5
    mul, tl = [], []
    for a, h in sorted(top, key=lambda t: t[1]):
        db = -20 * math.log10(a / ref)
        t = int(round(db / 0.75)) + 8
        mul.append(h)
        tl.append(max(0, min(127, t)))
    while len(mul) < 4:
        mul.append(1); tl.append(127)
    return dict(mul=mul, tl=tl, retained=round(kept / power, 4), rms=round(math.sqrt(power), 3))


# ---- C emission ---------------------------------------------------------------
def emit_c(rom, tracks, insts, waves, path):
    lo = BANK_BASE
    hi = max(a for a in rom.used if a >= BANK_BASE) + 1
    blob = rom.bank[lo - 0x8000:hi - 0x8000]
    ftab = rom.bank[FTAB - 0x8000:FTAB - 0x8000 + 258]
    patches, srcs = [], []
    for w in [dict(addr=i['addr'], wave=i['wave'], tag='instrument %d' % i['index']) for i in insts] + \
             [dict(addr=w['addr'], wave=w['wave'], tag='sfx wave %d' % w['index']) for w in waves]:
        if w['addr'] in srcs:
            continue
        p = fit_patch(w['wave'])
        p['tag'] = w['tag']
        srcs.append(w['addr'])
        patches.append(p)
    L = []
    L.append('/* GENERATED by tools/ss2/sound/extract.py -- do not edit.')
    L.append(' * Source: %s (sha1 %s)' % (os.path.basename(ROM_PATH), rom.sha1))
    L.append(' * Son Son II sound data: HuCard bank 0x0A $%04X..$%04X (original driver byte code,' % (lo, hi - 1))
    L.append(' * interpreted natively by src/ss2_sound_seq.c), the period table, and one')
    L.append(' * YM2612 additive patch per PCE waveform.  Schema/semantics: docs/ss2/SOUND.md. */')
    L.append('#include "ss2_sound_seq.h"')
    L.append('#include "ss2_sound_data.h"')
    L.append('')
    L.append('const u8 ss2_snd_ftab[258] = {')
    for i in range(0, 258, 16):
        L.append('    ' + ','.join('0x%02X' % v for v in ftab[i:i + 16]) + ',')
    L.append('};')
    L.append('')
    L.append('const u16 ss2_snd_bank_len = %d;' % len(blob))
    L.append('const u8 ss2_snd_bank[%d] = {' % len(blob))
    for i in range(0, len(blob), 16):
        L.append('    ' + ','.join('0x%02X' % v for v in blob[i:i + 16]) + ',  /* $%04X */' % (lo + i))
    L.append('};')
    L.append('')
    L.append('const u16 ss2_snd_patch_count = %d;' % len(patches))
    L.append('const u16 ss2_snd_wave_src[%d] = {' % len(patches))
    L.append('    ' + ','.join('0x%04X' % a for a in srcs))
    L.append('};')
    L.append('const Ss2FmPatch ss2_snd_patch[%d] = {' % len(patches))
    for p in patches:
        L.append('    {{%s}, {%s}},  /* %s: %.0f%% of AC power in 4 partials */' % (
            ','.join(str(m) for m in p['mul']), ','.join(str(t) for t in p['tl']),
            p['tag'], 100 * p['retained']))
    L.append('};')
    open(path, 'w').write('\n'.join(L) + '\n')
    return dict(blob=len(blob), lo=lo, hi=hi, patches=patches, srcs=srcs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--json', default=os.path.join(ROOT, 'out', 'ss2', 'sound', 'ss2_sound.json'))
    ap.add_argument('--c', default=os.path.join(ROOT, 'src', 'ss2_sound_data.c'))
    ap.add_argument('--quiet', action='store_true')
    a = ap.parse_args()
    rom = Rom()
    tracks, insts, waves = decode_all(rom)
    info = emit_c(rom, tracks, insts, waves, a.c)
    for t in tracks:
        if t['kind'] == 'music':
            for c in t['channels']:
                if c:
                    for e in c['events']:
                        if e['op'] == 'note' and e['pitch']:
                            e['pitch_note'] = e['pitch']
    doc = dict(schema=SCHEMA, rom=os.path.basename(ROM_PATH), rom_sha1=rom.sha1,
               driver=dict(request='$8003', tick='$8000', song_table='$89B7', count=len(tracks),
                           instrument_base='$%04X' % rom.w(INST_BASE, False), period_table='$81C8',
                           period_hz='3579545 / (32 * period)', tempo='ticks/frame = 0x3000 / T / 256'),
               period_table=[rom.ftab(2 * i) for i in range(96)],
               instruments=insts, sfx_waves=waves, tracks=tracks,
               patches=[dict(src='$%04X' % s, **p) for s, p in zip(info['srcs'], info['patches'])],
               data_region=dict(start='$%04X' % info['lo'], end='$%04X' % (info['hi'] - 1),
                                bytes=info['blob'], used_bytes=len([u for u in rom.used if u >= BANK_BASE])))
    os.makedirs(os.path.dirname(a.json), exist_ok=True)
    json.dump(doc, open(a.json, 'w'), indent=1)
    if not a.quiet:
        nm = sum(1 for t in tracks if t['kind'] == 'music')
        print('tracks %d (music %d, sfx %d), instruments %d, sfx waves used %d, patches %d' % (
            len(tracks), nm, len(tracks) - nm, len(insts), sum(w['used'] for w in waves), len(info['patches'])))
        print('data region $%04X..$%04X = %d bytes (%d reachable bytes)' % (
            info['lo'], info['hi'] - 1, info['blob'], doc['data_region']['used_bytes']))
        nev = sum(len(c['events']) for t in tracks if t['kind'] == 'music' for c in t['channels'] if c)
        nev += sum(len(t['events']) for t in tracks if t['kind'] == 'sfx')
        print('decoded commands', nev, '->', a.json)
        print('C ->', a.c)


if __name__ == '__main__':
    main()

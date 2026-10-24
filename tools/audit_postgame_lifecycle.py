#!/usr/bin/env python3
"""Byte-pinned original post-game contracts; not an original CPU replay."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
rom = b''.join((root / 'arcade' / n).read_bytes()
               for n in ('ss.01e', 'ss.02e', 'ss.03e'))
assert hashlib.sha256(rom).hexdigest() == '09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
pins = {
    'input_handler_start_order': (0x9d60, 'bd9a2cd608588e9d6fad957e92b0'),
    'completion_before_idle': (0x9dfa, 'bd9e01bd9e3739'),
    'both_done_increment': (0x9e28, 'bd9ea68e0380e602e40e27020c0b39'),
    'idle_expiry_increment': (0x9ea0, '7f03780c0b39'),
    'individual_hold_180': (0xa28b, 'cc00b4dd060c0b39'),
    'individual_countdown': (0xa293, '9e06301f9f0626020c0b39'),
    'total_hold_180': (0xa2ad, 'cc00b4dd060c0b39'),
    'total_countdown': (0xa2b5, '9e06301f9f062607bd63f60c080f0b39'),
    'total_ranking_source': (0x9cd5, '8e0074108e0328bd9d24'),
    'decimal_sum': (0x4d5a, '34408e0070ce0074108e0078a682abc219a7a2a682a9c219a7a2a682a9c219a7a2a682a9c219a7a235c0'),
}
for name, (address, raw) in pins.items():
    expected = bytes.fromhex(raw)
    assert rom[address-0x4000:address-0x4000+len(expected)] == expected, name
report = {
    'passed': True,
    'source_sha256': hashlib.sha256(rom).hexdigest(),
    'signatures': {k: {'address': hex(a), 'bytes': b} for k, (a, b) in pins.items()},
    'contracts': [
        '9D60 samples input, dispatches the current handler, then checks held Start.',
        '9DFA runs completion before idle; each may independently increment 0B.',
        'A28B/A2AD load 180; subsequent handlers decrement the full 16-bit word once per call.',
        '4D5A adds packed-decimal P1[6C:6F] and P2[70:73] into total[74:77], discarding final carry.',
        '9CD5 qualifies that total against the total ranking table.',
    ],
    'limits': [
        'Source-byte/manual control-flow evidence, not original CPU execution.',
        'Sum equivalence assumes valid packed decimal and current total recomputation; call timing and continue/reset ownership remain unverified.',
        'Native binary scores are not yet proven to wrap at every source scoring operation.',
        'Native completion/timeout coalesces the two increments; simultaneous source behavior remains unresolved.',
        'Native setup collapses original queue-wait substates; no exact lifecycle parity claim.',
    ],
}
out = root / 'out/parity-audit/postgame-lifecycle'
out.mkdir(parents=True, exist_ok=True)
(out / 'report.json').write_text(json.dumps(report, indent=2)+'\n')
print(f'{len(pins)} original post-game signatures verified; integration gaps remain open')

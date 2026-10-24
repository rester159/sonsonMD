#!/usr/bin/env python3
"""Pin front-end transitions to original bytes, without claiming CPU execution.

This is a source contract audit, not a native parity test. Timer durations are
handler calls; IRQ/Freeze and rendering-queue waits must be integrated separately.
"""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROM = b''.join((ROOT / 'arcade' / n).read_bytes()
               for n in ('ss.01e', 'ss.02e', 'ss.03e'))
SHA = hashlib.sha256(ROM).hexdigest()
assert SHA == '09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
signatures = {
    'dispatch_then_credit_change_then_start': (0x9296, '9605488e92a4ad96bd931e8d0d39'),
    'handlers': (0x92a4, '933e9348949294b694d59574'),
    'credit_change': (0x931e, 'dc251093272718dd27cc0338bd9604cc0700bd9604c605d7050f080f0b0f0e39'),
    'new_game_resets_exclude_rng': (0x8f5b, '4f5fdd65dd90d7675fd75dd761d77ed77cd77dc6ffd77b'),
    'demo_ignores_dip_difficulty': (0x8f80, 'd6540d2226015fd75a'),
    'seed_then_prelude': (0x933e, 'cc87cddd780f7a0c0539'),
    'prelude_disable_shots': (0x93cf, '4f5fed11ed31e70ce72c'),
    'fire_available_guard': (0xad10, 'a60c273e'),
    'prelude_update_order': (0x9463, 'bd9a2cbda98abdb19abde7d6bde7dfbdfecfbdff1abdff3dbdff09'),
    'prelude_timer_load': (0x945b, 'cc000add060c0839'),
    'prelude_timer_tick': (0x947e, 'd624c43f260d9e06301f9f0626050c050f083939'),
    'demo_timer_load': (0x9492, '8e001e9f03'),
    'demo_timer_tick': (0x94b6, 'bd9646d624c43f26f69e03301f9f0326eebd95da0fe60c050f080f0b0f0e39'),
    'post_demo_handlers': (0x94de, '94e894f995229533955c'),
    'first_post_demo_timer': (0x951b, '86f097070c0839'),
    'first_post_demo_expiry': (0x9522, '0a07260c860197e8cc0119bd96040c0839'),
    'second_post_demo_timer': (0x9555, '86f097070c0839'),
    'second_post_demo_expiry': (0x955c, '0a07261396259a269a5126050f080f05390c050f080f0b39'),
    'credited_handlers': (0x957c, '9582959a95cd'),
    'credited_timer_load': (0x95c5, 'cc00f0dd060c0839'),
    'credited_timer_expiry': (0x95cd, '9e06301f9f0626040f050f0839'),
}
for name, (address, hexbytes) in signatures.items():
    data = bytes.fromhex(hexbytes)
    assert ROM[address-0x4000:address-0x4000+len(data)] == data, (name, hex(address))

report = {
    'source_sha256': SHA,
    'evidence_scope': 'Byte-pinned manual control-flow interpretation; no original CPU or native execution.',
    'signatures': {name: {'address': hex(addr), 'bytes': data}
                   for name, (addr, data) in signatures.items()},
    'dispatch_order': ['current state handler', 'credit-change detection', 'held Start purchase'],
    'states': [
        {'state': 0, 'handler': '933E', 'action': 'Seed 78:79=87CD, clear 7A, advance to state 1'},
        {'state': 1, 'handler': '9348', 'action': 'Prelude setup and actors; countdown 10 at clock24 & 63 == 0; advance to 2'},
        {'state': 2, 'handler': '9492', 'action': 'Initialize demo streams and timer=30; advance to 3'},
        {'state': 3, 'handler': '94B6', 'action': 'Run 9646; countdown at clock24 & 63 == 0; clear sprites and advance to 4'},
        {'state': 4, 'handler': '94D5', 'action': 'Two queued presentation sequences, each followed by 240 handler calls; go to 5 if credits or free play, otherwise 0'},
        {'state': 5, 'handler': '9574', 'action': 'Wait for queue readiness, enqueue credited presentation, load 240; countdown per handler call then return to 0 unconditionally'},
    ],
    'prelude_firing': '93CF clears both player availability bytes at +0C. AD10 returns immediately when availability is zero. 9463 calls no projectile update or projectile render handler; synthetic auto-fire bits must not create prelude shots.',
    'credit_change': 'After state dispatch, unequal credit words 25:26 and 27:28 copy current credit and select state 5, clearing 08/0B/0E. A subsequent unchanged nonzero balance does not retrigger this.',
    'critical_conclusion': 'Credits do not suppress demos indefinitely. State 5 returns to state 0 without a credit test; state 0 then starts the prelude and demo sequence.',
    'native_gaps': [
        'Six-state lifecycle is connected; exact original queue completion timing remains unverified.',
        'Prelude/gameplay retained player/input/world state handoffs need full source comparison.',
        'Captured title background remains an adaptation rather than complete source command reconstruction.',
    ],
    'next_implementation': 'Compare integrated lifecycle source timing and retained state; sustain both player modes at the target presentation rate.',
    'unverified': ['Queue command visual meanings and completion timing', 'Natural full lifecycle trace including coin interruptions', 'Native integrated parity'],
}
out = ROOT / 'out/parity-audit/title-lifecycle'
out.mkdir(parents=True, exist_ok=True)
(out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print(f'{len(signatures)} original-byte signatures verified; native lifecycle remains FAIL')
print(report['critical_conclusion'])

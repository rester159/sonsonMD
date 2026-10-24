#!/usr/bin/env python3
"""Pin original demo-input setup and dispatch evidence; do not infer full timing."""
from pathlib import Path
import hashlib
import json
root=Path(__file__).resolve().parents[1]
rom=b''.join((root/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
digest=hashlib.sha256(rom).hexdigest()
assert digest=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def require(address,hexbytes):
    expected=bytes.fromhex(hexbytes)
    assert rom[address-0x4000:address-0x4000+len(expected)]==expected, hex(address)
require(0x9492,'8e001e9f034f5fdd440f480f498e9a6e9f4a8e9b149f4cc601d720')
require(0x94b6,'bd9646d624c43f26f69e03301f9f0326ee')
require(0x9a2c,'dc44dd460d2227079640d642dd4439')
require(0x9a3b,'ce0044108e004a8d09334131228d038d1339')
require(0x9a4d,'e644260bae a4ec81a7c4e744afa4396a4439'.replace(' ',''))
require(0x9a5f,'d624c4072608dc44ca018a01dd4439')
streams=[]
for start,end in ((0x9a6e,0x9b14),(0x9b14,0x9bb6)):
    data=rom[start-0x4000:end-0x4000]
    pairs=[{'input':data[i],'delay':data[i+1]} for i in range(0,len(data),2)]
    streams.append({'start':start,'next_known_table':end,'pairs':pairs,
                    'updates_to_consume_span':sum(p['delay']+1 for p in pairs)})
source=(root/'src/game.c').read_text()
report={
    'source_sha256':digest,
    'scope':'Pinned source bytes and straight-line data-flow inspection, not original-CPU execution or natural demo timing.',
    'setup':{'address':0x9492,'timer_word':0x03,'timer_initial':30,
             'inputs_cleared':[0x44,0x45],'delays_cleared':[0x48,0x49],
             'stream_pointers':[0x4a,0x4c],'session_selector':{'address':0x20,'value':1}},
    'dispatch':{'address':0x9a2c,'live_input_gate':0x22,
                'previous_inputs':[0x46,0x47],
                'script_rule':'If delay is nonzero, decrement only; otherwise load input/delay pair and advance pointer by two. Every eight source-clock ticks OR fire bit1 into both current inputs. Previous inputs are copied before either operation.',
                'timeout_rule':'94B6 calls 9646 including setup states, then decrements timer03 only when source24 & 63 is zero.'},
    'streams':streams,
    'native_evidence':{'forces_one_player_demo':'g_numPlayers >= 2 && !g_attract' in source,
                       'synthetic_autopilot':'(t % 200)' in source and '(t % 16)' in source,
                       'demo_cap_2400':'#define ATTRACT_MAX_FRAMES  2400' in source,
                       'two_player_script_adapter':'g_attract ? attract_pad(i)' in source,
                       'phased_timeout':'--demoRemaining==0' in source},
    'unresolved':['Natural setup duration and source clock phase at first gameplay input',
                  'Number of stream reads before source timeout; adjacent table boundaries are not terminators',
                  'Prelude and main demo presentation, initial world state, and demo-sound DIP behavior'],
    'integration_gate':'FAIL: source inputs and phased timeout are connected, but prelude/setup clock phase, world state and interruptions remain unverified.'
}
out=root/'out/parity-audit/attract-contract'
out.mkdir(parents=True,exist_ok=True)
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='streams'},indent=2))

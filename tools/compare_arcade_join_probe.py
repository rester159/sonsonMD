#!/usr/bin/env python3
"""Bounded real-input evidence for the console-only mid-game join requirement."""
from pathlib import Path
import csv,hashlib,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit';trace=o/'join-probe.csv'
rows=[{k:int(v)for k,v in d.items()}for d in csv.DictReader(trace.open())]
assert 'JOIN_PROBE_DONE' in (o/'join-probe/mame.log').read_text()
assert len(rows)==301 and [d['tick']for d in rows]==list(range(301))
assert all(d['mode']==0 and d['out1']==0 and d['out2']==1 and d['active1']==1 and d['active2']==0 for d in rows)
assert rows[0]['credits']==0 and rows[-1]['credits']==1
for begin,end in ((100,120),(200,220)):
 assert any(d['system']&2 for d in rows[begin:end+3])
 assert all(d['credits']==1 for d in rows[begin:end+3])
assert any(d['system']&16 for d in rows[40:63])
sha=lambda b:hashlib.sha256(b).hexdigest()
program=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert sha(program)=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
report={'program_sha256':sha(program),'trace_sha256':sha(trace.read_bytes()),'observations':len(rows),'coin_accepted':True,'start2_pulses':2,'player2_joined':False,'scope':'Original 1P gameplay, one extra coin and two 2P Start presses over300refreshes; hostile records cleared, player records untouched. Not an exhaustive start/continue lifecycle proof.'}
(o/'join-probe-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

#!/usr/bin/env python3
"""Verify the complete $9094 write contract against original MC6809 execution."""
from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
sha=lambda b:hashlib.sha256(b).hexdigest()
program=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert sha(program)=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
trace=o/'start-oracle.jsonl';rows=[json.loads(s)for s in trace.read_text().splitlines()]
assert 'START_DONE' in (o/'start-source.log').read_text() and len(rows)==162
for d in rows:
 b=bytearray.fromhex(d['before'])
 def put(address,value,width=1):
  i=address-0x47e;b[i:i+width]=value.to_bytes(width,'big')
 # P2's active/kind/owner are always assigned, even when it will not start.
 put(0x4b0,0);put(0x4b3,1,2)
 for owner in (0,1):
  enabled=d['p1out']==0 if owner==0 else d['mode']!=0 and d['p2out']==0
  if not enabled:continue
  p=0x490+owner*48
  put(p+14,0x4c0 if owner==0 else 0x490,2)
  put(p-16,1);put(p-15,0,2)
  if owner==0:put(p-13,0,2)
  put(p-11,(d['camera']+32)&65535,2);put(p-8,104+owner*32)
  put(p-6,1);put(p-2,0xafac+owner*36,2);put(p-3,0)
  put(p-18,0x4e0+owner*64,2);put(p+12,2)
 actual=bytes.fromhex(d['after'])
 assert b==actual,{'case':d,'differences':[(hex(0x47e+i),x,y)for i,(x,y)in enumerate(zip(b,actual))if x!=y]}
report={'program_sha256':sha(program),'trace_sha256':sha(trace.read_bytes()),'cases':len(rows),'byte_checks':len(rows)*98,'differences':0,'scope':'Injected original $9094 initialization; full 98-byte region including retained bytes, two owners, eligibility, camera wrap. Call-site/start/join reachability and native binding separate.'}
(o/'start-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

#!/usr/bin/env python3
"""Bound normal C movement before course-end stop2 from pinned arcade data.

This checks a normal uninterrupted course after the startup9C delay has expired.
Bonus conversion never returns to moving state1. Boot/reentry traces remain a
separate obligation; arbitrary externally injected RAM is outside this proof.
"""
from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parents[1]
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def byte(a):return b[a-0x4000]
def word(a):return byte(a)*256+byte(a+1)
def guard(a,h):assert b[a-0x4000:a-0x4000+len(bytes.fromhex(h))]==bytes.fromhex(h)
# Producer applies all12 controller bits, one per update; C is second.
guard(0xefb4,'c6 0c e7 08 6c 02')
guard(0xef51,'bd ef 7c bd ef ed bd f2 69')
guard(0xf2e4,'c6 80 e7 50 4f 5f ed 51')
masks=[word(word(0xfcd9+2*i))for i in range(87)]
last=max(i for i,v in enumerate(masks)if v&2)
assert last==67 and not any(v&2 for v in masks[last+1:])
rows=[tuple(byte(0x6485+i*7+j)for j in range(3))for i in range(26)]
assert rows[-1][1]==2 and all(v[1]<2 for v in rows[:-1])
# The first section starts with screens-1; next_stop is handed off at boundary.
finish_screen=sum(v[0]for v in rows)-1
assert finish_screen==86
# Even allow an entire13-update producer pass at the first disabled screen.
disable_screen=last+1
pixel_gap=(finish_screen-disable_screen)*256
# Source scroll advances141/256 pixel per actor update. Stops only lengthen gap.
minimum_updates=(pixel_gap*256)//141-13
max_delay=max(byte(0xf2f8+2*i+1)for i in range(48))
p=word(0xbdaf);durations=[]
while byte(p)<128:
 durations.append(byte(p+1)+1);p+=4
assert p==0xbe81 and byte(p)==255
# Include init and terminal updates; intentionally conservative by a few ticks.
movement_bound=max_delay+sum(durations)+3
# A hit on the final moving update can extend cleanup by at most35 updates.
cleanup_bound=movement_bound+35
assert minimum_updates>cleanup_bound
report={'last_C_enabled_screen':last,'first_disabled_screen':disable_screen,
 'finish_stop_screen':finish_screen,'minimum_actor_updates_after_disable':minimum_updates,
 'maximum_birth_delay':max_delay,'script_duration_bound':sum(durations),
 'moving_actor_lifetime_bound':movement_bound,'including_late_hit_cleanup':cleanup_bound,
 'scope':'Normal uninterrupted source course after startup9C expires. C movement ends before stop2; converted bonus actors may persist but do not read movement vectors. Not a whole-game boot/reentry trace.'}
o=r/'out/parity-audit/c-finish-gap';o.mkdir(parents=True,exist_ok=True)
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

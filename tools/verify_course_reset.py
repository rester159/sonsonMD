#!/usr/bin/env python3
"""Verify session retention in the production course-reset wrapper.
The underlying entity_reset is replaced with a destructive stub: this proves
wrapper retention/call-through, not all arcade actor-reset semantics.
"""
from pathlib import Path
import json,subprocess
r=Path(__file__).resolve().parents[1]
o=r/'out/parity-audit/course-reset';o.mkdir(parents=True,exist_ok=True)
# Bind the counter contract to the inspected revision and the two stores.
import hashlib
rom=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
for address in (0x90cf,0x9108):
 assert rom[address-0x4000:address-0x4000+4]==bytes.fromhex('c6 02 e7 0c')
s=(r/'src/entity.c').read_text()
a=s.index('void entity_course_restart(');b=s.index('\n/*',a)
template='#include <assert.h>\n#include "arcade_waves.h"\nstatic ArcadeRng arcade_rng;\nstatic u8 wave_clock,wave_clock_high,arcade_bonus_next,arcade_bonus_limit;\nstatic unsigned calls;static u8 actor_active,player_shot_available[2];\nvoid entity_reset(u8 cold){assert(!cold);calls++;actor_active=0;player_shot_available[0]=player_shot_available[1]=2;arcade_rng.seed=0;arcade_rng.last=0;wave_clock=0;wave_clock_high=0;arcade_bonus_next=0;arcade_bonus_limit=255;}\n__RESET_FUNCTION__\nint main(void){unsigned i;for(i=0;i<65536;i++){\n arcade_rng.seed=i;arcade_rng.last=i>>8;wave_clock=i;wave_clock_high=i>>8;\n arcade_bonus_next=i>>8;arcade_bonus_limit=255-(i&255);actor_active=1;\n u8 out[2]={(i&1)?128:0,(i&2)?255:0};u8 two=(i&4)?255:0;\n player_shot_available[0]=i>>8;player_shot_available[1]=255-(i>>8);\n entity_course_restart(two,out);\n assert(player_shot_available[0]==(out[0]?(i>>8):2));\n assert(player_shot_available[1]==((two && !out[1])?2:255-(i>>8)));\n assert(arcade_rng.seed==i && arcade_rng.last==(i>>8));\n assert(wave_clock_high==(i>>8));assert(wave_clock==(i&255) && arcade_bonus_next==(i>>8));\n assert(arcade_bonus_limit==255-(i&255) && actor_active==0 && calls==i+1);\n}return 0;}\n'
(o/'test.c').write_text(template.replace('__RESET_FUNCTION__',s[a:b]))
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-I'+str(r/'inc'),str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'cases':65536,'scope':'Production wrapper with destructive reset stub: retains RNG seed/sample, clock and bonus progression and calls actor reset once; all byte shot counters across player eligibility preserve excluded players and reset eligible players to two. Actual entity pool/source restart parity remains pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

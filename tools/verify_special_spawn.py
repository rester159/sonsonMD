#!/usr/bin/env python3
"""Native semantic spawn events versus independently executed original CPU."""
from pathlib import Path
import ctypes as C
import hashlib,json,subprocess
r=Path(__file__).resolve().parents[1]
o=r/'out/parity-audit/special-spawn';o.mkdir(exist_ok=True)
(o/'wrapper.c').write_text('''#include "arcade_special_spawn.h"
void probe(unsigned char *v,unsigned char gates){
 ArcadeSpecialSpawn s={v[0],v[1],v[4],v[5]};ArcadeSpecialSpawnEvent e={0};
 arcade_special_spawn_step(&s,gates&1,(gates>>1)&1,(gates>>2)&1,&e);
 v[0]=s.active;v[1]=s.phase;v[5]=s.row;
 if(e.write){v[40+e.row]=e.codes[0];v[41+e.row]=e.codes[1];
 v[72+e.row]=e.attributes[0];v[73+e.row]=e.attributes[1];}
 if(e.spawn){v[8]=1;v[9]=v[10]=0;v[36]=s.row;}
}
''')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(o/'wrapper.c'),str(r/'src/arcade_special_spawn.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'));lib.probe.argtypes=[C.POINTER(C.c_uint8),C.c_uint8]
trace=r/'out/parity-audit/special-spawn.jsonl'
rows=[json.loads(s)for s in trace.read_text().splitlines()];assert len(rows)==768
for row in rows:
 d=bytearray([row['active'],row['phase'],0,0,row['placement'],row['placement']>>3,0xb1,0x8a])
 b=bytearray(32);b[0:4]=bytes([2,3,7,255]);b[28]=99
 before=d+b+bytes([0x55])*65
 v=(C.c_uint8*len(before)).from_buffer_copy(before);lib.probe(v,row['gates'])
 # Include the word crossing the 64-byte strip boundary at row31.
 expected=bytes.fromhex(row['descriptor']+row['object']+row['strip'])
 assert bytes(v)[:len(expected)]==expected,row
report={'cases':len(rows),'bytes_compared':sum(len(bytes.fromhex(x['descriptor']+x['object']+x['strip']))for x in rows),'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'source_sha256':hashlib.sha256((r/'src/arcade_special_spawn.c').read_bytes()).hexdigest(),'scope':'Host semantic event generator versus original B0D1, both slots, six phases, all three boolean gates, inactive/active, four placements. Live scheduling/render binding and target CPU pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

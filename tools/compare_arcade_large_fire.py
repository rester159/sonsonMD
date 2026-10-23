#!/usr/bin/env python3
"""Native firing decision and explicit birth writes vs full original records."""
from pathlib import Path
import csv,ctypes as C,hashlib,json,subprocess,collections
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit';trace=o/'large-fire-oracle.csv';(o/'large-fire-comparison.json').unlink(missing_ok=True)
assert 'LARGE_FIRE_DONE'in(o/'large-fire-source/mame.log').read_text()
source=r/'src/arcade_large_fire.c'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(source),'-o',str(o/'large-fire.dylib')],check=True)
class Player(C.Structure):_fields_=[('x',C.c_uint16),('y',C.c_uint8),('active',C.c_uint8),('kind',C.c_uint8)]
class Birth(C.Structure):_fields_=[('x',C.c_uint16),('y',C.c_uint8),('facing',C.c_uint8),('slot',C.c_uint8)]
lib=C.CDLL(str(o/'large-fire.dylib'));lib.arcade_large_fire.argtypes=[C.c_uint16,C.c_uint8,C.POINTER(C.c_uint8),C.c_uint8,C.POINTER(Player),C.POINTER(C.c_uint8),C.POINTER(Birth)];lib.arcade_large_fire.restype=C.c_uint8
rows=list(csv.DictReader(trace.open()));assert len(rows)==8192
fixtures=[];coverage=collections.Counter()
for row in rows:
 d={k:(v if k.endswith(('before','after'))else int(v))for k,v in row.items()}
 before=bytes.fromhex(d['pool_before']);pool=bytearray(before);parent=bytearray.fromhex(d['parent_before']);pa=bytes.fromhex(d['parent_after']);after=bytes.fromhex(d['pool_after'])
 players=(Player*2)(*[Player(d[f'p{k}x'],d[f'p{k}y'],d[f'p{k}active'],0)for k in range(2)])
 occupied=(C.c_uint8*4)(*[before[k*32]for k in range(4)]);timer=C.c_uint8(d['timer']);birth=Birth(0x1234,0x56,0x78,0x9a)
 fired=lib.arcade_large_fire(d['x'],d['y'],C.byref(timer),d['period'],players,occupied,C.byref(birth));parent[32]=timer.value
 if fired:
  k=birth.slot*32;pool[k:k+4]=bytes((1,0,0,2));pool[k+5:k+7]=birth.x.to_bytes(2,'big');pool[k+8]=birth.y;pool[k+10]=birth.facing
 assert bytes(pool)==after and bytes(parent)==pa,(d,fired)
 # Expected fixture output comes directly from the original pool differences.
 changed=[k for k in range(4)if before[k*32:(k+1)*32]!=after[k*32:(k+1)*32]]
 assert len(changed)<=1
 if changed:
  k=changed[0];rec=after[k*32:(k+1)*32];expected=bytes((1,pa[32]))+rec[5:7]+bytes((rec[8],rec[10],k));coverage[f'slot{k}']+=1
 else:expected=bytes((0,pa[32],0x12,0x34,0x56,0x78,0x9a));coverage['no_birth']+=1
 inp=d['x'].to_bytes(2,'big')+bytes((d['y'],d['timer'],d['period']))
 for k in range(2):inp+=d[f'p{k}x'].to_bytes(2,'big')+bytes((d[f'p{k}y'],d[f'p{k}active']))
 inp+=bytes(occupied);fixtures.append(inp.hex()+','+expected.hex())
(o/'large-fire-target.csv').write_text('\n'.join(fixtures)+'\n')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
program=hashlib.sha256(b''.join((r/'arcade'/n).read_bytes()for n in('ss.01e','ss.02e','ss.03e'))).hexdigest();assert program=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
d={'program_sha256':program,'trace_sha256':sha(trace),'native_sha256':sha(source),'target_sha256':sha(o/'large-fire-target.csv'),'cases':len(rows),'record_bytes_compared':len(rows)*176,'coverage':dict(coverage),'scope':'Original D74A vs native decision and explicit partial-write adapter. Four shared child slots, all occupancy masks; no child AI, renderer or actor integration.'}
(o/'large-fire-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))

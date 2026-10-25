#!/usr/bin/env python3
"""Original CPU selection/steering versus native C, including conditional RNG."""
from pathlib import Path
import csv,ctypes as C,hashlib,json,subprocess,collections
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit';trace=o/'large-steering-oracle.csv'
(o/'large-steering-comparison.json').unlink(missing_ok=True)
assert 'LARGE_STEERING_DONE'in(o/'large-steering-source/mame.log').read_text()
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_large.c'),'-o',str(o/'large-steering.dylib')],check=True)
class Player(C.Structure):_fields_=[('x',C.c_uint16),('y',C.c_uint8),('active',C.c_uint8),('kind',C.c_uint8)]
class World(C.Structure):_fields_=[('camera',C.c_uint16),('clock',C.c_uint8),('stage',C.c_uint8),('tracking',C.c_uint8),('stopped',C.c_uint8),('budget',C.c_uint8),('purge',C.c_uint8),('players',Player*2)]
lib=C.CDLL(str(o/'large-steering.dylib'));lib.arcade_large_target.argtypes=[C.POINTER(World),C.POINTER(C.c_uint8)];lib.arcade_large_target.restype=C.c_uint8
lib.arcade_large_steer.argtypes=[C.c_uint8]*5+[C.POINTER(C.c_uint16),C.POINTER(C.c_uint8)];lib.arcade_large_steer.restype=C.c_uint8
rows=[{k:int(v)for k,v in d.items()}for d in csv.DictReader(trace.open())];assert len(rows)==8192
coverage=collections.Counter()
for d in rows:
 w=World();w.tracking=d['tracking']
 for k in range(2):w.players[k]=Player(d[f'x{k}'],d[f'y{k}'],d[f'a{k}'],d[f'k{k}'])
 valid=C.c_uint8(123);target=lib.arcade_large_target(C.byref(w),C.byref(valid))
 seed=C.c_uint16(d['seed']);sample=C.c_uint8(d['sample'])
 direction=lib.arcade_large_steer(d['y'],d[f'y{target}'],d['hp'],d['previous'],d['clock'],C.byref(seed),C.byref(sample))
 got=(target,valid.value,direction,seed.value,sample.value);expected=tuple(d[k]for k in('target','eligible','direction','after_seed','after_sample'))
 assert got==expected,(d,got,expected)
 desired=4 if (d['y']+12)&255<d[f'y{target}']else 2
 gate=d['hp']==1 and desired!=d['previous']and not d['clock']&32
 coverage['rng_used'if gate else'rng_retained']+=1
 coverage[f'target{target}_eligible{valid.value}']+=1
 if gate:coverage['random_candidate_retained_old_direction'if ((sample.value&1)*2+1)==d['previous']else'random_candidate_selected']+=1
sha=lambda b:hashlib.sha256(b).hexdigest()
program=sha(b''.join((r/'arcade'/n).read_bytes()for n in('ss.01e','ss.02e','ss.03e')))
assert program=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
report={'program_sha256':program,'trace_sha256':sha(trace.read_bytes()),'native_sha256':sha((r/'src/arcade_large.c').read_bytes()),'cases':len(rows),'field_checks':len(rows)*5,'coverage':dict(coverage),'scope':'Original AD53 and D6F3/95F4 vs native C; component only, not gameplay integration or whole-game RNG ownership.'}
(o/'large-steering-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

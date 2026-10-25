#!/usr/bin/env python3
"""Original three-slot controller vs continuous native controller/actor state."""
# Import also reruns the full ordinary actor regression after initializer extraction.
import compare_arcade_large_actor as a
import csv,ctypes as C,hashlib,json,subprocess
r=a.r;o=a.o;trace=o/'large-wave-oracle.csv';(o/'large-wave-comparison.json').unlink(missing_ok=True)
assert 'LARGE_WAVE_DONE'in(o/'large-wave-source/mame.log').read_text()
sources=a.sources+[r/'src/arcade_large_wave.c']
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),*map(str,sources),'-o',str(o/'large-wave.dylib')],check=True)
class Control(C.Structure):_fields_=[('auxiliary',C.c_uint16)]+[(k,C.c_uint8)for k in('active','state','phase','wait','parameter')]
lib=C.CDLL(str(o/'large-wave.dylib'));lib.arcade_large_wave_step.argtypes=[C.POINTER(Control),C.POINTER(a.Actor),C.c_uint16,C.c_uint8];lib.arcade_large_wave_step.restype=C.c_uint8
rows=list(csv.DictReader(trace.open()));assert len(rows)==3072
fixtures=[];births=0
def control(b):
 ptr=int.from_bytes(b[4:6],'big');assert ptr in(0xe48,0xe78,0xea8,0xed8)
 return ((ptr-0xe48)//48).to_bytes(2,'big')+bytes(b[k]for k in(0,2,3,6,14))
def dump(c):return c.auxiliary.to_bytes(2,'big')+bytes(getattr(c,k)for k in('active','state','phase','wait','parameter'))
for d in rows:
 b=bytes.fromhex(d['control_before']);e=bytes.fromhex(d['control_after']);actors=bytes.fromhex(d['actors_before']);after=bytes.fromhex(d['actors_after'])
 v=control(b);before=[a.convert(actors[k*48:(k+1)*48])for k in range(3)];expected=[a.convert(after[k*48:(k+1)*48])for k in range(3)]
 if int(d['tick'])==0:c=Control(int.from_bytes(v[:2],'big'),*v[2:]);pool=(a.Actor*3)(*[a.load(s)for s in before])
 else:assert dump(c)==v and all(a.dump(pool[k])==before[k]for k in range(3)),('continuity',d)
 born=lib.arcade_large_wave_step(C.byref(c),pool,int(d['camera']),int(d['difficulty']))
 assert dump(c)==control(e),(d,'control')
 assert all(a.dump(pool[k])==expected[k]for k in range(3)),(d,'actors')
 mask=sum(1<<k for k in range(3)if actors[k*48]==0 and after[k*48]!=0);assert born==mask
 births+=int(bool(born));assert all(b[k]==e[k]for k in range(16)if k not in(0,2,3,4,5,6))
 for k in range(3):
  b0=actors[k*48:(k+1)*48];e0=after[k*48:(k+1)*48]
  allowed={0,1,2,3,5,6,8,10,13,14,15,*range(16,25),26,27,29,32,33}
  assert all(b0[j]==e0[j]for j in range(48)if j not in allowed)
 inp=v+b''.join(before)+int(d['camera']).to_bytes(2,'big')+bytes([int(d['difficulty'])]);out=control(e)+b''.join(expected)+bytes([born]);fixtures.append(inp.hex()+','+out.hex())
(o/'large-wave-target.csv').write_text('\n'.join(fixtures)+'\n')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
d={'program_sha256':a.program,'trace_sha256':sha(trace),'source_sha256':{p.name:sha(p)for p in sources},'target_sha256':sha(o/'large-wave-target.csv'),'cases':len(rows),'trajectories':512,'births':births,'scope':'Continuous FAFA controller, three retained actors, all occupancy masks, disabled/active, four variants,difficulties,camera wrap. Native init called without D55B drift. Not gameplay integration.'}
(o/'large-wave-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))

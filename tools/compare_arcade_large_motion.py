#!/usr/bin/env python3
"""Independent semantic movement model vs full original D55B trajectories.

Research reference only: animation, firing, collision and low-HP RNG are excluded.
No source CPU is emulated; source tables are decoded as fixed native vectors.
"""
from pathlib import Path
import collections,csv,hashlib,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
rom=(r/'arcade/ss.03e').read_bytes()
def data(a,n):return rom[a-0xc000:a-0xc000+n]
def word(a):return int.from_bytes(data(a,2),'big')
entry=[[tuple((int.from_bytes(data(word(0xdb4b+s*2)+d*5,3),'big',signed=True),int.from_bytes(data(word(0xdb4b+s*2)+d*5+3,2),'big',signed=True)))for d in range(5)]for s in range(3)]
retreat=[[tuple((int.from_bytes(data(word(0xdb9c+s*2)+d*5,3),'big',signed=True),int.from_bytes(data(word(0xdb9c+s*2)+d*5+3,2),'big',signed=True)))for d in range(5)]for s in range(3)]
def step(b,d):
 a=bytearray(b);major,phase,sub=a[1],a[2],a[3];clock=int(d['clock']);difficulty=int(d['difficulty']);camera=int(d['camera'])
 def move(exit=False):
  dx,dy=(retreat if exit else entry)[sub][a[10]]
  xx=(int.from_bytes(a[5:8],'big')+dx)&0xffffff;yy=(int.from_bytes(a[8:10],'big')+dy)&65535
  a[5:8]=xx.to_bytes(3,'big');a[8:10]=yy.to_bytes(2,'big')
 def target():
  return 1 if int(d['players'])==2 and int(d['p2x'])>int(d['p1x']) else 0
 def steer(k):return 4 if ((a[8]+12)&255)<int(d['p%dy'%(k+1)])else 2
 def edge():
  if (int.from_bytes(a[5:7],'big')-camera)&255>=232:a[1:3]=bytes((2,4))
 if not a[0]:return a
 if major==0:
  if phase==0:
   a[24]=7;a[29]=16;a[10]=a[26]=3;a[2]+=1
  elif phase==1:
   a[29]=(a[29]-1)&255
   if not a[29]:a[10]=a[26]=3;a[2]+=1
  elif phase==2:
   index=((difficulty&56)>>1)+((difficulty&56)>>2)+sub
   threshold=word(0xd636+index) # Source adds subtype as a BYTE offset, without scaling.
   if ((int.from_bytes(a[5:7],'big')-camera)&65535)<=threshold:
    a[28]=data(0xd67c+((difficulty&48)>>3)+((difficulty&48)>>4)+sub,1)[0]
    a[1]+=1;a[2]=0
   else:move()
  else:raise AssertionError(('unhandled entry',phase))
 elif major==1:
  if phase==0:
   assert a[24]!=1,'low-HP RNG requires separate source contract'
   k=target();a[30:32]=(0x490+k*48).to_bytes(2,'big');a[10]=steer(k)
   a[26]=3 if int.from_bytes(a[5:7],'big')>int(d['p%dx'%(k+1)])else 1
   a[29]=48;a[2]+=1
  move();a[29]=(a[29]-1)&255
  if not a[29]:a[2]=0
  if clock&63==0:
   a[28]=(a[28]-1)&255
   if not a[28]:a[1:3]=bytes((3,0))
  edge()
 elif major==2:
  if phase==4:a[29]=16;a[2]=5
  elif phase==5:
   a[29]=(a[29]-1)&255
   if not a[29]:a[2]=2
  elif phase==2:a[0:3]=bytes(3)
  else:raise AssertionError(('unhandled death',phase))
 elif major==3:
  if phase==0:a[10]=3;a[2]=1
  a[10]=steer(target());move(True);edge()
 else:raise AssertionError(('unhandled major',major))
 if int(d['scroll']):a[5:8]=((int.from_bytes(a[5:8],'big')+141)&0xffffff).to_bytes(3,'big')
 return a
trace=o/'large-motion-oracle.csv';assert 'LARGE_MOTION_DONE'in(o/'large-motion-source/mame.log').read_text()
rows=list(csv.DictReader(trace.open()));assert len(rows)==36864
fields=[0,1,2,3,5,6,7,8,9,10,24,26,28,29,30,31]
coverage=collections.Counter();births=0
for d in rows:
 b=bytes.fromhex(d['before']);a=bytes.fromhex(d['after']);got=step(b,d)
 for k in fields:assert got[k]==a[k],(d['case'],d['tick'],k,got[k],a[k])
 coverage[f'{b[1]}/{b[2]}']+=1
 children=bytes.fromhex(d['children']);births+=sum(children[k]!=0 for k in range(0,128,32))
sha=lambda b:hashlib.sha256(b).hexdigest()
program=sha(b''.join((r/'arcade'/n).read_bytes()for n in('ss.01e','ss.02e','ss.03e')))
assert program=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
report={'program_sha256':program,'trace_sha256':sha(trace.read_bytes()),'reference_sha256':sha(Path(__file__).read_bytes()),'updates':len(rows),'field_checks':len(rows)*len(fields),'input_state_coverage':dict(coverage),'recorded_projectile_births':births,'entry_vectors':entry,'retreat_vectors':retreat,'scope':'Research movement reference, scripted camera controlled trajectory. Art, firing, rewards and low-HP RNG excluded. Production movement unchanged. Major3 retirement not covered unless listed.'}
(o/'large-motion-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

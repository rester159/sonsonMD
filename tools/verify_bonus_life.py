#!/usr/bin/env python3
"""Source-pinned FC2C bonus-life state machine, reachable phases 0 and 1."""
from pathlib import Path
import ctypes as C,hashlib,json,random,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/bonus-life';o.mkdir(parents=True,exist_ok=True)
rom=(r/'arcade/ss.01e').read_bytes();code=(r/'arcade/ss.03e').read_bytes()
assert code[0x3c77:0x3c92].hex()=='9ec88d1724026c3f39d655c50127f99ec82008d6552af19ec82000'
assert code[0x3c92:0x3cd9].hex()=='eca410a384223da6225f10a30222356cc434709ecacc0c00eb14bd9604bd640b3570de563344e63f585833c5a622ab4219a722a621a94119a721a6a4a9c419a7a41a01391cfe39'
assert code[0x3c2c:0x3c3a].hex()=='d622271fd624c5072619c5082616'
tables=[]
for dip in range(4):
 ptr=int.from_bytes(rom[0x1fe+dip*2:0x200+dip*2],'big')-0x4000
 tables.append(rom[ptr:ptr+13])
assert [t.hex() for t in tables]==['00000200000000000000000000','00000300000000000000000000','81000200000006000000100000','81000300000006000000120000']
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_bonus_life.c'),'-o',str(o/'test.dylib')],check=True)
class State(C.Structure):_fields_=[('threshold',C.c_uint32),('phase',C.c_uint8)]
l=C.CDLL(str(o/'test.dylib'));l.arcade_bonus_life_step.argtypes=[C.POINTER(State),*([C.c_uint8]*5),C.c_uint32];l.arcade_bonus_life_step.restype=C.c_uint8
l.arcade_bonus_life_begin.argtypes=[C.POINTER(State),C.c_uint8]
def bcd(n,bytes=4):return int(str(n).zfill(bytes*2),16).to_bytes(bytes,'big')
def oracle(threshold,phase,dip,clock,owner,live,out,score):
 t=tables[dip];s=bcd(score)
 if not live or out or clock&7 or (clock>>3)&1!=owner or s[0] or (phase and not(t[0]&1)):return 0,threshold,phase
 y=threshold.to_bytes(3,'big')
 if y[:2]>s[:2] or bytes([y[2],0])>s[2:]:return 0,threshold,phase
 add=t[5+phase*4:8+phase*4];result=list(y);carry=0
 for k in (2,1,0):
  total=(y[k]>>4)*10+(y[k]&15)+(add[k]>>4)*10+(add[k]&15)+carry
  carry=total>=100;total%=100;result[k]=(total//10)*16+total%10
 return 1,int.from_bytes(bytes(result),'big'),1
rng=random.Random(991);cases=0
for dip in range(4):
 s=State();l.arcade_bonus_life_begin(C.byref(s),dip);assert s.threshold==int.from_bytes(tables[dip][1:4],'big') and s.phase==0
 for n in range(30000):
  phase=n&1;threshold=int.from_bytes(bcd(rng.randrange(1000000),3),'big');score=rng.randrange(100000000)
  if n%2==0:score=rng.randrange(1000000)
  clock=rng.randrange(32)*8;owner=rng.randrange(2);live=int(n%17!=0);out=int(n%19==0)
  if n%7==0:clock|=1
  expected=oracle(threshold,phase,dip,clock,owner,live,out,score);s=State(threshold,phase)
  actual=l.arcade_bonus_life_step(C.byref(s),dip,clock,owner,live,out,score)
  assert (actual,s.threshold,s.phase)==expected,(dip,score,threshold,expected,actual,s.threshold)
  cases+=1
 # Ascending exact threshold scores demonstrate recurring awards beyond three.
 s=State();l.arcade_bonus_life_begin(C.byref(s),dip);awards=[]
 for score in range(0,1000000,1000):
  if l.arcade_bonus_life_step(C.byref(s),dip,0,0,1,0,score):awards.append(score)
 expected=([20000] if dip==0 else [30000] if dip==1 else [20000,80000]+list(range(180000,1000000,100000)) if dip==2 else [30000,90000]+list(range(210000,1000000,120000)))
 assert awards==expected,(dip,awards)
report={'cases':cases,'tables':[t.hex() for t in tables],'rom_hashes':[hashlib.sha256(x).hexdigest() for x in (rom,code)],'scope':'Native routine vs independent byte-oriented oracle pinned to FC2C/FC77/FCA1 and DIP tables. Reachable phases 0/1, wrapped BCD addition, source split-word comparison, player/clock/live/out gates, recurring awards. Host execution; integration and original-CPU runtime comparison pending.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

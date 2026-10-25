#!/usr/bin/env python3
"""Pin ranking source contracts and check native insertion/metadata."""
import ctypes as C
import hashlib
import json
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[1]
out=root/'out/parity-audit/ranking'
out.mkdir(parents=True,exist_ok=True)
rom=b''.join((root/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def require(addr,data):assert rom[addr-0x4000:addr-0x4000+len(bytes.fromhex(data))]==bytes.fromhex(data)
require(0x9cbf,'ec8410a3a42509220aec0210a32224034f200286ff39')
require(0x9c6d,'f60373f10374260bc10427047c0373397f037039')
require(0x9c63,'c601f7038ec604d70839')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib',
 '-I'+str(root/'inc'),str(root/'src/arcade_ranking.c'),'-o',str(out/'ranking.dylib')],check=True)
lib=C.CDLL(str(out/'ranking.dylib'))
class Table(C.Structure):
    _fields_=[('individual',(C.c_uint8*8)*5),('total',(C.c_uint8*12)*5)]
class Result(C.Structure):
    _fields_=[('qualified',C.c_uint8*3),('row',C.c_uint8*3)]
Scores=(C.c_uint8*4)*3
lib.arcade_ranking_init.argtypes=[C.POINTER(Table)]
lib.arcade_ranking_submit.argtypes=[C.POINTER(Table),C.POINTER(C.c_uint8*4),C.c_uint8,C.POINTER(Result)]
s=Table();lib.arcade_ranking_init(C.byref(s))
assert bytes(s)==rom[0x1e6:0x1ee]*5+rom[0x1ee:0x1fa]*5
def score(n):return bytes.fromhex(f'{n:08d}')
def initial(values,width):return [score(v)+bytes([17+i])*(width-4) for i,v in enumerate(values)]
def insert(rows,value):
    # Independent stable descending sort: new row precedes all old ties.
    new=score(value)+bytes([16])*(len(rows[0])-4)
    tagged=[(new,True)]+[(r,False) for r in rows]
    ranked=sorted(tagged,key=lambda v:v[0][:4],reverse=True)
    rank=next(i for i,(_,fresh) in enumerate(ranked) if fresh)
    return [r for r,_ in ranked[:5]],rank
cases=0
for values in ([50,40,30,20,10],[30]*5,[0]*5):
    for two in (0,1):
        for p1 in range(61):
            for p2 in range(61):
                original=initial(values,8);totals=initial(values,12)
                s=Table.from_buffer_copy(b''.join(original+totals))
                scores=Scores(*[(C.c_uint8*4).from_buffer_copy(score(n)) for n in (p1,p2,p1+p2)])
                q=[0,0,0];positions=[0,0,0]
                original,r1=insert(original,p1)
                if r1<5:q[0]=1;positions[0]=r1
                if two:
                    original,r2=insert(original,p2)
                    if r2<5:
                        q[1]=1;positions[1]=r2
                        if positions[0]==r2:
                            if r2==4:q[0]=0
                            else:positions[0]+=1
                    totals,rt=insert(totals,p1+p2)
                    if rt<5:q[2]=1;positions[2]=rt
                result=Result()
                lib.arcade_ranking_submit(C.byref(s),scores,two,C.byref(result))
                assert bytes(s)==b''.join(original+totals),(values,p1,p2,two)
                assert list(result.qualified)==q and list(result.row)==positions
                cases+=1
Names=(C.c_uint8*4)*2
lib.arcade_ranking_names.argtypes=[C.POINTER(Table),C.POINTER(Result),C.POINTER(C.c_uint8*4)]
name_cases=0
for mask in range(8):
    for a in range(5):
        for b in range(5):
            for c in range(5):
                s=Table.from_buffer_copy(bytes(range(100)));expected=bytearray(bytes(s))
                names=Names((C.c_uint8*4)(17,18,19,20),(C.c_uint8*4)(21,22,23,24))
                result=Result((C.c_uint8*3)(mask&1,mask&2,mask&4),(C.c_uint8*3)(a,b,c))
                if mask&1:expected[a*8+4:a*8+8]=bytes(names[0])
                if mask&2:expected[b*8+4:b*8+8]=bytes(names[1])
                if mask&4:expected[40+c*12+4:40+c*12+12]=bytes(names)
                lib.arcade_ranking_names(C.byref(s),C.byref(result),names)
                assert bytes(s)==expected
                name_cases+=1
report={'cases':cases,'table_byte_checks':cases*100,'name_commit_cases':name_cases,'passed':True,
 'scope':'Pinned compare/tie and P1 metadata contracts plus independent stable-sort table oracle; default ROM entries, equal scores, full/empty tables, both player counts and displacement. Original-CPU runtime comparison, name entry and SRAM integration pending.'}
(out/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
